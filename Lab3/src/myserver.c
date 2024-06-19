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
#include <math.h>

#define	MAXLINE		4096
#define	LISTENQ		1024
#define	SERV_PORT		 9877
#define	SA	struct sockaddr
#define SERVER_PORT argv[1]
#define DROPPC argv[2]
#define NUM_ARGS argc
#define MAX_CLIENTS 128
#define MAX_FILE_PATH 128
#define MAX_DATA_SIZE 32768
#define PACKET_FOUND_FALSE 0
#define PACKET_FOUND_TRUE 1
#define PACKET_LOST_FALSE 0
#define PACKET_LOST_TRUE 1
#define NO_DUPLICATES 0
#define DUPLICATES_FOUND 1
#define APPEND 0
#define CLEAR 1
#define PCLIADDR (SA *) &cliaddr
#define CLILEN sizeof(cliaddr)


typedef struct pkt {
    int index;
    uint32_t thissn;
    uint32_t nextsn;
    int strLength;
    char path[MAX_FILE_PATH];
    char data[MAX_DATA_SIZE];
} pkt;

typedef struct ack {
    uint32_t thissn;
    uint32_t nextsn;
    int index;
} ack;


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
    int dropChance = 0;
    

    trueServerPort = atoi(SERVER_PORT);
    if (argc != 3) {
        fprintf(stderr, "Incorrect initial arguments! Too many/not enough!\n");
        exit(1);
    }

    pkt packet;
    ack acket;
    acket.nextsn = 1;
    dropChance = atoi(DROPPC);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
    servaddr.sin_port = htons(trueServerPort);

    bind(sockfd, (SA *) &servaddr, sizeof(servaddr));

    FILE *outputFile;
    int currentClient = 0;
    int totalClients = 0;
    char files[MAX_CLIENTS][MAX_FILE_PATH];
    uint32_t nextSn[MAX_CLIENTS] = {1};
    int duplicateFlag = NO_DUPLICATES;
    int packetFound = PACKET_FOUND_FALSE;
    int packetLost = PACKET_LOST_FALSE;
    int clearFile = APPEND;
    char outFileString[MAX_FILE_PATH];
    socklen_t cliLen;
    time_t t;
    srand((unsigned) time(&t));
    int64_t eternalTimer = 0;
    int weNeedtoUnACK = 0;
    int hours, minutes, seconds, day, month, year;
    time_t now;
    time_t whoops;
    time(&now);
    struct tm *tx = gmtime(&now); 
    hours = tx ->tm_hour;         
    minutes = tx ->tm_min;        
    seconds = tx ->tm_sec;      
    day = tx ->tm_mday;            
    month = tx ->tm_mon + 1;     
    year = tx ->tm_year + 1900; 

    while (1) {
        
        cliLen = sizeof(cliaddr);
        recvfrom(sockfd, &packet, sizeof(packet), 0, PCLIADDR, &cliLen);
        //Drop Data
        if (dropChance > (rand() % 100)) {
            //Drop
            packetLost = PACKET_LOST_TRUE;
            fprintf(stdout, "%d-%d-%dT%02d:%02d:%02dZ, DATA, %d\n", year, month, day, hours, minutes, seconds, acket.thissn);
            //printf("We're getting lost!\n");
        }

        //Receive Data
        if (packetLost == PACKET_LOST_FALSE) {
            if (packet.thissn == 0) {
                //printf("New client!\n");
                strcpy(files[totalClients], packet.path);
                clearFile = CLEAR;
                currentClient = totalClients;
                totalClients++;
                packetFound = PACKET_FOUND_TRUE;
            }
            else {
                //printf("Returning client!\n");
                for (int i = 0; i < totalClients; i++) {
                    if (strcmp(packet.path, files[i]) == 0) {
                        if (packet.thissn == nextSn[i]) {
                            currentClient = i;
                            packetFound = PACKET_FOUND_TRUE;
                        }
                        else if (packet.thissn != nextSn[i]) {
                            duplicateFlag = DUPLICATES_FOUND;
                            currentClient = i;
                            packetFound = PACKET_FOUND_TRUE;
                        }
                    }
                }
            }
        }

        //Write
        if ((packetFound == PACKET_FOUND_TRUE) && (packetLost == PACKET_LOST_FALSE)) {
            //eternalTimer++;
            if (duplicateFlag == NO_DUPLICATES) {
                if (clearFile == APPEND) {
                    //printf("We're appending!\n");
                    snprintf(outFileString, MAX_FILE_PATH, "%s", packet.path);
                    outputFile = fopen(outFileString, "a");
                }
                else if (clearFile == CLEAR) {
                    //printf("We're writing!\n");
                    snprintf(outFileString, MAX_FILE_PATH, "%s", packet.path);
                    outputFile = fopen(outFileString, "w");
                }
                for (int i = 0; i < packet.strLength; i++) {
                    //printf("%c", packet.data[i]);
                    fputc(packet.data[i], outputFile);
                    //if (packet.data[i] == 'A') {
                    //    printf("SECRET PATH: thissn %d, nextsn %d, nextSn %d\n", acket.thissn, acket.nextsn, nextSn[currentClient]);
                    //}
                }
                fclose(outputFile);
                nextSn[currentClient]++;
            }
            else {
                //printf("We're duplicating!\n");
            }
        }

        //Build
        if ((packetFound == PACKET_FOUND_TRUE) && (packetLost == PACKET_LOST_FALSE)) {
            //printf("We're building!\n");
            acket.index = packet.index;
            acket.thissn = packet.thissn;
            acket.nextsn = packet.nextsn;
            
            //weNeedtoUnACK = 1;
            //printf("thissn %d, nextsn %d, nextSn %d\n", acket.thissn, acket.nextsn, nextSn[currentClient]);
        }

        //Drop ACK
        if ((packetFound == PACKET_FOUND_TRUE) && (packetLost == PACKET_LOST_FALSE)) {
            //Drop ACK
            if (dropChance > (rand() % 100)) {
                //Drop
                packetLost = PACKET_LOST_TRUE;
                fprintf(stdout, "%d-%d-%dT%02d:%02d:%02dZ, ACK, %d\n", year, month, day, hours, minutes, seconds, acket.thissn);
                //printf("We'll never be found!\n");
                if (duplicateFlag == DUPLICATES_FOUND) {
                    nextSn[currentClient]--;
                    //weNeedtoUnACK = 0;
                }
            }
        }

        //Send
        if ((packetFound == PACKET_FOUND_TRUE) && (packetLost == PACKET_LOST_FALSE)) {
            //printf("We're sending!\n");
            sendto(sockfd, &acket, sizeof(acket), 0, PCLIADDR, CLILEN);
        }

        duplicateFlag = NO_DUPLICATES;
        currentClient = 0;
        packetFound = PACKET_FOUND_FALSE;
        packetLost = PACKET_LOST_FALSE;
        clearFile = APPEND;
    }


    //dg_echo(sockfd, (SA *) &cliaddr, sizeof(cliaddr));
}