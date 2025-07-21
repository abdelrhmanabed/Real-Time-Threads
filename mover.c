#include "local.h"

int msgid_generated, shmid_stats, semid_stats, shmid_metaData, semid_metaData, msgid_mover;
SharedStats *shmptr_stats;
FileMetadata *shmptr_metaData;

// clean up resources on exit
void cleanup() {
    // printf("mover with pid %d exiting...\n", getpid());
    if (shmdt(shmptr_stats) == -1 || shmdt(shmptr_metaData) == -1) {
        // perror("shmdt -- mover -- detach");
    }

    if (shmctl(shmid_stats, IPC_RMID, NULL) == -1 || shmctl(shmid_metaData, IPC_RMID, NULL) == -1) {
        // perror("shmctl -- mover -- remove");
    }

    if (semctl(semid_stats, 0, IPC_RMID) == -1 || semctl(semid_metaData, 0, IPC_RMID) == -1) {
        // perror("semctl -- mover -- remove");
    }

    if (msgctl(msgid_mover, IPC_RMID, NULL) == -1) {
        // perror("msgid -- mover -- remove");
    }
}

// Signal handler for SIGTERM
void sigtermHandler(int sig) {
    if (sig == SIGTERM) {
        cleanup();
        exit(0);
    }
}

void initializeIPCS();

int main() {
    atexit(cleanup);
    signal(SIGTERM, sigtermHandler);
    signal(SIGINT, SIG_IGN);

    if (access(PROCESSED_DIR, F_OK) == -1) {
        mkdir(PROCESSED_DIR, 0777);
    }

    initializeIPCS();

    FileMessage msg;

    while (1) {
        // receive a message from the queue
        if (msgrcv(msgid_mover, &msg, sizeof(FileMessage) - sizeof(long), 1, 0) == -1) {
            perror("msgid -- mover -- receive");
            exit(1);
        }

        // move the file to the processed directory
        char fileName[256];
        snprintf(fileName, sizeof(fileName), "./home/processed/%d.csv", msg.fileNumber);

        if (rename(shmptr_metaData[msg.fileNumber].fileName, fileName) == -1) {     // move the file to the processed directory
            perror("Error moving file to processed directory");
        } else {
            printf("Mover: Moved file ./home/%d.csv to %s\n", msg.fileNumber, fileName);
            strcpy(shmptr_metaData[msg.fileNumber].fileName, fileName);
            shmptr_metaData[msg.fileNumber].status = STATUS_PROCESSED;
        }

        int timeToSleep = randomRange(SLEEP_TIME_MIN, SLEEP_TIME_MAX);
        sleep(timeToSleep);
    }

    return 0;
}

void initializeIPCS(){
    key_t key = ftok("./", 'S');
    // create shared memory for files meta data
    if ((shmid_stats = shmget((int) key, sizeof(SharedStats), 0777)) != -1) {
        // attach shared memory
        if ((shmptr_stats = (SharedStats *) shmat(shmid_stats, 0, 0)) == (SharedStats *) -1) {
            perror("shmptr -- mover -- attach");
            exit(1);
        }
    } else {
        perror("shmid -- mover -- creation");
        exit(2);
    }

    if ((semid_stats = semget(key, 1, 0777)) == -1) {
        perror("semget -- mover -- access");
        exit(1);
    }

    key = ftok("./", 'G');
    // create shared memory for files meta data
    if ((shmid_metaData = shmget((int) key, sizeof(FileMetadata), 0777)) != -1) {
        // attach shared memory
        if ((shmptr_metaData = (FileMetadata *) shmat(shmid_metaData, 0, 0)) == (FileMetadata *) -1) {
            perror("shmptr -- mover -- attach");
            exit(1);
        }
    } else {
        perror("shmid -- mover -- creation");
        exit(2);
    }

    // create a semaphore
    if ((semid_metaData = semget((int) key, 1, 0777)) == -1) {
        perror("semid -- mover -- initialization");
        exit(3);
    }

    // message queue for calculator to send processed file name to mover
    key = ftok("./", 'C');
    if ((msgid_mover = msgget(key, 0777)) == -1) {
        perror("msgid -- mover -- creation");
        exit(1);
    }
}
