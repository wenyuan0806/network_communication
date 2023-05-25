#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

#define PORT 8888
#define BUFSIZE 1024

/* ------------- Global Variable ------------ */
int sockfd;
int clientsockfd;
/* ------------------------------------------ */

int main(void)
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

    return 0;
}