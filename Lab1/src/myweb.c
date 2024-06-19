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
#define	SA	struct sockaddr
#define URL_ARG argv[1]
#define IP_ARG argv[2]
#define HEADER_ARG argv[3]
#define NUM_ARGS argc

int main(int argc, char **argv, char **argw, char **argx) {
    int sockfd, n;
    char recvline[MAXLINE + 1];
    const char dash[2] = "/";
    const char *lenBreak = "Content-Length:";
    const char headerCheck[] = "-h";
    const char headerBreak[16] = "\r\n\r\n<";
    const char errorBreak[20] = "HTTP/1.1 2";
    const char errorBreak2[5] = "\n";
    const char chunkCheck[40] = "Transfer-Encoding: chunked";
    char *ipHalf;
    char *pathHalf;
    char *port;
    char *errorSplit;
    char *chunkSplit;
    char *portBreak = ":";
    
    char truePathHalf[100];
    char fullLink[2048];
    int defaultPort = 80;
    int numConLen = -1;
    char *headerHolder;
    char *conLength;
    char trueConLength[7];
    char trueHeader[MAXLINE];
    char finalLine[MAXLINE + 1];
    char smallBody[MAXLINE];

    char *trueBody;
    char *body;
    int headerFlag = 0;
    FILE *fp;


    fp = fopen("output.dat", "w");

    struct sockaddr_in servaddr;
    if (NUM_ARGS == 4) {
        if (strcmp(HEADER_ARG, headerCheck) == 0) {
                headerFlag = 1;
        }
        else {
            fprintf(stderr, "Invalid arguments.\n");
            fclose(fp);
            exit(0);
        }
    }
    else if (NUM_ARGS > 4) {
        fprintf(stderr, "Invalid arguments.\n");
        fclose(fp);
        exit(0);
    }
    
    ipHalf = strtok(IP_ARG, dash);
    
    pathHalf = strtok(NULL, dash);
    snprintf(truePathHalf, sizeof truePathHalf, "/%s", pathHalf);

    port = strstr(ipHalf, portBreak);
    if (port != NULL) {
        port = port + 1;
        defaultPort = atoi(port);
        ipHalf = strtok(ipHalf, portBreak);
    }

    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Socket error!\n");
        fclose(fp);
        close(sockfd);
        exit(0);
    }
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(defaultPort);
    if (inet_pton(AF_INET, ipHalf, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "inet_pton error for %s \n", ipHalf);
        fclose(fp);
        close(sockfd);
        exit(0);
    }

    if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "Connect error. Check IP and Ports!\n");
        fclose(fp);
        close(sockfd);
        exit(0);
    }

    snprintf(fullLink, 1048, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", truePathHalf, URL_ARG);
    write(sockfd, fullLink, 1048);
    int firstLook = 1;
    int trueIndex = 0;
    while ( (n = read(sockfd, recvline, MAXLINE)) > 0) {
        recvline[n] = 0; /* null terminate */
        if (firstLook == 1) {
            
            errorSplit = strstr(recvline, errorBreak);
            if (errorSplit == NULL) {
                
                errorSplit = strtok(recvline, errorBreak2);
                fprintf(stderr, "%s\n", errorSplit);
                fclose(fp);
                close(sockfd);
                exit(0);
            }
            chunkSplit = strstr(recvline, chunkCheck);
            if (chunkSplit != NULL) {
                fprintf(stderr, "Chunked encoding is not supported!\n");
                fclose(fp);
                close(sockfd);
                exit(0);
            }
            conLength = strstr(recvline, lenBreak);
            if (conLength == NULL) {
                //Weird no content-length error.
                errorSplit = strtok(recvline, errorBreak);
                fprintf(stderr,"%s\n", errorSplit);
                fclose(fp);
                close(sockfd);
                exit(0);
            }
            
            
            memcpy(trueConLength, &conLength[15], 6);
            firstLook = 0;
            numConLen = atoi(trueConLength);
            
            if (numConLen == -1) {
                fprintf(stderr, "Chunked encoding is not supported!\n");
                fclose(fp);
                close(sockfd);
                exit(0);
            }
            trueBody = strstr(recvline, "\r\n\r\n");
            

            memcpy(trueHeader, recvline, (int)(trueBody-recvline));
            if (headerFlag == 1) {
                //Returns the header.
                fprintf(stdout, "%s\n",trueHeader);
                fclose(fp);
                close(sockfd);
                exit(0);
                
            }
            
            trueIndex = n - (int)(trueBody-recvline)-4;
            if ((numConLen + trueIndex) < MAXLINE) {
                
                //this is for small bodies
                memcpy(smallBody, trueBody+4, numConLen);
                
                fputs(smallBody, fp);
                fclose(fp);
                close(sockfd);
                exit(0);
            }
            fputs(trueBody+4, fp);
            
        }

        if (firstLook == 2) {
            
            if ((numConLen - trueIndex) >= n) {
                if (fputs(recvline, fp) == EOF) {
                    fprintf(stderr, "fputs error\n");
                    fclose(fp);
                    close(sockfd);
                    exit(0);
                }
                trueIndex = trueIndex + n;
            }
            else {
                strncpy(finalLine, recvline, numConLen - trueIndex);
                
                finalLine[numConLen - trueIndex + 1] = '\0';
                if (fputs(finalLine, fp) == EOF) {
                    fprintf(stderr, "fputs error\n");
                    fclose(fp);
                    close(sockfd);
                    exit(0);
                }
                firstLook = 3;
            }
        }
        if (firstLook != 3)
            firstLook = 2;
    }
    //Read error
    if (n < 0)
        fprintf(stderr, "read error\n");
    
    fclose(fp);
    close(sockfd);
    exit(0);
}