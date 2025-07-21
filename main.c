#include "local.h"

SharedStats *sharedStats, *shmptr_stats;
Config *config, *shmptr_config;
FileMetadata *metaData, *shmptr_metaData;
int shmid_stats, semid_stats, shmid_config, shmid_metaData, semid_metaData, msgid_mover;

void readData(FILE *f_data);

// function to clean up resources on program exit
void cleanup() {
    free(sharedStats);
    free(config);
    free(metaData);

    if (shmdt(shmptr_stats) == -1 || shmdt(shmptr_config) == -1 || shmdt(shmptr_metaData) == -1) {
        perror("shmdt -- main -- detach");
    }

    if (shmctl(shmid_stats, IPC_RMID, NULL) == -1 || shmctl(shmid_config, IPC_RMID, NULL) == -1 || shmctl(shmid_metaData, IPC_RMID, NULL) == -1) {
        perror("shmctl -- main -- remove");
    }

    if (semctl(semid_stats, 0, IPC_RMID) == -1 || semctl(semid_metaData, 0, IPC_RMID) == -1) {
        perror("semctl -- main -- remove");
    }

    if (msgctl(msgid_mover, IPC_RMID, NULL) == -1) {
        perror("msgid -- calculator -- remove");
    }

    kill(-getpgrp(), SIGTERM);
}

void sigintHandler(int sig){	//if received SIGINT, kill all the children and grandchildren
    if(sig == SIGINT){
        exit(0);
    }
}

void initializeIPCS();
void generateProcesses();
void printMinMaxAverages();

int main(int argc, char *argv[]) {
    atexit(cleanup);
    signal(SIGINT, sigintHandler);	//handle the SIGINT (ctrl+c) with sigintHandler function

    if(argc != 2){
        printf("Error: wrong number of arguments\n");
        exit(1);
    }

    time_t startTime = time(NULL);

    char *dataFile = argv[1];	//file name of the main data
    FILE *f_data = fopen(dataFile, "r");
    if(f_data == NULL){			//handle NULL file error
        printf("Error opening file\n");
        exit(1);
    }

    config = (Config *) malloc(sizeof(Config));
    sharedStats = (SharedStats *) malloc(sizeof(SharedStats));
    metaData = (FileMetadata *) calloc(MAX_FILES, sizeof(FileMetadata));

    init(config, sharedStats);
    readData(f_data);
    fclose(f_data);
    printf("program will run for %d seconds\n", config->runForSeconds);

    initializeIPCS();
    generateProcesses();

    while(1){
        // check if time is up or any threshold is reached
        if (difftime(time(NULL), startTime) >= shmptr_config->runForSeconds) {
            printf("time is up!\n");
            break;
        } else if (shmptr_stats->totalProcessed >= shmptr_config->processedTh) {
            printf("processed threshold reached!\n");
            break;
        } else if (shmptr_stats->totalUnprocessed >= shmptr_config->unprocessedTh) {
            printf("unprocessed threshold reached!\n");
            break;
        } else if (shmptr_stats->totalBackedup >= shmptr_config->backedupTh) {
            printf("backedup threshold reached!\n");
            break;
        } else if (shmptr_stats->totalDeleted >= shmptr_config->deletedTh) {
            printf("deleted threshold reached!\n");
            break;
        }
        sleep(1);
    }

    printMinMaxAverages();
    return 0;
}

void readData(FILE *f_data){    // read the configuration data (user-defined) from the data.txt file
    char line[40], data[20];
    float x, y;
    while(fgets(line, 40, f_data) != NULL){
        sscanf(line, "%[^,],%f,%f", data, &x, &y);
        if(strcmp(data, "generators") == 0)
            config->generators = (int) x;
        else if(strcmp(data, "generateTime") == 0){
            config->generateTime[0] = (int) x;
            config->generateTime[1] = (int) y;
        }
        else if(strcmp(data, "rows") == 0){
            config->rows[0] = (int) x;
            config->rows[1] = (int) y;
        }
        else if(strcmp(data, "columns") == 0){
            config->columns[0] = (int) x;
            config->columns[1] = (int) y;
        }
        else if(strcmp(data, "decimalRange") == 0){
            config->decimalRange[0] = (int) x;
            config->decimalRange[1] = (int) y;
        }
        else if(strcmp(data, "missPercentage") == 0)
            config->missPercentage = (int) x;
        else if(strcmp(data, "calculators") == 0)
            config->calculators = (int) x;
        else if(strcmp(data, "movers") == 0)
            config->movers = (int) x;
        else if(strcmp(data, "inspector1s") == 0)
            config->inspector1s = (int) x;
        else if(strcmp(data, "inspector2s") == 0)
            config->inspector2s = (int) x;
        else if(strcmp(data, "inspector3s") == 0)
            config->inspector3s = (int) x;
        else if(strcmp(data, "unprocessedAge") == 0)
            config->unprocessedAge = (int) x;
        else if(strcmp(data, "processedAge") == 0)
            config->processedAge = (int) x;
        else if(strcmp(data, "backupAge") == 0)
            config->backupAge = (int) x;
        else if(strcmp(data, "processedTh") == 0)
            config->processedTh = (int) x;
        else if(strcmp(data, "unprocessedTh") == 0)
            config->unprocessedTh = (int) x;
        else if(strcmp(data, "backedupTh") == 0)
            config->backedupTh = (int) x;
        else if(strcmp(data, "deletedTh") == 0)
            config->deletedTh = (int) x;
        else if(strcmp(data, "runForMins") == 0)
            config->runForSeconds = (int) (x*60);
        else{
            printf("Error reading file\n");
            exit(1);
        }
    }
}

void initializeIPCS(){
    key_t key = ftok("./", 'C');
    // create shared memory for config data
    if ((shmid_config = shmget((int) key, sizeof(Config), IPC_CREAT | 0777)) != -1) {
        // attach shared memory
        if ((shmptr_config = (Config *) shmat(shmid_config, 0, 0)) == (Config *) -1) {
            perror("shmptr -- main -- attach");
            exit(1);
        }

        // copy config data to shared memory
        if (memcpy(shmptr_config, config, sizeof(Config)) == NULL) {
            perror("shmptr -- main -- copy");
            exit(1);
        }
    } else {
        perror("shmid -- main -- creation");
        exit(2);
    }

    key = ftok("./", 'S');
    // create shared memory for sharedStats data
    if ((shmid_stats = shmget((int) key, sizeof(SharedStats), IPC_CREAT | 0777)) != -1) {
        // attach shared memory
        if ((shmptr_stats = (SharedStats *) shmat(shmid_stats, 0, 0)) == (SharedStats *) -1) {
            perror("shmptr -- main -- attach");
            exit(1);
        }

        // copy sharedStats data to shared memory
        if (memcpy(shmptr_stats, sharedStats, sizeof(SharedStats)) == NULL) {
            perror("shmptr -- main -- copy");
            exit(1);
        }
    } else {
        perror("shmid -- main -- creation");
        exit(2);
    }

    // create a semaphore and initialize its value to 1
    if ((semid_stats = semget((int) key, 1, IPC_CREAT | 0777)) != -1) {
        if (semctl(semid_stats, 0, SETVAL, 1) == -1) {
            perror("semid -- main -- initialization");
            exit(3);
        }
    } else {
        perror("semid -- main -- creation");
        exit(4);
    }

    key = ftok("./", 'G');
    // create shared memory for files meta data
    if ((shmid_metaData = shmget((int) key, sizeof(FileMetadata) * MAX_FILES, IPC_CREAT | 0777)) != -1) {
        // attach shared memory
        if ((shmptr_metaData = (FileMetadata *) shmat(shmid_metaData, 0, 0)) == (FileMetadata *) -1) {
            perror("shmptr -- main -- attach");
            exit(1);
        }

        // copy metaData data to shared memory
        if (memcpy(shmptr_metaData, metaData, sizeof(FileMetadata) * MAX_FILES) == NULL) {
            perror("shmptr -- main -- copy");
            exit(1);
        }
    } else {
        perror("shmid -- main -- creation");
        exit(2);
    }

    // create a semaphore and initialize its value to 1
    if ((semid_metaData = semget((int) key, 1, IPC_CREAT | 0777)) != -1) {
        if (semctl(semid_metaData, 0, SETVAL, 1) == -1) {
            perror("semid -- main -- initialization");
            exit(3);
        }
    } else {
        perror("semid -- main -- creation");
        exit(4);
    }

    // message queue for calculator to send processed file name to mover
    key = ftok("./", 'C');
    if ((msgid_mover = msgget(key, IPC_CREAT | 0777)) == -1) {
        perror("msgid -- main -- creation");
        exit(1);
    }
}

void generateProcesses(){
    for (int i = 0; i < config->generators; i++){
        pid_t pidGenerator = fork();	//fork a generator process
        if (pidGenerator == -1){		//if failed to create a child
            printf("Error creating generator\n");
            exit(1);
        }
        else if (pidGenerator == 0) {	//here executes the child process
            execlp("./generator", "generator", NULL);	//execute a new program
            perror("Error executing generator\n");
            exit(1);
        }
    }

    for (int i = 0; i < config->calculators; i++){
        pid_t pidCalculator = fork();	//fork a calculator process
        if (pidCalculator == -1){		//if failed to create a child
            printf("Error creating calculator\n");
            exit(1);
        }
        else if (pidCalculator == 0) {	//here executes the child process
            execlp("./calculator", "calculator", NULL);	//execute a new program
            perror("Error executing calculator\n");
            exit(1);
        }
    }

    for (int i = 0; i < config->movers; i++){
        pid_t pidMover = fork();	//fork a mover process
        if (pidMover == -1){		//if failed to create a child
            printf("Error creating mover\n");
            exit(1);
        }
        else if (pidMover == 0) {	//here executes the child process
            execlp("./mover", "mover", NULL);	//execute a new program
            perror("Error executing mover\n");
            exit(1);
        }
    }

    for (int i = 0; i < config->inspector1s; i++){
        pid_t pidInspector1 = fork();	//fork a inspector1 process
        if (pidInspector1 == -1){		//if failed to create a child
            printf("Error creating inspector1\n");
            exit(1);
        }
        else if (pidInspector1 == 0) {	//here executes the child process
            execlp("./inspector1", "inspector1", NULL);	//execute a new program
            perror("Error executing inspector1\n");
            exit(1);
        }
    }

    for (int i = 0; i < config->inspector2s; i++){
        pid_t pidInspector2 = fork();	//fork a inspector2 process
        if (pidInspector2 == -1){		//if failed to create a child
            printf("Error creating inspector2\n");
            exit(1);
        }
        else if (pidInspector2 == 0) {	//here executes the child process
            execlp("./inspector2", "inspector2", NULL);	//execute a new program
            perror("Error executing inspector2\n");
            exit(1);
        }
    }

    for (int i = 0; i < config->inspector3s; i++){
        pid_t pidInspector3 = fork();	//fork a inspector3 process
        if (pidInspector3 == -1){		//if failed to create a child
            printf("Error creating inspector3\n");
            exit(1);
        }
        else if (pidInspector3 == 0) {	//here executes the child process
            execlp("./inspector3", "inspector3", NULL);	//execute a
            perror("Error executing inspector3\n");
            exit(1);
        }
    }

    //fork opengl
    pid_t opengl_pid = fork();
    if (opengl_pid == -1){		//if failed to create a child
        printf("Error creating opengl\n");
        exit(1);
    }
    else if (opengl_pid == 0) {
        execlp("./opengl", "opengl", NULL);	//execute opengl
        perror("Error executing opengl\n");
        exit(1);
    }
}

void printMinMaxAverages() {
    printf("Minimum Average:\n");
    printf("  File: %s\n", shmptr_stats->minAverageFile);
    printf("  Column: %d\n", shmptr_stats->minAverageCol);
    printf("  Average: %.2f\n", shmptr_stats->minAverage);

    printf("\nMaximum Average:\n");
    printf("  File: %s\n", shmptr_stats->maxAverageFile);
    printf("  Column: %d\n", shmptr_stats->maxAverageCol);
    printf("  Average: %.2f\n", shmptr_stats->maxAverage);
}
