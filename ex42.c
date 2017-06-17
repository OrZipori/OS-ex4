#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/fcntl.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#define ARR_SIZE 5

typedef struct {
    void (*function)(void *);
} Task;

typedef struct {
    char *queue;
    int qSize;
    int front;
    int end;
} Queue;

typedef enum {FALSE = 0, TRUE} Boolean;

// job queue - global variable
Queue jobQueue;
// internal count -- global variable
int internal_count = 0;
// shut down flag -- global variable
Boolean shutDown = FALSE;
// thread array -- global variable
pthread_t threads[ARR_SIZE];

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
}

Boolean enqueue(char mission) {
    // if the queue is null no access
    if (jobQueue.queue == NULL) return FALSE;
    // if the queue is full -> allocate dynamically more
    if (jobQueue.end == (jobQueue.qSize - 1)) {
        jobQueue.queue = (char *) realloc(jobQueue.queue, 2 * jobQueue.qSize);

        if (jobQueue.queue == NULL) {
            exitWithError("realloc error");
        }
    }

    // enqueue the mission
    jobQueue.queue[jobQueue.end++] = mission;
    return TRUE;
}

char dequeue() {
    // if the queue is null no access
    if (jobQueue.queue == NULL) return FALSE;
    // if the queue is empty
    if (jobQueue.front == jobQueue.end) {
        jobQueue.front = 0;
        jobQueue.end = 0;
        return FALSE;
    }

    // return first in line
    return jobQueue.queue[jobQueue.front++];
}

void freeQueue() {
    free(jobQueue.queue);
}

void initThreadPool() {

}

int main(int argc, char **argv) {

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
 */