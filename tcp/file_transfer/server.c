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
#define BUFSIZE_1   1
#define BUFSIZE_12  12
#define BUFSIZE_256 256

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

long calFileSize(FILE *fp)
{
    // seek to end of file
    if (fseek(fp, 0, SEEK_END) == -1) {
        perror("fseek() SEEK_END");
        return -1;
    }

    // get file size
    long size = ftell(fp);
    if (size == -1) {
        perror("ftell error");
        return -1;
    }

    // seek to end of file
    if (fseek(fp, 0, SEEK_SET) == -1) {
        perror("fseek() SEEK_SET");
        return -1;
    }

    return size;
}

void *socketThread(void *arg)
{
    struct sockaddr_in server;
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);

    char DIRECTION[BUFSIZE_1];
    char FILEPATH[BUFSIZE_256];
    char FILESIZE[BUFSIZE_12];
    char DATA[BUFSIZE_256];

    int reuse = 1;
    int filefd;
    FILE *file;
    long fileSize = 0;

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

        while(1)
        {
            printf("\n ======================================== \n");
            memset(DIRECTION, 0, BUFSIZE_1);
            if(read(clientsockfd, DIRECTION, BUFSIZE_1) <= 0)
            {
                /* 
                printf("\n");
                perror(" read() 1");
                printf("\n"); 
                */
                break;
            }

            /* if (DIRECTION[0] == 0x00)
                continue; */
            printf("\n Client says: 0x%02x \n", DIRECTION[0]);

            memset(FILEPATH, 0, BUFSIZE_256);
            if(read(clientsockfd, FILEPATH, BUFSIZE_256) == -1)
            {
                printf("\n");
                perror(" read() 2");
                printf("\n");
                break;
            }

            printf("\n Client says: %s \n", FILEPATH);

            memset(FILESIZE, 0, BUFSIZE_12);
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

                    memset(FILESIZE, 0, BUFSIZE_12);
                    fileSize = calFileSize(file);
                    sprintf(FILESIZE, "%ld", fileSize);
                    if(write(clientsockfd, FILESIZE, BUFSIZE_12) == -1)
                    {
                        printf("\n");
                        perror(" write()");
                        printf("\n");
                        break;
                    }

                    memset(DATA, 0, BUFSIZE_256);
                    while(read(filefd, DATA, BUFSIZE_256))
                    {
                        if(write(clientsockfd, DATA, BUFSIZE_256) == -1)
                        {
                            printf("\n");
                            perror(" write()");
                            printf("\n");
                            break;
                        }
                        memset(DATA, 0, BUFSIZE_256);
                    }

                    printf("\n write finished \n");

                    break;

                case CLI_TO_SERV:
                    printf("\n Client will send file to [%s] \n", FILEPATH);

                    file = fopen(FILEPATH, "wb");
                    filefd = fileno(file);
                    if(!file)
                    {
                        printf("\n");
                        perror(" fopen()");
                        printf("\n");
                        break;
                    }

                    if(read(clientsockfd, FILESIZE, BUFSIZE_12) == -1)
                    {
                        printf("\n");
                        perror(" read()");
                        printf("\n");
                        break;
                    }
                    fileSize = atoi(FILESIZE);
                    printf("\n File size is totally %lu bytes \n", fileSize);

                    memset(DATA, 0, BUFSIZE_256);
                    while(1)
                    {
                        if(read(clientsockfd, DATA, BUFSIZE_256) == -1)
                        {
                            printf("\n");
                            perror(" read()");
                            printf("\n");
                            break;
                        }
                        /* for(int i=0; i<BUFSIZE_256; i++)
                        {
                            printf("%02x ", DATA[i]);
                        }
                        printf("\n"); */
                        
                        fileSize -= BUFSIZE_256;
                        if(fileSize < 0)
                        {
                            write(filefd, DATA, fileSize+BUFSIZE_256);
                            break;
                        }
                        write(filefd, DATA, BUFSIZE_256);
                        memset(DATA, 0, BUFSIZE_256);
                    }

                    printf("\n read finished \n");

                    break;

                default:
                    printf("\n ERROR: The direction code that client sent is not exist. Please try 0x01(serv->cli) or 0x02(cli->serv) \n");
                    break;
            }
            close(filefd);
            fclose(file);
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