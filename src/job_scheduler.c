//
// Created by aidan on 27/05/2019.
//

#include <malloc.h>
#include <time.h>
//#include <string.h>
#include "job_scheduler.h"

struct job_schedule* init_job_schedule(){
    struct job_schedule* j = malloc(sizeof(struct job_schedule));
    j->head = NULL;
    return j;
}

int schedule_job(struct job_schedule* jSch, int jId, char* command,time_t time){
    struct node* new_job = malloc(sizeof(struct node));

    if(new_job == NULL) return -1;
    new_job->jId = jId;
    new_job->command = command; //idk why past a certain length the command fucks up
    new_job->sch_time = time;
    new_job->next_job = NULL;


    if(jSch->head == NULL){
        jSch->head = new_job;
        return 1;
    }

    struct node* current_node;
    current_node = jSch->head;
    //no other nodes to compare with except head
    if(current_node->next_job == NULL){
        if(difftime(current_node->sch_time,time)>=0){
            new_job->next_job = jSch->head;
            jSch->head = new_job;
        }
        else{
            current_node->next_job = new_job;
        }
    }
    else {
        while (current_node->next_job != NULL) {
            //checking whether new job is scheduled between two entries
            if ((difftime(time, current_node->sch_time) > 0) &&
                (difftime(current_node->next_job->sch_time, time) >= 0)) {
                if (current_node == jSch->head) {
                    new_job->next_job = current_node->next_job;
                    jSch->head = new_job;
                } else {
                    new_job->next_job = current_node->next_job;
                    current_node->next_job = new_job;
                }
                return 1;
            }

        }
        current_node->next_job = new_job;
    }
    return 1;
}

void delete_next_job(struct job_schedule* jShc) {
    if (jShc->head != NULL) {
        struct node *temp = jShc->head; //to delete
        jShc->head = jShc->head->next_job;//new head
        free(temp);
    }
}

int delete_job(struct job_schedule* jSch, int jId) {
    if(jSch->head == NULL) return -1;
    if(jSch->head->jId == jId){
        delete_next_job(jSch);
        return 1;
    }
    else{
        struct node* current_node;
        current_node = jSch->head;
        while (current_node->next_job != NULL){
            if(current_node->next_job->jId == jId){
                struct node* del_node = current_node->next_job;
                current_node->next_job = current_node->next_job->next_job;
                free(del_node);
                return 1;
            }
        }
    }
    return -1;
}
