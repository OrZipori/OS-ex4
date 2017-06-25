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

typedef struct {
    void (*function)(void *);
} Task;

typedef struct {
    char *queue;
    int qSize;
    int front;
    int end;
    int remaining;
    int items;
} Queue;

typedef struct {
    pthread_mutex_t queueLock;
    pthread_mutex_t fileLock;
    pthread_mutex_t internalLock;
    pthread_mutexattr_t attr;
    int fd;
} Locks;

typedef enum {FALSE = 0, TRUE} Boolean;

// job queue - global variable
Queue jobQueue;
// internal count -- global variable
int internal_count = 0;
// shut down flag -- global variable
Boolean shutDown = FALSE;
// thread array -- global variable
pthread_t threads[ARR_SIZE];
// locks -- global variable
Locks locks;

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

void initQueue(int size) {
    jobQueue.queue = (char *) malloc(sizeof(char) * size);
    if (jobQueue.queue == NULL) {
        exitWithError("malloc error");
    }

    // set the queue size
    jobQueue.qSize = size;
    jobQueue.front = 0;
    jobQueue.end = 0;
    jobQueue.items = 0;
    jobQueue.remaining = 0;
}

Boolean enqueue(char mission) {
    // if the queue is null no access
    if (jobQueue.queue == NULL) return FALSE;

    // lock the queue
    if((pthread_mutex_lock(&(locks.queueLock)) != 0)) {
        exitWithError("mutex lock error");
    }

    // if the queue is full -> allocate dynamically more
    if (jobQueue.end == (jobQueue.qSize - 1)) {
        jobQueue.queue = (char *) realloc(jobQueue.queue, 2 * jobQueue.qSize);

        if (jobQueue.queue == NULL) {
            exitWithError("realloc error");
        }
    }

    jobQueue.items++;
    // enqueue the mission
    jobQueue.queue[jobQueue.end++] = mission;

    // unlock the queue
    if((pthread_mutex_unlock(&(locks.queueLock)) != 0)) {
        exitWithError("mutex unlock error");
    }

    return TRUE;
}

char dequeue() {
    char c;

    // if the queue is null no access
    if (jobQueue.queue == NULL) return FALSE;

    // lock the queue
    if((pthread_mutex_lock(&(locks.queueLock)) != 0)) {
        exitWithError("mutex lock error");
    }

    // if the queue is empty
    if (jobQueue.front == jobQueue.end) {
        jobQueue.front = 0;
        jobQueue.end = 0;
        c = FALSE;
    } else {
        jobQueue.items--;

        if (shutDown) jobQueue.remaining--;

        // return first in line
        c = jobQueue.queue[jobQueue.front++];
    }

    // unlock the queue
    if((pthread_mutex_unlock(&(locks.queueLock)) != 0)) {
        exitWithError("mutex unlock error");
    }

    return c;
}

void freeQueue() {
    free(jobQueue.queue);
}

void increment(int num) {
    // lock internal_count
    if((pthread_mutex_lock(&(locks.internalLock)) != 0)) {
        exitWithError("mutex lock error");
    }

    internal_count += num;

    // unlock internal_count
    if((pthread_mutex_unlock(&(locks.internalLock)) != 0)) {
        exitWithError("mutex lock error");
    }
}

void microWait() {
    struct timespec time, ret;

    time.tv_nsec = 0;
    time.tv_sec = 1;

    if ((nanosleep(&time, &ret)) < 0) {
        exitWithError("nanosleep error");
    }
}

void nanoWait() {
    int waitTime;
    struct timespec time, ret;

    // we want a number between 10 and 100
    waitTime = (rand() % 91) + 10;

    time.tv_nsec = waitTime;
    time.tv_sec = 0;

    if ((nanosleep(&time, &ret)) <0) {
        exitWithError("nanosleep error");
    }
}

void writeToFile() {
    char buffer[ARR_SIZE * 40];
    unsigned int temp;

    // lock internal_count
    if((pthread_mutex_lock(&(locks.fileLock)) != 0)) {
        exitWithError("mutex lock error");
    }

    // thread id
    temp = (unsigned int) pthread_self();

    sprintf(buffer, "thread identifier is %u and internal_count is %d\n", temp, internal_count);

    // write to file
    if ((write(locks.fd, buffer, strlen(buffer))) < 0) {
        exitWithError("write error");
    }

    // lock internal_count
    if((pthread_mutex_unlock(&(locks.fileLock)) != 0)) {
        exitWithError("mutex lock error");
    }
}

void* mainThreadFunction(void * args) {
    Task task;
    char c;

    printf("thread %d started\n", (unsigned int) pthread_self());

    while (TRUE) {
        // exit the thread while shutdown
        if (shutDown) break;

        // get the task to execute
        c = dequeue();

        printf("thread %d got %c\n", (unsigned int) pthread_self(), c);

        // assign correct function
        switch (c) {
            case 0: // no new task in the queue
                microWait();
                continue;
            case 'a':
                nanoWait();
                increment(1);
                break;
            case 'b':
                nanoWait();
                increment(2);
                break;
            case 'c':
                nanoWait();
                increment(3);
                break;
            case 'd':
                nanoWait();
                increment(4);
                break;
            case 'e':
                nanoWait();
                increment(5);
                break;
            case 'f':
                writeToFile();
                break;
            default:
                break;
        }

        printf("thread %d started is done with %c\n", (unsigned int) pthread_self(), c);

    }

    if (shutDown) {
        // run function that writes to the files --case h
        printf("thread %d shutdown\n", (unsigned int) pthread_self());
        writeToFile();
    }

    // return null
    return NULL;
}

void initThreadPool(int qSize) {
    int i;
    // init the job queue
    initQueue(qSize);

    pthread_mutexattr_init(&(locks.attr));
    pthread_mutexattr_settype(&(locks.attr), PTHREAD_MUTEX_ERRORCHECK);

    // initialize mutex &(locks.attr)
    if (pthread_mutex_init(&(locks.queueLock), NULL) != 0) {
        exitWithError("mutex init error");
    }
    // initialize mutex
    if (pthread_mutex_init(&(locks.fileLock), NULL) != 0) {
        exitWithError("mutex init error");
    }
    // initialize mutex
    if (pthread_mutex_init(&(locks.internalLock), NULL) != 0) {
        exitWithError("mutex init error");
    }

    // create the threads
    for (i = 0; i < ARR_SIZE; ++i) {
        if ((pthread_create((threads + i), NULL, mainThreadFunction, NULL)) != 0) {
            exitWithError("pthread create error");
        }
    }
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

void waitBeforeEnding(char *sharedMemory) {
    int i;

    // how much jobs to do before ending
    jobQueue.remaining = jobQueue.items;

    // wait until remaining job are 0
    while (jobQueue.remaining > 0) {
        microWait();
    }

    // block thread from receiving new jobs
    shutDown = TRUE;

    // wait for all threads
    for (i = 0; i < ARR_SIZE; ++i) {
        if ((pthread_join(threads[i], NULL)) < 0) {
            exitWithError("join error");
        }
    }

    // main thread
    writeToFile();

    // detach from the shared memory
    if ((shmdt(sharedMemory)) <0 ) {
        exitWithError("shmdt error");
    }
}

void endImmediately() {
    int i;

    printf("endImmediately\n");
    // end all threads
    for (i = 0; i < ARR_SIZE; ++i) {
        if ((pthread_cancel(threads[i])) < 0) {
            exitWithError("cancel error");
        }
    }
    printf("end of endImmediately");

    // main thread
    writeToFile();
}

void freeResources(int read, int write, int shmid) {
    union semun semUnion;
    semUnion.val = 1;

    // free read
    if ((semctl(read, 0, IPC_RMID, semUnion)) < 0) {
        exitWithError("semctl error");
    }

    // free write
    if ((semctl(write, 0, IPC_RMID, semUnion)) < 0) {
        exitWithError("semctl error");
    }

    // free mutex
    if ((pthread_mutex_destroy(&(locks.fileLock))) < 0) {
        exitWithError("destroy error");
    }

    // free mutex
    if ((pthread_mutex_destroy(&(locks.internalLock))) < 0) {
        exitWithError("destroy error");
    }

    // free mutex
    if ((pthread_mutex_destroy(&(locks.queueLock))) < 0) {
        exitWithError("destroy error");
    }

    // remove shared memory
    if ((shmctl(shmid, IPC_RMID, NULL)) < 0) {
        exitWithError("shmctl error");
    }
}

int main(int argc, char **argv) {
    key_t sm_key, read_key, write_key;
    char *sharedMemory;
    int shmid, sem_read_id, sem_write_id;
    char task;
    union semun semUnion;

    // create <yourID.txt>
    if ((locks.fd = open("302933833.txt", O_CREAT | O_RDWR | O_TRUNC, 0666)) < 0) {
        exitWithError("error creating file");
    }

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

    // initialize shared memory
    memset(sharedMemory, 0, 4096);

    // initialize the thread pool
    initThreadPool(10);

    // seed the random
    srand(time(NULL));

    // create the read semaphore
    if ((sem_read_id = semget(read_key, 1, IPC_CREAT |0666)) < 0) {
        exitWithError("semget error");
    }

    // create the write semaphore
    if ((sem_write_id = semget(write_key, 1, IPC_CREAT |0666)) < 0) {
        exitWithError("semget error");
    }

    // initialize the read semaphore
    semUnion.val = 0;
    if ((semctl(sem_read_id, 0, SETVAL, semUnion)) < 0) {
        exitWithError("semctl error");
    }

    // initialize the write semaphore
    semUnion.val = 1;
    if ((semctl(sem_write_id, 0, SETVAL, semUnion)) < 0) {
        exitWithError("semctl error");
    }

    do {
        lock(sem_read_id);

        // get the task
        task = sharedMemory[0];
        if (task == 'h' || task == 'H') {
            waitBeforeEnding(sharedMemory);
            freeResources(sem_read_id, sem_write_id, shmid);
            break;
        } else if (task == 'g' || task == 'G') {
            endImmediately();
            freeResources(sem_read_id, sem_write_id, shmid);
            break;
        } else {
            // add the task to the job queue
            enqueue(task);
        }

        // set read semaphore to 0 in order to make it stop again at the lock
        semUnion.val = 0;
        if ((semctl(sem_read_id, 0, SETVAL, semUnion)) < 0) {
            exitWithError("semctl error");
        }

        // unlock write semaphore
        unlock(sem_write_id);
    } while(TRUE);

    freeQueue();

    return 0;

}

/*
 * every thread that is free runs a while true that every second search for a
 * new task that can be run. (use mutex to ensure that only one thread is capable
 * of selecting the next task).
 *
 * the job queue will be dynamic array that has two pointers, one for the front
 * and the other for the back.
 *
 * thread pool :
 * every thread :
 *      try to dequeue next task, if succeeded lock and assign the correct function by the char
 *      and exit the lock and run it. if dequque == 0 continue to run on with while true
 *      if flag of shutdown is true all thread must end (after finishing their tasks)
 *
 *
 * regarding shared memory reading :
 *
 * third semaphore :
 *      server :
 *          while()
 *              lock(third)
 *                  lock(write)
 *                      read
 *
 * client :
 *  lock write (initialize to 1)
 *      get char
 *      write to memory
 *      unlock read
 *
 *  server : (initialize read to 0)
 *      lock read
 *          read char
 *          ......
 *          setval lock to 0
 *          unlock write
 *
 */