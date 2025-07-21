#include "local.h"

SharedStats *shmptr_stats;
Config *shmptr_config;
FileMetadata *shmptr_metaData;
int shmid_stats, semid_stats, shmid_config, shmid_metaData, semid_metaData;

void cleanup() {
    if (shmdt(shmptr_stats) == -1) {
        // perror("shmdt -- inspector3 -- detach");
    }
    if (shmctl(shmid_stats, IPC_RMID, NULL) == -1) {
        // perror("shmctl -- inspector3 -- remove");
    }
    if (semctl(semid_stats, 0, IPC_RMID) == -1) {
        // perror("semctl -- inspector3 -- remove");
    }
}

// Signal handler for SIGTERM
void sigtermHandler(int sig) {
    if (sig == SIGTERM) {
        cleanup();
        exit(0);
    }
}

void deleteFile(FileMetadata *file);
void initializeIPCS();
void lockSemStats();
void unlockSemStats();
void lockSemMetaData();
void unlockSemMetaData();

int main(int argc, char *argv[]) {
    atexit(cleanup);
    signal(SIGTERM, sigtermHandler);
    signal(SIGINT, SIG_IGN);
    sleep(1);

    initializeIPCS();

    while (1){
        if (shmptr_stats->totalBackedup == 0){  // if no files are backed up yet
            sleep(1);
            continue;
        }

        lockSemMetaData();

        for (int i = 0; i < shmptr_stats->totalGenerated; i++) {
            FileMetadata *file = &shmptr_metaData[i];
            // printf("age of file %s is %f\n", file->fileName, difftime(time(NULL), file->generateTime));

            // check if the file is in STATUS_PROCESSED and is older than a user-defined threshold
            if (file->status == STATUS_BACKEDUP && difftime(time(NULL), file->generateTime) > (double) shmptr_config->backupAge) {
                // move to backup dir
                deleteFile(file);
                break;
            }
        }

        unlockSemMetaData();
        int timeToSleep = randomRange(SLEEP_TIME_MIN, SLEEP_TIME_MAX);
        sleep(timeToSleep);
    }

    return 0;
}

void deleteFile(FileMetadata *file){
    if (remove(file->fileName) == -1) { // delete file
        perror("Error deleting file");
    } else {
        printf("Inspector3: Deleted file %s\n", file->fileName);
    }

    file->status = STATUS_DELETED;

    lockSemStats();
    shmptr_stats->totalDeleted++;
    unlockSemStats();
}

void initializeIPCS(){
    key_t key = ftok("./", 'S');
    // create shared memory for files meta data
    if ((shmid_stats = shmget((int) key, sizeof(SharedStats), 0777)) != -1) {
        // attach shared memory
        if ((shmptr_stats = (SharedStats *) shmat(shmid_stats, 0, 0)) == (SharedStats *) -1) {
            perror("shmptr -- inspector3 -- attach");
            exit(1);
        }
    } else {
        perror("shmid -- inspector3 -- creation");
        exit(2);
    }

    if ((semid_stats = semget(key, 1, 0777)) == -1) {
        perror("semget -- inspector3 -- access");
        exit(1);
    }

    key = ftok("./", 'C');
    // create shared memory for config data
    if ((shmid_config = shmget((int) key, sizeof(Config), IPC_CREAT | 0777)) != -1) {
        // attach shared memory
        if ((shmptr_config = (Config *) shmat(shmid_config, 0, 0)) == (Config *) -1) {
            perror("shmptr -- inspector3 -- attach");
            exit(1);
        }
    } else {
        perror("shmid -- inspector3 -- creation");
        exit(2);
    }

    key = ftok("./", 'G');
    // create shared memory for files meta data
    if ((shmid_metaData = shmget((int) key, sizeof(FileMetadata), 0777)) != -1) {
        // attach shared memory
        if ((shmptr_metaData = (FileMetadata *) shmat(shmid_metaData, 0, 0)) == (FileMetadata *) -1) {
            perror("shmptr -- inspector3 -- attach");
            exit(1);
        }
    } else {
        perror("shmid -- inspector3 -- creation");
        exit(2);
    }

    // create a semaphore
    if ((semid_metaData = semget((int) key, 1, 0777)) == -1) {
        perror("semid -- inspector3 -- initialization");
        exit(3);
    }
}

void lockSemStats(){
    if(semop(semid_stats, &sem_lock, 1) == -1){	//lock semaphore to read data
        perror("semop -- inspector3 -- lock");//handle error locking semaphore
        exit(5);
    }
}

void unlockSemStats(){
    if(semop(semid_stats, &sem_unlock, 1) == -1){	//unlock semaphore
        perror("semop -- inspector3 -- unlock");//handle error unlocking semaphore
        exit(5);
    }
}

void lockSemMetaData(){
    if(semop(semid_metaData, &sem_lock, 1) == -1){	//lock semaphore to edit data
        perror("semop -- inspector3 -- lock");//handle error locking semaphore
        exit(5);
    }
}

void unlockSemMetaData(){
    if(semop(semid_metaData, &sem_unlock, 1) == -1){	//unlock semaphore
        perror("semop -- inspector3 -- unlock");//handle error unlocking semaphore
        exit(5);
    }
}
