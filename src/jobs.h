//
// Created by aidan on 24/05/2019.
//

#ifndef ASSIGNMENT_CODE_JOBS_H
#define ASSIGNMENT_CODE_JOBS_H

#endif //ASSIGNMENT_CODE_JOBS_H

typedef enum job_type{
    JOB_B = 'B', JOB_I='I'
}job_type;

struct job{
    int jId;
    char host[20];
    job_type type;
    time_t regTime;
    char status[20];
    char command[50];
    int pid;
};

struct dyn_arr{
    int current_size; //current size will also be used to denote the unique job id, since entries can't be removed
    int max_size;
    struct job* jobs;
};

/*! \brief  Adds a job to the global registry at the current time and date
 *
 *  \param host The owner of the job
 *  \param command The command to execute
 *  \param The type of command ( (B)atch or (I)nteractive )
 *
 *  \return The unique id for that job. -1 if error
 */
int add_job_reg(struct dyn_arr* jr, const char* host, const char* command, job_type type,char* status, time_t time, int pid);


/*! \brief  Submits a job to be executed at a certain time
 *
 *  \param host The owner of the job
 *  \param command The command to execute
 *  \param The type of command ( (B)atch or (I)nteractive )
 *
 *  \return The unique id for that job
 */

/*! \brief Initializes a job registry in memory
 *  \return NULL if error
 */
struct dyn_arr* init_job_registry();

/*! \brief  Displays the status of all jobs in the registry */
void show_jobs(struct dyn_arr* jr);

void update_job(struct dyn_arr* jr, int jobID, char* status, time_t time, int pid);

int get_job_pid(struct dyn_arr* jr, int jobID);