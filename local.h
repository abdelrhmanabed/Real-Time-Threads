#ifndef __LOCAL_H__
#define __LOCAL_H__


#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>

// sleep time for calculators and movers is not defined, so this would result in none files in the unprocessed directory, i will define values for them
#define SLEEP_TIME_MIN 1
#define SLEEP_TIME_MAX 5

#define HOME_DIR "./home"
#define PROCESSED_DIR "./home/processed"
#define UNPROCESSED_DIR "./home/unprocessed"
#define BACKUP_DIR "./home/backup"

#define MAX_FILES 100
#define MAX_COLS 100
#define STATUS_READY 0      // file ready for processing
#define STATUS_PROCESSING 1 // file being processed
#define STATUS_PROCESSED 2  // file processed
#define STATUS_AGED 3       // file is aged (unprocessed/backedup/deleted)
#define STATUS_BACKEDUP 4   // file is backed up
#define STATUS_DELETED 5    // file is deleted

int randomRange(int min, int max){
    if(min > max){
        printf("Error: min > max, %d %d\n", min, max);
        exit(1);
    }
    else if(min == max){
        return min;
    }
    srand((time(NULL)%11) * 7 + getpid());
    return rand() % (max - min + 1) + min;
}

// Function to generate a random double within a range
double generateRandomDouble(int min, int max) {
    return min + (rand() / (double)RAND_MAX) * (max - min);
}

struct sembuf sem_lock = {0, -1, SEM_UNDO},
              sem_unlock = {0, 1, SEM_UNDO};

typedef struct {
    long msg_type;
    int fileNumber;
} FileMessage;

typedef struct {
    int generators;
    int generateTime[2];
    int rows[2];
    int columns[2];
    int decimalRange[2];
    int missPercentage;
    int calculators;
    int movers;
    int inspector1s;
    int inspector2s;
    int inspector3s;
    int unprocessedAge;
    int processedAge;
    int backupAge;
    int processedTh;        // processed threshold
    int unprocessedTh;      // unprocessed threshold
    int backedupTh;         // backedup threshold
    int deletedTh;          // deleted threshold
    int runForSeconds;
} Config;

typedef struct {
    int serialNumber;
    int totalGenerated;
    int totalProcessed;
    int totalUnprocessed;
    int totalBackedup;
    int totalDeleted;

    double minAverage;
    double maxAverage;
    int minAverageCol;
    int maxAverageCol;
    char minAverageFile[256];
    char maxAverageFile[256];
} SharedStats;

typedef struct {
    char fileName[256];
    int rows;
    int cols;
    int serial_number;
    int status;
    double averages[MAX_COLS];
    time_t generateTime;
} FileMetadata;

void init(Config *config, SharedStats *sharedStats) {
    config->generators = 5;
    config->rows[0] = 10000;
    config->rows[1] = 10000;
    config->columns[0] = 10;
    config->columns[1] = 10;
    config->movers = 10;
    config->inspector1s = 10;       // assumed to be 10
    config->inspector2s = 10;       // assumed to be 10
    config->inspector3s = 10;       // assumed to be 10


    sharedStats->serialNumber = 0;
    sharedStats->totalGenerated = 0;
    sharedStats->totalProcessed = 0;
    sharedStats->totalUnprocessed = 0;
    sharedStats->totalBackedup = 0;
    sharedStats->totalDeleted = 0;
}

#endif
