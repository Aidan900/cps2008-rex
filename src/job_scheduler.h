//
// Created by aidan on 27/05/2019.
//

#ifndef ASSIGNMENT_CODE_JOB_SCHEDULER_H
#define ASSIGNMENT_CODE_JOB_SCHEDULER_H

#include <bits/types/time_t.h>

struct node{
    int jId;
    char* command;
    time_t sch_time;
    struct node* next_job;
};

struct job_schedule{
    struct node* head;
};

struct job_schedule* init_job_schedule();

int schedule_job(struct job_schedule* jSch, int jId, char* command,time_t time);

//delete job at the head
void delete_next_job(struct job_schedule* jShc);

int delete_job(struct job_schedule* jShc, int jId);

#endif //ASSIGNMENT_CODE_JOB_SCHEDULER_H
