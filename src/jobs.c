//
// Created by aidan on 24/05/2019.
//

#include <time.h>
#include "jobs.h"
#include <malloc.h>
#include <string.h>

#define INIT_CAPACITY 5

//int get_unique_id(struct dyn_arr* j);
struct dyn_arr* init_dyn_arr();
int dyn_add(struct dyn_arr* d, struct job* j);
char* job_type_to_string(job_type j);


//struct dyn_arr* job_registry = NULL;

struct dyn_arr* init_job_registry(){
    struct dyn_arr* j = init_dyn_arr();
    if(j ==NULL) return NULL;
    else return j;
}

int add_job_reg(struct dyn_arr* jr,const char* host, const char* command, job_type type, char* status, time_t time, int pid){
    //Initialising a struct
    struct job* new_job = malloc(sizeof(struct job));
    if (new_job == NULL) return -1;
    new_job->jId = jr->current_size;
    new_job->type = type;
    bzero(new_job->host,20);
    bzero(new_job->status,20);
    bzero(new_job->command,50);
    strncpy(new_job->host,host,strlen(host));
    strncpy(new_job->command, command,strlen(command));
    strncpy(new_job->status,status,strlen(status));
    new_job->regTime = time;
    new_job->pid = pid;

    if(dyn_add(jr,new_job) < 0){
        return -1;
    }
    return new_job->jId;
}

void show_jobs(struct dyn_arr* jr){
    fflush(stdout);
    printf("%4.4s %17.17s %20.20s %10.10s %10.10s %10.10s %10.10s\n",
            "Job", "Host", "Command", "Type", "Status", "Date", "Time");
    for(int i=0;i<jr->current_size; i++) {
        struct job* j = (jr->jobs+i);
        char date_buffer[10];
        char time_buffer[10];
        struct tm* info;
        info = localtime(&j->regTime);
        char* j_type = job_type_to_string(j->type);
        strftime(date_buffer,10,"%x",info);
        strftime(time_buffer,10,"%H:%M:%S",info);
        printf("%4d %17.17s %20.20s %10.10s %10.10s %10.10s %10.10s\n",
                j->jId,j->host,j->command,j_type, j->status, date_buffer, time_buffer);
    }
}

void update_job(struct dyn_arr* jr, int jobID, char* status, time_t time,int pid){
    if(jobID < jr->current_size) {
        struct job *j = (jr->jobs + jobID); //getting the required job
        strncpy(j->status,status,strlen(status));
        j->regTime = time;
        if (pid != 0) {
            j->pid = pid;
        }
    }
}

int get_job_pid(struct dyn_arr* jr, int jobID){
    if(jobID < jr->current_size) {
        struct job *j = (jr->jobs + jobID);
        return j->pid;
    }
    else return -1;
}

char* job_type_to_string(job_type j){
    if(j==JOB_B) return "B";
    else return "I";
}

struct dyn_arr* init_dyn_arr(){
    struct dyn_arr* da =  malloc(sizeof(struct dyn_arr));
    if(da == NULL) return NULL;
    da->jobs = malloc(sizeof(struct job)*INIT_CAPACITY);
    if(da->jobs == NULL) return NULL;
    da->current_size = 0;
    da-> max_size = INIT_CAPACITY;
    return da;
}

int dyn_add(struct dyn_arr* d, struct job* j){
    if(d->current_size >= d->max_size){
        d->jobs = realloc(d->jobs,sizeof(struct job)*(d->max_size + 5));
        if(d->jobs == NULL) return -1;
        d->max_size += 5;
    }
    *(d->jobs+d->current_size) = *j;
    d->current_size++;
    return 1;
}
