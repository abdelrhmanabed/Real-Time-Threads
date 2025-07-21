#include "local.h"


SharedStats *shmptr_stats;
Config *shmptr_config;
FileMetadata *shmptr_metaData;
int shmid_stats, semid_stats, shmid_config, shmid_metaData, semid_metaData;

// function to clean up resources on program exit
void cleanup() {
    if (shmdt(shmptr_stats) == -1 || shmdt(shmptr_config) == -1 || shmdt(shmptr_metaData) == -1) {
        // perror("shmdt -- generator -- detach");
    }

    if (shmctl(shmid_stats, IPC_RMID, NULL) == -1 || shmctl(shmid_config, IPC_RMID, NULL) == -1 || shmctl(shmid_metaData, IPC_RMID, NULL) == -1) {
        // perror("shmctl -- generator -- remove");
    }

    if (semctl(semid_stats, 0, IPC_RMID) == -1 || semctl(semid_metaData, 0, IPC_RMID) == -1) {
        // perror("semctl -- generator -- remove");
    }
}

// signal handler for SIGTERM
void sigtermHandler(int sig) {
    if (sig == SIGTERM) {
        // exit the program gracefully
        exit(0);
    }
}

void generateCSVFile(int fileNumber);
void initializeIPCS();
void lockSemStats();
void unlockSemStats();
void lockSemMetaData();
void unlockSemMetaData();

int main(int argc, char *argv[]) {

    atexit(cleanup);
    signal(SIGTERM, sigtermHandler);
    signal(SIGINT, SIG_IGN);

    // ensure the home directory exists
    if (access(HOME_DIR, F_OK) == -1) {
        mkdir(HOME_DIR, 0777);
    }

    initializeIPCS();

    // keep generating files
    while (1) {

        lockSemStats();
        int fileNumber = shmptr_stats->serialNumber++;
        unlockSemStats();

        generateCSVFile(fileNumber);

        lockSemStats();
        shmptr_stats->totalGenerated++;
        unlockSemStats();

        int sleepTime = randomRange(shmptr_config->generateTime[0], shmptr_config->generateTime[1]);
        sleep(sleepTime);
    }

    return 0;
}

// function to create a CSV file
void generateCSVFile(int fileNumber) {
    char filename[256];
    snprintf(filename, 256, "%s/%d.csv", HOME_DIR, fileNumber);

    // Open the file for writing
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Error opening file for writing");
        return;
    }

    int rows = randomRange(shmptr_config->rows[0], shmptr_config->rows[1]);
    int cols = randomRange(shmptr_config->columns[0], shmptr_config->columns[1]);

    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            // decide whether to leave the value missing based on miss_percentage
            if ((rand() % 100) < shmptr_config->missPercentage) {
                // printf("miss\n");
                fprintf(file, ""); // Missing value
            } else {
                double value = generateRandomDouble(shmptr_config->decimalRange[0], shmptr_config->decimalRange[1]);
                fprintf(file, "%.2f", value);
            }

            if (j < cols - 1) {
                fprintf(file, ",");
            }
        }
        fprintf(file, "\n");
    }

    fclose(file);
    // printf("Generated file: %s\n", filename);

    lockSemMetaData();

    strncpy(shmptr_metaData[fileNumber].fileName, filename, 255);
    shmptr_metaData[fileNumber].fileName[255] = '\0';
    shmptr_metaData[fileNumber].rows = rows;
    shmptr_metaData[fileNumber].cols = cols;
    shmptr_metaData[fileNumber].serial_number = fileNumber;
    shmptr_metaData[fileNumber].status = STATUS_READY;
    shmptr_metaData[fileNumber].generateTime = time(NULL);

    unlockSemMetaData();
}

void initializeIPCS(){
    key_t key = ftok("./", 'C');
    // create shared memory for config data
    if ((shmid_config = shmget((int) key, sizeof(Config), IPC_CREAT | 0777)) != -1) {
        // attach shared memory
        if ((shmptr_config = (Config *) shmat(shmid_config, 0, 0)) == (Config *) -1) {
            perror("shmptr -- generator -- attach");
            exit(1);
        }
    } else {
        perror("shmid -- generator -- creation");
        exit(2);
    }

    key = ftok("./", 'S');
    // create shared memory for sharedStats data
    if ((shmid_stats = shmget(key, sizeof(SharedStats), 0777)) != -1) {
        // attach shared memory
        if ((shmptr_stats = (SharedStats *) shmat(shmid_stats, 0, 0)) == (SharedStats *) -1) {
            perror("shmptr -- generator -- attach");
            exit(1);
        }
    } else {
        perror("shmid -- generator -- creation");
        exit(2);
    }

    // create a semaphore
    if ((semid_stats = semget(key, 1, 0777)) == -1) {
        perror("semid -- generator -- initialization");
        exit(3);
    }

    key = ftok("./", 'G');
    // create shared memory for files meta data
    if ((shmid_metaData = shmget((int) key, sizeof(FileMetadata), 0777)) != -1) {
        // attach shared memory
        if ((shmptr_metaData = (FileMetadata *) shmat(shmid_metaData, 0, 0)) == (FileMetadata *) -1) {
            perror("shmptr -- generator -- attach");
            exit(1);
        }
    } else {
        perror("shmid -- generator -- creation");
        exit(2);
    }

    // create a semaphore
    if ((semid_metaData = semget((int) key, 1, 0777)) == -1) {
        perror("semid -- generator -- initialization");
        exit(3);
    }
}

void lockSemStats(){
    if(semop(semid_stats, &sem_lock, 1) == -1){	//lock semaphore to read data
        perror("semop -- generator -- lock");//handle error locking semaphore
        exit(5);
    }
}

void unlockSemStats(){
    if(semop(semid_stats, &sem_unlock, 1) == -1){	//unlock semaphore
        perror("semop -- generator -- unlock");//handle error unlocking semaphore
        exit(5);
    }
}

void lockSemMetaData(){
    if(semop(semid_metaData, &sem_lock, 1) == -1){	//lock semaphore to edit data
        perror("semop -- generator -- lock");//handle error locking semaphore
        exit(5);
    }
}

void unlockSemMetaData(){
    if(semop(semid_metaData, &sem_unlock, 1) == -1){	//unlock semaphore
        perror("semop -- generator -- unlock");//handle error unlocking semaphore
        exit(5);
    }
}