#include <time.h>
#include <sys/time.h>

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
#include <errno.h>

#define	MAXLINE		4096
#define	LISTENQ		1024
#define	SERV_PORT		 9877
#define	SA	struct sockaddr
#define SERVER_IP argv[1]
#define SERVER_PORT argv[2]
#define MTU argv[3]
#define WINSZ argv[4]
#define IN_FILE_PATH argv[5]
#define OUT_FILE_PATH argv[6]
#define NUM_ARGS argc
#define PSERVADDR (SA *) &servaddr
#define SERVLEN sizeof(servaddr)
#define DMTU_LOOP dMTU

/*
#define EOF_NOT_REACHED 0
#define EOF_REACHED 1
#define EOF_SEEKING 2
#define BUILD_MODE 0
#define SEND_MODE 1
#define RECEIVE_MODE 2
#define MUTATION_MODE 3
#define ORDER_MODE 4
#define SINGLE_MODE 5
#define FINAL_MODE 6
#define FALSE_END 0
#define TRUE_END 1
*/
#define TIMEOUT_CHECK if (k < 0) { \
    if (errno == EWOULDBLOCK) { \
        fprintf(stderr, "Cannot detect server\n"); \
        fclose(inputFile); \
        close(sockfd); \
        exit(1); \
    } else { \
    fprintf(stderr, "Unknown error\n"); \
    fclose(inputFile); \
    close(sockfd); \
    exit(1); \
    } \
}
#define EOF_SEEKING 1
#define EOF_FOUND 0
#define TRUE_NEXT_BUILD nextBuild - 1
#define dWINSN baseSn + dWINSZ
#define MAX_FILE_PATH 128
#define DATA_SIZE atoi(MTU) - MAX_FILE_PATH - sizeof(int) - sizeof(uint32_t) - sizeof(uint32_t) - sizeof(int)
#define NOT_SEEKING_RETRANSMIT 0
#define SEEKING_RETRANSMIT 1
#define LOOKING_FOR_SERVER 0
#define SERVER_FOUND 1

/*
int reorderReturner(char array1[], char array2[], char array3[], char array4[], char array5[], char array6[], char array7[], char array8[], int arraySize, char nextArray) {
    if (array1[0] == nextArray) {
        return 1;
    }
    if (array2[0] == nextArray) {
        return 2;
    }
    if (array3[0] == nextArray) {
        return 3;
    }
    if (array4[0] == nextArray) {
        return 4;
    }
    if (array5[0] == nextArray) {
        return 5;
    }
    if (array6[0] == nextArray) {
        return 6;
    }
    if (array7[0] == nextArray) {
        return 7;
    }
    if (array8[0] == nextArray) {
        return 8;
    }
}
*/

int buildPacket(char* packetData, int dataSize, FILE *inputFile, int strLength) {
    int packetIndex = 0;
    int nextChar;
    int returnFlag = 1;
    while (packetIndex < dataSize) {
        nextChar = fgetc(inputFile);
        packetData[packetIndex] = nextChar;
        packetIndex++;
        strLength = packetIndex;
        if (feof(inputFile) != 0) {
            returnFlag = 0;
            if (packetIndex < dataSize) {
                packetData[packetIndex] = '\0';
            }
            break;
        }
    }
    return returnFlag;
}

int main(int argc, char **argv) {
    typedef struct pkt {
        int index;
        uint32_t thissn;
        uint32_t nextsn;
        int strLength;
        char path[MAX_FILE_PATH];
        char data[DATA_SIZE];
    } pkt;
    typedef struct ack {
        uint32_t thissn;
        uint32_t nextsn;
        int index;
    } ack;
    int sockfd;
    struct sockaddr_in servaddr;
    FILE *inputFile;
    char inFileString[1024];
    int trueServerPort = 9877;

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    if(argc != 7) {
        fprintf(stderr,"Not enough/too many initial parameters.\n");
        exit(1);
    }

    int n;
    int dMTU = atoi(MTU);
    if (dMTU > 32768) {
        dMTU = 32768;
    }
    if (dMTU < (MAX_FILE_PATH + sizeof(int) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int))) {
        fprintf(stderr, "The MTU is invalid!\n");
        exit(1);
    }
    int dWINSZ = atoi(WINSZ);
    if (dWINSZ > 1024) {
        dWINSZ = 1024;
    }
    if (dWINSZ <= 1) {
        fprintf(stderr, "The WINSZ is invalid!\n");
        exit(1);
    }
    if (DATA_SIZE <= 4) {
        fprintf(stderr, "The MTU is too small!\n");
        exit(1);
    }
    //pkt* packet = malloc(dWINSZ * sizeof(*packet));
    pkt packet[atoi(WINSZ)];
    for (int arraySetup = 0; arraySetup < dWINSZ; arraySetup++) {
        packet[arraySetup].index = arraySetup;
        packet[arraySetup].strLength = 0;
        packet[arraySetup].thissn = 0;
        packet[arraySetup].nextsn = 0;
        //packet[arraySetup].path = malloc(sizeof(OUT_FILE_PATH)+1);
        strcpy(packet[arraySetup].path, OUT_FILE_PATH);
        //packet[arraySetup].data = malloc(sizeof(char)*(DATA_SIZE)+1);
    }
    ack acket;
    acket.thissn = 128;
    trueServerPort = atoi(SERVER_PORT);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(trueServerPort);
    inet_pton(AF_INET, SERVER_IP, &servaddr.sin_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    

    uint32_t baseSn = 0;
    uint32_t nextSn = 1;
    uint32_t thisSn = 0;
    uint32_t winSn = baseSn + dWINSZ;
    uint32_t maxSn = 0;
    int64_t ackSn = -1;
    uint32_t lastmaxSn = -1;
    int timeouts = 0;
    int tempTimeoutFlag = 0;
    int tempAckFlag = 0;
    int eofFlag = EOF_SEEKING;
    int builtPackets = 0;
    int nextBuild = 0;
    int k = 0;
    int finalPacket = dWINSZ+2;
    //time_t retransmitStart, retransmitEnd;
    //int seekingNewTransmit = NOT_SEEKING_RETRANSMIT;
    int serverTimeout = LOOKING_FOR_SERVER;
    int fatalTimeouts = 0;

    snprintf(inFileString, 1024, "%s", IN_FILE_PATH);
    inputFile = fopen(inFileString, "r");
    int hours, minutes, seconds, day, month, year;
    time_t now;
    time(&now);
    struct tm *tx = gmtime(&now); 
    hours = tx ->tm_hour;         
    minutes = tx ->tm_min;        
    seconds = tx ->tm_sec;      
    day = tx ->tm_mday;            
    month = tx ->tm_mon + 1;     
    year = tx ->tm_year + 1900; 
    while (ackSn < maxSn) {
        //printf("We starting now.\n");
        //Build Mode
        if ((builtPackets < dWINSZ) && (eofFlag != EOF_FOUND)) {
            //buildPacket(packet[nextBuild].data, DATA_SIZE, inputFile, packet[nextBuild].strLength);
            //printf("Building in! %d \n", builtPackets);
            //printf("Next building! %d \n", nextBuild);
            //\\if (nextBuild >= dWINSZ) {
            //    printf("Strange world.\n");
            //    nextBuild = nextBuild % dWINSZ;
            //}
            int packetIndex = 0;
            int nextChar;
            //int returnFlag = 1;
            for (int i = 0; i < DATA_SIZE; i++) {
                packet[nextBuild].data[i] = '\0';
            }
            while (packetIndex < DATA_SIZE) {
                nextChar = fgetc(inputFile);
                packet[nextBuild].data[packetIndex] = nextChar;
                //printf("%c", packet[nextBuild].data[packetIndex]);
                packetIndex++;
                
                if (feof(inputFile) != 0) {
                    //returnFlag = 0;
                    eofFlag = EOF_FOUND;
                    finalPacket = nextBuild;
                    //printf("I refuse!\n");
                    //exit(1);
                    //if (packetIndex < DATA_SIZE) {
                    //    packet[nextBuild].data[packetIndex] = '\0';
                    //}
                    break;
                }
                packet[nextBuild].strLength = packetIndex;
            }
            builtPackets++;
            packet[nextBuild].index = nextBuild;
            
            //printf("Last built! %d \n", nextBuild);
            nextBuild++;
            //if (nextBuild >= dWINSZ) {
            //    printf("Bad world.\n");
            //    nextBuild = nextBuild % dWINSZ;
            //}
            //printf("Building! %d \n", builtPackets);
            //printf("Next built! %d \n", nextBuild);
        }

        //printf("We sending now.\n");
        //Transmit Mode
        if (thisSn <= maxSn) {
            packet[thisSn % dWINSZ].thissn = thisSn;
            packet[thisSn % dWINSZ].nextsn = nextSn;
            sendto(sockfd, &packet[thisSn % dWINSZ], sizeof(packet[thisSn % dWINSZ]), 0, PSERVADDR, SERVLEN);
            fprintf(stdout, "%d-%d-%dT%02d:%02d:%02dZ, DATA, %d, %d, %d, %d\n", year, month, day, hours, minutes, seconds, thisSn, baseSn, nextSn, dWINSN);
            if (maxSn < dWINSN) {
                if ((eofFlag != EOF_FOUND) || (packet[thisSn % dWINSZ].index != finalPacket)) {
                    maxSn++;
                }
                else if ((eofFlag == EOF_FOUND) && (packet[thisSn % dWINSZ].index < (finalPacket))) {
                    maxSn++;
                }
            }
            //if (seekingNewTransmit != SEEKING_RETRANSMIT) {
            //   seekingNewTransmit = SEEKING_RETRANSMIT;
            //    printf("HOPE SEEKING\n");
            //    time(&retransmitStart);
            //}
            thisSn++;
            nextSn++;
            //printf("This sequence! %d \n", thisSn);
            //Data line here

        }

        //Timeout Mode
        /*
        time(&retransmitEnd);
        if (difftime(retransmitStart, retransmitEnd) >= 5) {
            printf("DESPAIR SEEKING\n");
            thisSn = baseSn;
            timeouts++;
            if (timeouts == 3) {
                fprintf(stderr, "Retransmission failed, closing client.\n");
                fclose(inputFile);
                exit(1);
            }
        }
        */
        
        //printf("Box 16 for you?\n");
        //Receive Mode
        if ((builtPackets == dWINSZ) || (maxSn == lastmaxSn)) {
            k = recvfrom(sockfd, &acket, sizeof(acket), 0, NULL, NULL);
            if (k < 0) {
                if (errno == EWOULDBLOCK) {
                    if (serverTimeout == LOOKING_FOR_SERVER) {
                        fatalTimeouts++;
                        if (fatalTimeouts > 5) {
                            fprintf(stderr, "Cannot detect server\n");
                            fclose(inputFile);
                            close(sockfd);
                            exit(1);
                        }
                    } else if (serverTimeout == SERVER_FOUND) {
                        timeouts++;
                        //printf("DESPAIR SEEKING\n");
                        thisSn = baseSn;
                        if (timeouts > 5) {
                            fprintf(stderr, "Reached max re-transmission limit\n");
                            fclose(inputFile);
                            close(sockfd);
                            exit(1);
                        }
                        else {
                            thisSn = baseSn;
                            fprintf(stderr, "Packet loss detected\n");
                        }
                    }
                } else {
                    fprintf(stderr, "Unknown error\n");
                    fclose(inputFile);
                    close(sockfd);
                    exit(1);
                }
            }
            else {
                fatalTimeouts = 0;
            }
            //printf("Dying here?\n");
            if (acket.thissn == baseSn) {
                serverTimeout = SERVER_FOUND;
                //printf("Looking forwards?\n");
                //Ack line here
                fprintf(stdout, "%d-%d-%dT%02d:%02d:%02dZ, ACK, %d, %d, %d, %d\n", year, month, day, hours, minutes, seconds, acket.thissn, baseSn, acket.nextsn, dWINSN);
                nextBuild = acket.index % dWINSZ;
                //printf("basesn %d, thissn %d, nextsn %d, acksn %d, maxsn %d, winSn %d, nextBuild %d\n", baseSn, thisSn, nextSn, ackSn, maxSn, winSn, nextBuild);
                baseSn++;
                ackSn++;
                builtPackets--;
                
                //if (nextBuild >= dWINSZ) {
                //    printf("Dark world.\n");
                //    nextBuild = nextBuild % dWINSZ;
                //}
                //printf("Looking onwards?\n");
                winSn = baseSn + dWINSZ;
                //nextSn++;
                timeouts = 0;
                //seekingNewTransmit = NOT_SEEKING_RETRANSMIT;
                //time(&retransmitStart);
                //printf("Looking inwards?\n");
                
            }
            //printf("Is the seg here?\n");
        }
        lastmaxSn = maxSn;
    }
   fclose(inputFile);
    close(sockfd);
    exit(0);
}