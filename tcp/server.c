#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/wait.h>

#define PORT 8888
#define BUFSIZE 1024

/* ------------- Global Variable ------------ */
int sockfd;
int clientsockfd;
/* ------------------------------------------ */

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

void *socketThread(void *arg)
{
    struct sockaddr_in server;
    struct sockaddr_in client;

    int client_len = sizeof(client);
    char buffer[BUFSIZE];
    ssize_t len;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n ERROR: Socket creation failed \n");
        exit(1);
    }
    printf("\n Socket created \n");

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if(bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("\n ERROR: Bind failed \n");
        exit(1);
    }
    printf("\n Bind done \n");

    if(listen(sockfd, 5))
    {
        printf("\n ERROR: Listen failed \n");
        exit(1);
    }
    printf("\n Server listening on port %d \n", PORT);
    
    while(1)
    {
        if((clientsockfd = accept(sockfd, (struct sockaddr *)&client, &client_len)) < 0)
        {
            printf("\n ERROR: accept() failed \n");
            continue;
        }
        printf("\n Accepted connection from %s \n", inet_ntoa(client.sin_addr));
    
        memset(buffer, 0, BUFSIZE);
        if(read(clientsockfd, buffer, BUFSIZE) == -1)
        {
            printf("\n ERROR: read() failed \n");
            continue;
        }
        printf("\n client say: %s \n", buffer);
    }
}

int main(void)
{
    pthread_t tid;

    initSig();

    if(pthread_create(&tid, NULL, socketThread, NULL))
    {
        printf("\n ERROR: Thread creation failed \n");
        exit(1);
    }
    printf("\n Create the socketThread \n");

    while(1)
    {
        ; // Do your things
    }

    return 0;
}