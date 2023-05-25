#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8888
#define SERVERNAME "172.22.101.221"
#define BUFSIZE 1024

/* ------------- Global Variable ------------ */
int sockfd;
/* ------------------------------------------ */

int main(void)
{
    struct sockaddr_in server;
    char buffer[BUFSIZE];

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    inet_pton(AF_INET, SERVERNAME, &server.sin_addr);
    server.sin_port = htons(PORT);

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n ERROR: Socket creation failed \n");
        exit(1);
    }

    if(connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("\n ERROR: Unable to connect to server \n");
        exit(1);
    }
    printf("\n Connected to %s \n", SERVERNAME);

    memset(buffer, 0, BUFSIZE);
    sprintf(buffer, "Hello, I'm client.");
    if(write(sockfd, buffer, BUFSIZE) == -1)
    {
        printf("\n ERROR: write() failed \n");
        exit(1);
    }

    close(sockfd);

    return 0;
}