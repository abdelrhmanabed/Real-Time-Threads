#include "local.h"


int msgid_generated, shmid_stats, semid_stats, shmid_metaData, semid_metaData, msgid_mover;
SharedStats *shmptr_stats;
FileMetadata *shmptr_metaData;

// clean up resources on exit
void cleanup() {
    if (shmdt(shmptr_stats) == -1 || shmdt(shmptr_metaData) == -1) {
        // perror("shmdt -- calculator -- detach");
    }

    if (shmctl(shmid_stats, IPC_RMID, NULL) == -1 || shmctl(shmid_metaData, IPC_RMID, NULL) == -1) {
        // perror("shmctl -- calculator -- remove");
    }

    if (semctl(semid_stats, 0, IPC_RMID) == -1 || semctl(semid_metaData, 0, IPC_RMID) == -1) {
        // perror("semctl -- calculator -- remove");
    }

    if (msgctl(msgid_mover, IPC_RMID, NULL) == -1) {
        // perror("msgid -- calculator -- remove");
    }
}

// Signal handler for SIGTERM
void sigtermHandler(int sig) {
    if (sig == SIGTERM) {
        cleanup();
        exit(0);
    }
}

// Function to calculate column averages and update min and max averages
void calculateColumnAverages(FileMetadata *metadata);
void updateMinMaxAverages(FileMetadata *metadata);

void initializeIPCS();
void lockSemStats();
void unlockSemStats();
void lockSemMetaData();
void unlockSemMetaData();

int main() {
    atexit(cleanup);
    signal(SIGTERM, sigtermHandler);
    signal(SIGINT, SIG_IGN);
    sleep(1);

    initializeIPCS();

    while (1) {
        if (shmptr_stats->totalGenerated == 0)
            continue;

        lockSemMetaData();

        int foundFile = 0;

        for (int i = 0; i < shmptr_stats->totalGenerated; i++) { // search for a STATUS_READY file
            FileMetadata *file = &shmptr_metaData[i];

            if (file->status == STATUS_READY) {
                file->status = STATUS_PROCESSING; // file being processed
                foundFile = 1;

                unlockSemMetaData();

                calculateColumnAverages(file); // calculate averages for columns

                FileMessage msg;
                msg.msg_type = 1;
                msg.fileNumber = file->serial_number;

                if (msgsnd(msgid_mover, &msg, sizeof(FileMessage) - sizeof(long), 0) == -1) {
                    perror("msgid -- calculator -- send");
                    exit(1);
                }
                break;
            }
        }

        if (!foundFile) {
            unlockSemMetaData();
        }

        int timeToSleep = randomRange(SLEEP_TIME_MIN, SLEEP_TIME_MAX);
        sleep(timeToSleep);
    }

    return 0;
}

void calculateColumnAverages(FileMetadata *metadata) {
    char fileName[256];
    strcpy(fileName, metadata->fileName);

    FILE *file = fopen(fileName, "r");
    if (!file) {
        perror("Error opening file");
        return;
    }

    int numCols = metadata->cols;
    double *sums = (double *)calloc(numCols, sizeof(double)); // initialize sums to 0
    int *counts = (int *)calloc(numCols, sizeof(int)); // initialize counts to 0
    char line[1024];

    while (fgets(line, sizeof(line), file)) {
        int col = 0;
        char *ptr = line; // pointer to beginning of line
        char token[256];  // buffer

        while (*ptr != '\0' && col < metadata->cols) {
            char *start = ptr; // start for each column

            while (*ptr != ',' && *ptr != '\n' && *ptr != '\0') {
                ptr++;
            }

            size_t len = ptr - start;
            strncpy(token, start, len);
            token[len] = '\0';

            if (*ptr == ',') {
                ptr++; // skip the delimiter
            }

            if (len > 0) { // check if the token is not empty
                double value = atof(token);
                sums[col] += value;
                counts[col]++;
            }
            col++;
        }
    }


    fclose(file);

    for (int i = 0; i < numCols; i++) {
        if (counts[i] > 0) {
            metadata->averages[i] = sums[i] / counts[i];
        } else {
            metadata->averages[i] = 0.0; // No data for this column
        }
    }

    // update the stats
    lockSemStats();

    updateMinMaxAverages(metadata);
    shmptr_stats->totalProcessed++;
    unlockSemStats();

    printf("Processed file: %s\n", metadata->fileName);
    // for (int i = 0; i < metadata->cols; i++) {
    //     // printf("counts %d = %d", i, counts[i]);
    //     if (counts[i] > 0) {
    //         printf("Column %d Average: %.2f\n", i, metadata->averages[i]);
    //     } else {
    //         printf("Column %d Average: No data\n", i);
    //     }
    // }

    free(sums);
    free(counts);
}

void updateMinMaxAverages(FileMetadata *metadata) {

    for (int i = 0; i < metadata->cols; i++) {
        double avg = metadata->averages[i];
        char fileName[256];
        snprintf(fileName, sizeof(fileName), "%d.csv", metadata->serial_number);

        if (shmptr_stats->totalProcessed == 0 || avg < shmptr_stats->minAverage) { // update minimum average
            shmptr_stats->minAverage = avg;
            shmptr_stats->minAverageCol = i;
            strncpy(shmptr_stats->minAverageFile, fileName, 255);
            shmptr_stats->minAverageFile[255] = '\0';
        }

        if (shmptr_stats->totalProcessed == 0 || avg > shmptr_stats->maxAverage) { // update maximum average
            shmptr_stats->maxAverage = avg;
            shmptr_stats->maxAverageCol = i;
            strncpy(shmptr_stats->maxAverageFile, fileName, 255);
            shmptr_stats->maxAverageFile[255] = '\0';
        }
    }
}

void initializeIPCS(){
    key_t key = ftok("./", 'S');
    if ((shmid_stats = shmget(key, sizeof(SharedStats), 0777)) == -1) {
        perror("shmget -- calculator -- access");
        exit(1);
    }

    if ((shmptr_stats = (SharedStats *)shmat(shmid_stats, 0, 0)) == (SharedStats *)-1) {
        perror("shmat -- calculator -- attach");
        exit(1);
    }

    if ((semid_stats = semget(key, 1, 0777)) == -1) {
        perror("semget -- calculator -- access");
        exit(1);
    }

    key = ftok("./", 'G');
    // create shared memory for files meta data
    if ((shmid_metaData = shmget((int) key, sizeof(FileMetadata), 0777)) != -1) {
        // attach shared memory
        if ((shmptr_metaData = (FileMetadata *) shmat(shmid_metaData, 0, 0)) == (FileMetadata *) -1) {
            perror("shmptr -- calculator -- attach");
            exit(1);
        }
    } else {
        perror("shmid -- calculator -- creation");
        exit(2);
    }

    // create a semaphore
    if ((semid_metaData = semget((int) key, 1, 0777)) == -1) {
        perror("semid -- calculator -- initialization");
        exit(3);
    }

    // message queue for calculator to send processed file name to mover
    key = ftok("./", 'C');
    if ((msgid_mover = msgget(key, 0777)) == -1) {
        perror("msgid -- calculator -- creation");
        exit(1);
    }
}

void lockSemStats(){
    if(semop(semid_stats, &sem_lock, 1) == -1){	//lock semaphore to read data
        perror("semop -- calculator -- lock");//handle error locking semaphore
        exit(5);
    }
}

void unlockSemStats(){
    if(semop(semid_stats, &sem_unlock, 1) == -1){	//unlock semaphore
        perror("semop -- calculator -- unlock");//handle error unlocking semaphore
        exit(5);
    }
}

void lockSemMetaData(){
    if(semop(semid_metaData, &sem_lock, 1) == -1){	//lock semaphore to edit data
        perror("semop -- calculator -- lock");//handle error locking semaphore
        exit(5);
    }
}

void unlockSemMetaData(){
    if(semop(semid_metaData, &sem_unlock, 1) == -1){	//unlock semaphore
        perror("semop -- calculator -- unlock");//handle error unlocking semaphore
        exit(5);
    }
}
