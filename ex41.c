//
// Created by guest on 6/12/17.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/fcntl.h>
#include <string.h>
#include <sys/sem.h>
#include <pthread.h>
#include <time.h>

#define ARR_SIZE 5
#define READ 0
#define WRITE 1

/*******************************************************************************
* function name : exitWithError
* input : message
* output : -
* explanation : write to stderr the message and exit with code -1
*******************************************************************************/
void exitWithError(char *msg) {
    perror(msg);
    exit(-1);
}

void createKeys(key_t *shared, key_t *read, key_t *write) {
    // creating the key for shared memory
    *shared = ftok("302933833.txt", 'o');
    if (*shared == (key_t) -1) {
        exitWithError("ftok error");
    }

    // creating the key for read semaphore
    *read = ftok("302933833.txt", 'z');
    if (*read == (key_t) -1) {
        exitWithError("ftok error");
    }

    // creating the key for write semaphore
    *write = ftok("302933833.txt", 'k');
    if (*write == (key_t) -1) {
        exitWithError("ftok error");
    }
}

void unlock(int semid) {
    struct sembuf sb;

    sb.sem_op = 1;
    sb.sem_num = 0;
    sb.sem_flg = SEM_UNDO;

    // unlock
    if ((semop(semid, &sb, 1)) < 0) {
        exitWithError("semop error");
    }
}

void lock(int semid) {
    struct sembuf sb;

    sb.sem_op = -1;
    sb.sem_num = 0;
    sb.sem_flg = SEM_UNDO;

    // lock
    if ((semop(semid, &sb, 1)) < 0) {
        exitWithError("semop error");
    }
}


int main(int argc, char **argv) {
    key_t sm_key, read_key, write_key;
    char *sharedMemory;
    int shmid, sem_read_id, sem_write_id;
    char task;

    // creating the appropriate keys
    createKeys(&sm_key, &read_key, &write_key);

    // create shared memory
    if ((shmid = shmget(sm_key, 4096, IPC_CREAT | 0666)) < 0) {
        exitWithError("shemget error");
    }

    // attach to the shared memory
    sharedMemory = (char *) shmat(shmid, NULL, 0);
    if (((char *) - 1) == sharedMemory) {
        exitWithError("shmat error");
    }

    // use the read semaphore
    if ((sem_read_id = semget(read_key, 1, 0666)) < 0) {
        exitWithError("semget error");
    }

    // use the write semaphore
    if ((sem_write_id = semget(write_key, 1, 0666)) < 0) {
        exitWithError("semget error");
    }

    do {
        printf("before lock\n");
        lock(sem_write_id);
        printf("after lock\n");
        scanf("\n%c", &task);
        if (task != 'i') {
            sharedMemory[0] = task;
            printf("wrote\n");

            if (task == 'h' || task == 'H' || task == 'g' || task == 'G') {
                // clean resources?
                unlock(sem_read_id);
                break;
            }
        }
        unlock(sem_read_id);

    } while (1);

    // detach from the shared memory
    if ((shmdt(sharedMemory)) <0 ) {
        exitWithError("shmdt error");
    }


    return 0;
}
