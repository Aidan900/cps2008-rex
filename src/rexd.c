#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "jobs.h"
#include <pthread.h>
#include <wait.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include "semaphore.h"
#include "job_scheduler.h"

#define PATH_VAR_SIZE 1024
#define BUF_SIZE 256

int active_readers = 0; //keeping track of threads reading from job registry
int isLeader = 0;
int leaderPort = 10000;

//adapted from wikipedia
Semaphore resource_access; // to give read/write access to job_registry
Semaphore count_access; //to access active readers
Semaphore job_sch_access; //access to update job scheduler

typedef struct tp{
    struct dyn_arr* job_registry;
    struct job_schedule* sch_jobs;
    int socket;
    char* hostname;
}thread_param;

void error(char* msg);
int process_command(int sock, char* command);
int modify_registry(char op_type,struct dyn_arr* job_registry, int jId,char* hostname,char *cmd, job_type type,char* status,time_t time, int pid);
int send_to_leader(char op_type, int jId,char* hostname,char *cmd, job_type type,char* status,time_t time, int pid);

void* handle_client(void* _arg); //for thread to handle a client
void* job_scheduler(void* arg); //thread for executing scheduled jobs
void* child_listener(void* _arg); //for handling messages from non leaders


//Methods that threads use to handle the different types of commands
void handle_non_leader(struct dyn_arr* job_registry, int sock, char* command[]);
void run_command(struct dyn_arr* job_registry, int sock, char* command[], char* hostname);
void submit_command(struct dyn_arr* job_registry,struct job_schedule* j_sch, int sock, char* command[],char* hostname);
void status_command(struct dyn_arr* job_registry, int sock);
void chdir_command(int sock, char* directory);
void kill_command(struct dyn_arr* job_registry,struct job_schedule* j_sch, int sock, char* command[]);


int main(int argc, char *argv[]){

    //Declaring variables
    int sockfd, newsockfd, portno, clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int pid;
    struct dyn_arr* job_registry;
    struct job_schedule* job_sch;
    pthread_t sch_tread;
    char hostname[20]; // GET FROM PROGRAM ARGUMENTS

    if(argc<3){
        printf("Usage: ./rexd name port [L]\n");
        exit(0);
    }

    if(argc == 4 && strncmp(argv[3],"L",1) == 0){
        isLeader = 1;
    }

    portno = atoi(argv[2]);

    sprintf(hostname,"%s:%d",argv[1],portno);


    //Initialising variables
    job_registry = init_job_registry();
    job_sch = init_job_schedule();
    sm_init(&resource_access, 1);
    sm_init(&count_access, 1);
    sm_init(&job_sch_access, 1);
    thread_param* j = malloc(sizeof(thread_param));
    j->sch_jobs = job_sch;
    j->job_registry = job_registry;
    if (pthread_create(&sch_tread, NULL, job_scheduler, (void *)j) != 0) {
        perror("Error creating job scheduling thread");
    }

    //First we create the server socket
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        error("ERROR Opening socket");
    }

    //Initialize socket structure
    bzero((char *) &serv_addr, sizeof(serv_addr)); //simply initializing it to 0
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY; // Accept any connections
    serv_addr.sin_port = htons(portno);

    // Bind the host address
    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        close(sockfd);
        error("ERROR ON BINDING");
    }

    pthread_t child;

    //create listener for non leaders
    if(isLeader) {
        thread_param *t = malloc(sizeof(thread_param));
        if (t != NULL) {
            //If not null, create thread
            t->job_registry = job_registry;
            t->socket = newsockfd;
            t->sch_jobs = job_sch;
            t->hostname = hostname;
            if (pthread_create(&child, NULL, child_listener, (void *) t) != 0) {
                perror("Error creating client thread");
            }
        } else {
            error("Error passing argument to thread");
        }
    }

    //Now start listening for clients
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    printf("Sever listening for clients\n");
    while(1){
        //Accept actual connection from the client
        newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
        if(newsockfd < 0){
            close(sockfd);
            error("ERROR on ACCEPT");
        }

        pthread_t client;
        thread_param* t = malloc(sizeof(thread_param));
        if(t != NULL) {
            //If not null, create thread
            t->job_registry = job_registry;
            t->socket = newsockfd;
            t->sch_jobs = job_sch;
            t->hostname = hostname;
            if (pthread_create(&client, NULL, handle_client, (void *)t) != 0) {
                perror("Error creating client thread");
            }
        }
        else{
            error("Error passing argument to thread");
        }
    }
}

void* child_listener(void* _arg){
    thread_param* arg = (thread_param*) _arg;

    //Will listen on port 10000
    int sockfd, clilen, newsockfd;
    struct sockaddr_in serv_addr, cli_addr;
    char buffer[BUF_SIZE];
    char* tokenized_cmd[10];

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        error("ERROR Opening socket");
    }

    //Initialize socket structure
    bzero((char *) &serv_addr, sizeof(serv_addr)); //simply initializing it to 0
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY; // Accept any connections
    serv_addr.sin_port = htons(leaderPort); //listen on port 10000

    // Bind the host address
    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
        close(sockfd);
        error("ERROR ON BINDING");
    }

    //Now start listening for clients
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while(1){
        bzero(buffer,BUF_SIZE);
        newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
        if(newsockfd < 0){
            close(sockfd);
            error("ERROR on ACCEPT");
        }

        if (read(newsockfd,buffer,255) < 0){
            close(newsockfd);
            error("ERROR Reading from socket");
        }

        int i = 2;
        char *pToken = strtok(buffer, " ");// get first keyword (run, submit etc)
        tokenized_cmd[0] = pToken;
        pToken = strtok(NULL, " ");
        tokenized_cmd[1] = pToken; //type of registry access
        //if not an add to registry, no need to parse for command
        if(*pToken == 'a') {
            pToken = strtok(NULL, "]"); //getting command to store
            tokenized_cmd[2] = pToken;
            i++;
        }

        while(pToken != NULL) {
            pToken = strtok(NULL, " ");
            tokenized_cmd[i] = pToken;
            i++;
        }

        handle_non_leader(arg->job_registry, newsockfd,tokenized_cmd);
        close(newsockfd);
    }

}

void handle_non_leader(struct dyn_arr* job_registry,int sock,char* command[]){
    char* hostname = command[0];
    char* cmd;
    job_type type;
    char* status;
    time_t time;
    int pid;
    int jId;
    char add_or_upd = *command[1];

    if(add_or_upd == 'a'){
        cmd = command[2];
        type = (job_type)atoi(command[3]);
        status = command[4];
        time = (time_t) atoi(command[5]);
        pid = atoi(command[6]);
        sm_wait(&resource_access);
        if((jId = add_job_reg(job_registry,hostname,cmd, type, status,time,pid)) < 0 ){
            perror("Error adding job to registry");
        }
        sm_signal(&resource_access);
        char response[10];
        sprintf(response,"%d",jId);
        if (write(sock, response, 255) < 0) {
            perror("Error writing to non leader after adding to registry");
        }
    }
    else{
        jId = atoi(command[2]);
        status = command[3];
        time = (time_t)atoi(command[4]);
        pid = atoi(command[5]);
        sm_wait(&resource_access);
        update_job(job_registry, jId, status, time, pid);
        sm_signal(&resource_access);
    }
}

int modify_registry(char op_type,struct dyn_arr* job_registry,int jId,char* hostname,char *cmd, job_type type,char* status,time_t time, int pid){
    int new_JiD;

    if(isLeader){
        if(op_type == 'a'){
            sm_wait(&resource_access);
            if ((new_JiD = add_job_reg(job_registry, hostname, cmd, type,status, time,pid)) < 0) {
                perror("Error adding job to registry");
            }
            sm_signal(&resource_access);
            return new_JiD;
        }
        else{
            sm_wait(&resource_access);
            update_job(job_registry, jId, status, time, pid);
            sm_signal(&resource_access);
        }
    }
    else
    {
        return send_to_leader(op_type, jId,  hostname, cmd,  type,  status,  time, pid);
    }
}

int send_to_leader(char op_type, int jId,char* hostname,char *cmd, job_type type,char* status,time_t time, int pid){
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int sockfd;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        error("ERROR opening socket");
    }

    //Get server name
    if((server = gethostbyname("localhost")) == NULL)//setting it to localhost for the purposes of this assignment
    {
        fprintf(stderr,"ERROR, no such host\n");
        close(sockfd);
        exit(0);
    }

    //Like we did for client: Populate the structure
    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //ipv4
    bcopy(  (char *) server -> h_addr, // Set server address
            (char *) &serv_addr.sin_addr.s_addr,
            server -> h_length);
    serv_addr.sin_port = htons(leaderPort); // Set port (convert to network byte ordering)

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        close(sockfd);
        error("ERROR connecting to leader");
    }

    char message[256];
    bzero(message,256);
    //This is a non leader. send message to leader
    if(op_type == 'a')//add to registry
    {
        sprintf(message,"%s a %s] %d %s %ld %d",hostname,cmd ,type, status,time,pid);
        if (write(sockfd, message, 255) < 0) {
            perror("Error writing to client after submitting job");
        }
        bzero(message,256);
        if (read(sockfd, message, 255) < 0) {
            perror("Error reading from leader");
        }
        int ret_id = atoi(message); //expecting a unique job ID
        return ret_id;
    }
    else //else it is a 'u': Update registry
    {
        sprintf(message,"%s u %d %s %ld %d",hostname,jId, status,time,pid);
        if (write(sockfd, message, 255) < 0) {
            perror("Error writing to leader job");
        }
        return 0;
    }
}

/* Handles the execution of scheduled jobs*/
void* job_scheduler(void* _arg){
    thread_param* arg = (thread_param*) _arg;
    struct job_schedule* job_sch = arg->sch_jobs;
    time_t current_time;
    int pid;
    int status;

    //now keep polling job schedule until time to execute function
    while(1){
        sm_wait(&job_sch_access);
        if(job_sch->head != NULL){
            sm_signal(&job_sch_access);
            time(&current_time);
            //if current time has exceeded scheduled time
            sm_wait(&job_sch_access);
            if(difftime(current_time, job_sch->head->sch_time) > 0){
                int jId = job_sch->head->jId;
                char* command = job_sch->head->command;
                sm_signal(&job_sch_access);
                pid=fork();
                if(pid <0){
                    perror("Error executing submitted job");
                }
                //child
                if(pid==0){
                    char filename[30];
                    sprintf(filename,"%d.output",job_sch->head->jId); //to redirect output to file
                    int filedesc = open(filename,O_CREAT | O_WRONLY);
                    if(filedesc < 0){
                        perror("Error creating opening file");
                        exit(EXIT_FAILURE);
                    }
                    process_command(filedesc,command);
                }
                else{
                    sm_wait(&job_sch_access);
                    delete_next_job(job_sch);
                    sm_signal(&job_sch_access);
                    modify_registry('u',arg->job_registry, jId,NULL,NULL,0,"RUNNING",current_time,pid);

                    waitpid(pid,&status,0); //not very efficient imma too bad
                    modify_registry('u',arg->job_registry, jId,NULL,NULL,0,"TERMINATED",current_time,-1);

                }
            }
            else
                sm_signal(&job_sch_access);
        }
        else
            sm_signal(&job_sch_access);
        usleep(10000); //no need to poll on every cycle
    }
}

/* Service an incoming client */
void* handle_client(void* _arg){
    thread_param* arg = (thread_param*) _arg;
    int sock = arg->socket;
    struct dyn_arr* job_registry = arg->job_registry;
    char* hostname = arg->hostname;
    char buffer[BUF_SIZE];
    char* tokenized_cmd[10];
    char* pToken;

    bzero(buffer,BUF_SIZE);

    if (read(sock,buffer,255) < 0){
        close(sock);
        error("ERROR Reading from socket");
    }

    //Splitting the received command into an array for easier processing
    pToken = strtok(buffer, " ");// get first keyword (run, submit etc)
    tokenized_cmd[0] = pToken;
    pToken = strtok(NULL, "]");
    tokenized_cmd[1] = pToken; //the entire command is stored in one slot so that index 1 always points to entire command

    int i = 2;
    pToken = strtok(NULL, " ");
    while(pToken != NULL) {
        tokenized_cmd[i] = pToken;
        pToken = strtok(NULL, " ");
        i++;
    }
    tokenized_cmd[i] = NULL; //NULL terminated array

    //First entry should be the type of command (run, submit etc)
    if(strncmp("run", tokenized_cmd[0], 3) == 0) {
        run_command(job_registry,sock,tokenized_cmd,hostname);
    }
    else if(strncmp("status", tokenized_cmd[0], 6) == 0){
        status_command(job_registry,sock);
    }
    else if(strncmp("submit",tokenized_cmd[0], 6) == 0){
        submit_command(job_registry,arg->sch_jobs,sock, tokenized_cmd,hostname);
    }
    else if(strncmp("chdir",tokenized_cmd[0],5) == 0){
        chdir_command(sock,tokenized_cmd[1]);
    }
    else if(strncmp("kill",tokenized_cmd[0],5) == 0){
        kill_command(job_registry,arg->sch_jobs,sock,tokenized_cmd);
    }
    close(sock);//DO NOT FORGET TO CLOSE SOCKET
    free(arg);
}

/* Kill an running or scheduled process*/
void kill_command(struct dyn_arr* job_registry,struct job_schedule* j_sch, int sock, char* command[]){
    //kill doesnt work on non leaders

    //tokenising again since command[1] contains entire command. Now we split it again
    char* tokenized_cmd[10];
    char* pToken = strtok(command[1], " ");
    tokenized_cmd[0] = pToken;
    int i = 1;
    pToken = strtok(NULL, " ");
    while(pToken != NULL) {
        tokenized_cmd[i] = pToken;
        pToken = strtok(NULL, " ");
        i++;
    }
    tokenized_cmd[i] = NULL; //NULL terminated array

    int jId = atoi(tokenized_cmd[0]);
    time_t currentTime = time(NULL);
    read_job_registry(&resource_access,&count_access,&active_readers); //semaphore
    int pid = get_job_pid(job_registry,jId);
    stop_read_job_registry(&resource_access,&count_access,&active_readers); //semaphore
    int grace_period = 1; //default value
    int status;
    char result[60];

    if(tokenized_cmd[2] != NULL)
        grace_period = atoi(tokenized_cmd[2]);

    //If process is running
    if(pid>0){
        if(strncmp(tokenized_cmd[1],"hard",4)==0){
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
        }
        else {
            if(strncmp(tokenized_cmd[1],"soft",4)==0) {
                kill(pid, SIGTERM);
                waitpid(pid, &status, WNOHANG);
            }
            else {
                kill(pid, SIGTERM);
                waitpid(pid, &status, WNOHANG);
                if (WTERMSIG(status) == 0) { //returns non zero if process died
                    sleep((unsigned int) grace_period);
                    kill(pid, SIGKILL);
                }
            }
            sm_wait(&resource_access);
            delete_job(j_sch, jId);
            sm_signal(&resource_access);
        }
        sprintf(result, "Job %d was successfully terminated", jId);
    }
    else { //else check if its scheduled
        if (delete_job(j_sch, jId) < 0)
            sprintf(result, "Job %d was not found", jId);
        else {
            sprintf(result, "Job %d was successfully terminated", jId);
            sm_wait(&resource_access);
            update_job(job_registry, jId, "TERMINATED", currentTime, -1);
            sm_signal(&resource_access);
        }
    }

    if (write(sock, result, 255) < 0) {
        perror("Error writing to client after submitting job");
    }

}

/* Add a job to the job scheduler*/
void submit_command(struct dyn_arr* job_registry, struct job_schedule* j_sch, int sock, char* command[], char* hostname){
    int jId;

    time_t scheduled_time;
    struct tm* info = localtime(&scheduled_time);
    char buffer[256];

    bzero(buffer,256);

    char timeDetails[20];
    if(command[2] == NULL && command[3] == NULL){
        write(sock,"Incorrect usage of submit",26);
    }
    else {
        if(strncmp(command[2],"now",3) == 0){
            time(&scheduled_time);
        }
        else {
            //putting the time details in a single string to convert to time_t
            strcpy(timeDetails,command[2]);
            strcat(timeDetails, " ");
            strcat(timeDetails, command[3]);

            //Converting from string to dates by populating struct tm
            int year = 0, month = 0, day = 0, hour = 0, min = 0, sec=0;
            int result = sscanf(timeDetails, "%2d/%2d/%4d %2d:%2d:%2d", &day, &month, &year, &hour, &min,&sec);
            if ( result > 0) {
                info->tm_year = year - 1900; // years since 1900
                info->tm_mon = month - 1;
                info->tm_mday = day;
                info->tm_hour = hour;
                info->tm_min = min;
                info->tm_sec = sec;
                info->tm_isdst = -1;
            }
            if((scheduled_time = mktime(info)) < 0){
                printf("Time error\n");
            }
        }
        jId = modify_registry('a',job_registry,0,hostname,command[1],JOB_B, "SCHEDULED", scheduled_time,0);
        sm_wait(&job_sch_access);
        if(schedule_job(j_sch,jId, command[1], scheduled_time) < 0){
            perror("Error scheduling job");
            sm_signal(&job_sch_access);
        }
        else {
            sm_signal(&job_sch_access);
            sprintf(buffer, "Job \"%s\" submitted successfully; Unique ID is: %d", command[1], jId);
            if (write(sock, buffer, 255) < 0) {
                perror("Error writing to client after submitting job");
            }
        }
    }
}

/* Change the current working directory of the server*/
void chdir_command(int sock, char* directory){
    char* error;
    char new_path[256];

    bzero(new_path, 256);
    if(chdir(directory) < 0){
        error = strerror(errno);
        write(sock, error, 255);
    }
    else{
        getcwd(new_path, 256);
        if(write(sock,new_path,255) <0){
            perror("Error writing to client after changing directory");
        }
    }
}

/* Display the contents of the job registry*/
void status_command(struct dyn_arr* job_registry, int sock){
    int pid;
    int status;
    pid = fork();

    if(pid<0){
        error("Error in status command");
    }

    //in child
    if(pid==0){
        dup2(sock,STDOUT_FILENO);
        dup2(sock,STDERR_FILENO);
        show_jobs(job_registry);
        close(sock);
    }
    else{
        waitpid(pid, &status,0);
    }
}

/* Execute command interactively*/
void run_command(struct dyn_arr* job_registry, int sock, char* command[],char* hostname){
    int pid,status,jId;
    time_t current_time;
    time(&current_time);

    char* actual_command = command[1];

    //Creating the child
    pid = fork();

    if(pid<0){
        error("ERROR forking process");
    }

    //forked process
    if (pid == 0){
        process_command(sock,actual_command);
    }

        //Parent process
    else{
        jId= modify_registry('a',job_registry,0,hostname,command[1], JOB_I, "RUNNING",current_time,pid);
        waitpid(pid,&status,0);
        time(&current_time);
        modify_registry('u',job_registry, jId,NULL,NULL,0,"TERMINATED",current_time,-1);
    }
}

//This is a forked process. Doesnt affect the main program
int process_command(int sock, char* command)
{
    int n;
    char path[PATH_VAR_SIZE];
    char* ptoken;
    char* ctoken;
    char currentPath[PATH_VAR_SIZE];
    char* params[10] = {NULL}; //arbitrary number
    int i=0;

    close(0); /* close standard input  */
    close(1); /* close standard output */
    close(2); /* close standard error  */

    if( dup(sock) != 0 || dup(sock) != 1 || dup(sock) != 2 ) {
        error("Error duplicating socket for stdin/stdout/stderr");
    }

    bzero(path,PATH_VAR_SIZE);

    strcpy(path, getenv("PATH"));

    ctoken = strtok(command, " "); //command user wants to execute
    while(ctoken!=NULL){
        params[i] = ctoken;
        ctoken = strtok(NULL, " ");
        i++;
    }
    params[i] = NULL;

    ptoken = strtok(path, ":");

    //Trying all paths to execute function
    while(ptoken!=NULL)
    {
        strcpy(currentPath, ptoken);
        strcat(currentPath, "/");
        strcat(currentPath, params[0]);// strlen(params[0])-1);
        execv(currentPath, params);
        ptoken = strtok(NULL, ":");
    }

    //Not found if reached here
    if ((n = write(sock,"Command not found",18)) < 0) {
        close(sock);
        error("ERROR writing to socket");
    }

    close(sock);
    exit(EXIT_FAILURE);
}
