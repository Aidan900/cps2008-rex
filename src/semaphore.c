//
// Created by aidan on 26/05/2019.
//

#include "semaphore.h"
#include <pthread.h>

void sm_init(Semaphore *sem,int init_val){
    sem->count = init_val;
    pthread_mutex_init(&sem->lock, NULL);
}

void sm_wait(Semaphore* sem){
    while(1){
        pthread_mutex_lock(&sem->lock);
        if(sem->count > 0){
            sem->count--;
            pthread_mutex_unlock(&sem->lock);
            break;
        }
        pthread_mutex_unlock(&sem->lock);
    }
}

void sm_signal(Semaphore* sem){
    pthread_mutex_lock(&sem->lock);
    sem->count++;
    pthread_mutex_unlock(&sem->lock);
}

void sm_destroy(Semaphore* sem){
    pthread_mutex_destroy(&sem->lock);
}

void read_job_registry(Semaphore* w, Semaphore* r, int* readCount){
    sm_wait(r);
    (*readCount)++;
    if(*readCount == 1)
        sm_wait(w); //lock resource
    sm_signal(r);
    //now read
}
void stop_read_job_registry(Semaphore* w,Semaphore* r, int* readCount){
    sm_wait(r);
    (*readCount)--;
    if(*readCount==0)
        sm_signal(w); //free resource
    sm_signal(r);

}
