#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#define	MAXLINE		4096
#define	LISTENQ		1024
#define	SERV_PORT		 9877
#define	SA	struct sockaddr
#define SERVER_PORT argv[1]
#define IP_ARG argv[2]
#define HEADER_ARG argv[3]
#define NUM_ARGS argc

void dg_echo(int sockfd, SA *pcliaddr, socklen_t clilen) {
    int n;
    socklen_t len;
    char mesg[MAXLINE];

    for ( ; ; ) {
        len = clilen;
        n = recvfrom(sockfd, mesg, MAXLINE, 0, pcliaddr, &len);

        sendto(sockfd, mesg, n, 0, pcliaddr, len);
    }
}


int main(int argc, char **argv){
    int sockfd;
    int trueServerPort = 9877;
    struct sockaddr_in servaddr, cliaddr;
    /*
    FILE *fp;
    char testing[4096];
    int testC;
    int i = 0;
    int k = 0;
    fp = fopen("testing.txt", "r");
    while (feof(fp) == 0) {
        
        testC = fgetc(fp);
        testing[i] = testC;
        //printf("%d\n", i);
        //if (feof(fp) != 0) {
        //    printf("Breaking!\n");
        //    break;
        //} 
        i++;
    }
    testing[i] = '\0';
    for (int j = 0; j < i; j++) {
        fputc(testing[j], stdout);
        k++;
    }
    printf("\n");
    if (testing[0] == 'h') {
        printf("Verified! %d and %d and %d\n", i, k, strlen(testing));
    } */
    //printf("%s\n", testing);

    trueServerPort = atoi(SERVER_PORT);
    if (argc != 2) {
        fprintf(stderr, "Incorrect initial arguments! Too many/not enough!\n");
        exit(1);
    }
    //printf("server port: %d \n", trueServerPort);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
    servaddr.sin_port = htons(trueServerPort);

    bind(sockfd, (SA *) &servaddr, sizeof(servaddr));

    dg_echo(sockfd, (SA *) &cliaddr, sizeof(cliaddr));
}