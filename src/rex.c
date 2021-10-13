#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>

void error(char* msg);

int main(int argc, char *argv[])
{
    //Declaring variables
    int sockfd, portno, n;
    char* hostname;
    char tempCommand[256];
    char command[256];
    struct sockaddr_in serv_addr;
    struct hostent *server;


    char buffer[256];


    // Make sure that server name and port are available in command line arguments
    if(argc < 2){
        fprintf(stderr,"Usage: %s <port> <run|submit> \"<hostname>:<command>\" [(date time) | now]\n", argv[0]);
        exit(0);
    }

    bzero(tempCommand, 256);
    bzero(command, 256);


    if(strcmp(argv[2],"run")==0 || strncmp(argv[2],"chdir",5)==0){
        strcpy(tempCommand,argv[2]);
        strcat(tempCommand," ");
        char *cToken = strtok(argv[3], ":");
        hostname = cToken;
        cToken = strtok(NULL, ":");
        strcat(tempCommand,cToken);
        strcat(tempCommand,"]");
        for(int i= 4; i< argc; i++){
            strcat(tempCommand," ");
            strcat(tempCommand,argv[i]);
        }
//    strcat(tempCommand,"]");

    }
    else if(strncmp(argv[2],"submit",6)==0){
        strcpy(tempCommand,argv[2]);
        strcat(tempCommand," ");
        char *cToken = strtok(argv[3], ":");
        hostname = cToken;
        cToken = strtok(NULL, ":");
        strcat(tempCommand,cToken);
        strcat(tempCommand,"]");
        for(int i= 4; i< argc; i++){
            strcat(tempCommand," ");
            strcat(tempCommand,argv[i]);
        }
    }
    else if(strncmp(argv[2],"status",6)==0)
    {
        strcpy(tempCommand,argv[2]);
        hostname = "localhost";
    }
    else if(strncmp(argv[2],"kill",4)==0){
        hostname = "localhost";
        strcpy(tempCommand,argv[2]);
        strcat(tempCommand," ");
        for(int i= 3; i< argc; i++){
            strcat(tempCommand," ");
            strcat(tempCommand,argv[i]);
        }
    }

    bcopy(tempCommand,command,255);

    //GETTING host and command details

    //Get port number
    portno = atoi(argv[1]);


    //Create a socket
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        error("ERROR opening socket");
    }

    //Get server name
    if((server = gethostbyname(hostname)) == NULL)
    {
        fprintf(stderr,"ERROR, no such host\n");
        close(sockfd);
        exit(0);
    }

    //Like we did for server: Populate the structure
    bzero((char*) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //ipv4
    bcopy(  (char *) server -> h_addr, // Set server address
            (char *) &serv_addr.sin_addr.s_addr,
            server -> h_length);
    serv_addr.sin_port = htons(portno); // Set port (convert to network byte ordering)

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        close(sockfd);
        error("ERROR connecting to server");
    }

    bzero(buffer, 256);

    if ((n = write(sockfd, command, strlen(command))) < 0)
    {
        close(sockfd);
        error("ERROR writing to socket");
    }


    // Read response from server response
    bzero(buffer,256);

    int bytesRead = 1;
    //Keep reading until eof (read() = 0)
    while(bytesRead > 0)
    {
        bytesRead = (int)read(sockfd,buffer,255);
        if(bytesRead < 0){
            close(sockfd);
            error("ERROR reading from socket");
        }
        if(bytesRead!=0)
            printf("%s\n",buffer);
    }

    // All done, close socket
    close(sockfd);
    return 0;
}

void error(char* msg){
    perror(msg);
    exit(EXIT_FAILURE);
}