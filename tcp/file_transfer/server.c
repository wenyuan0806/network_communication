#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <fcntl.h>

#define PORT 8888
#define BUFSIZE 256

/* ------------- TCP File Transfer Direction ------------ */
#define SERV_TO_CLI 0x01
#define CLI_TO_SERV 0x02
/* ------------------------------------------------------ */

/* ------------------ Global Variable ------------------- */
int sockfd;
int clientsockfd;
/* ------------------------------------------------------ */

void sig_handler(int signo)
{
    if(signo == SIGINT)
        printf("\n received SIGINT \n");
    else if(signo == SIGTERM)
        printf("\n received SIGTERM \n");
    
    close(sockfd);
    exit(0);
}

void initSig(void)
{
    if(signal(SIGINT, sig_handler) == SIG_ERR)
        printf("\n Can't catch SIGINT \n");
    if(signal(SIGTERM, sig_handler) == SIG_ERR)
        printf("\n Can't catch SIGTERM \n");
    printf("\n Initialize signal handler \n");
}

void error(char *msg)
{
    printf("\n");
    perror(msg);
    printf("\n");
    exit(1);
}

void *socketThread(void *arg)
{
    struct sockaddr_in server;
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);

    char DIRECTION[1];
    char FILEPATH[BUFSIZE];
    char BUFFER[BUFSIZE];

    int reuse = 1;
    int filefd;
    FILE *file;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error(" socket()");

    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        error(" setsockopt()");

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if(bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
        error(" bind()");

    if(listen(sockfd, 5))
        error(" listen()");
    
    printf("\n Server listening on port %d \n", PORT);
    
    while(1)
    {
        close(clientsockfd);
        if((clientsockfd = accept(sockfd, (struct sockaddr *)&client, &client_len)) < 0)
        {
            printf("\n");
            perror(" accept()");
            printf("\n");
            continue;
        }

        printf("\n Accepted connection from %s \n", inet_ntoa(client.sin_addr));

        memset(DIRECTION, 0, sizeof(DIRECTION));
        if(read(clientsockfd, DIRECTION, sizeof(DIRECTION)) <= 0)
        {
            /* 
            printf("\n");
            perror(" read() 1");
            printf("\n"); 
            */
            continue;
        }

        printf("\n Client says: 0x%02x \n", DIRECTION[0]);

        memset(FILEPATH, 0, BUFSIZE);
        if(read(clientsockfd, FILEPATH, BUFSIZE) == -1)
        {
            printf("\n");
            perror(" read() 2");
            printf("\n");
            continue;
        }

        printf("\n Client says: %s \n", FILEPATH);

        switch(DIRECTION[0])
        {
            case SERV_TO_CLI:
                printf("\n Client request a file in [%s] \n", FILEPATH);

                file = fopen(FILEPATH, "rb");
                filefd = fileno(file);
                if(!file)
                {
                    printf("\n");
                    perror(" fopen()");
                    printf("\n");
                    break;
                }

                memset(BUFFER, 0, BUFSIZE);
                while(read(filefd, BUFFER, BUFSIZE))
                {
                    if(write(clientsockfd, BUFFER, BUFSIZE) == -1)
                    {
                        printf("\n");
                        perror(" write()");
                        printf("\n");
                        break;
                    }
                    memset(BUFFER, 0, BUFSIZE);
                }

                printf("\n write finished \n");

                close(filefd);
                fclose(file);
                break;

            case CLI_TO_SERV:
                printf("\n Client will send file to [%s] \n", FILEPATH);
                break;

            default:
                printf("\n ERROR: The direction code that client sent is not exist. Please try 0x01(serv->cli) or 0x02(cli->serv) \n");
                break;
        }
        close(clientsockfd);
    }
}

int main(void)
{
    pthread_t tid;

    initSig();

    if(pthread_create(&tid, NULL, socketThread, NULL))
        error(" pthread_create()");

    printf("\n Create the socketThread \n");

    while(1)
    {
        ; // Do your things
    }

    return 0;
}