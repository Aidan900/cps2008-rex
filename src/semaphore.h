//
// Created by aidan on 26/05/2019.
//

#ifndef ASSIGNMENT_CODE_SEMAPHORE_H
#define ASSIGNMENT_CODE_SEMAPHORE_H


#include <sys/types.h>

typedef struct S{
    int count;
    pthread_mutex_t lock;
} Semaphore;

void sm_init(Semaphore* sem,int init_val);

void sm_wait(Semaphore* sem);

void sm_signal(Semaphore* sem);

void sm_destroy(Semaphore* sem);

//semaphore access methods
void read_job_registry(Semaphore* w, Semaphore* c,int* readCount);
void stop_read_job_registry(Semaphore* w,Semaphore* c,int* readCount);

#endif //ASSIGNMENT_CODE_SEMAPHORE_H
