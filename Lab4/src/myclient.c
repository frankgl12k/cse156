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
#include <pthread.h>

#define	MAXLINE		4096
#define	LISTENQ		1024
#define	SERV_PORT		 9877
#define	SA	struct sockaddr
#define P_SERVER_IP argv[1]
#define P_SERVER_PORT argv[2]
#define P_MTU argv[3]
#define P_WINSZ argv[4]
#define P_IN_FILE_PATH argv[5]
#define P_OUT_FILE_PATH argv[6]
#define SERVER_IP argv.ip
#define SERVER_PORT argv.port
#define MTU argv.mtu
#define WINSZ argv.winsz
#define IN_FILE_PATH argv.in_file_path
#define OUT_FILE_PATH argv.out_file_path
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
        return (void*)1; \
    } else { \
    fprintf(stderr, "Unknown error\n"); \
    fclose(inputFile); \
    close(sockfd); \
    return (void*)1; \
    } \
}
#define EOF_SEEKING 1
#define EOF_FOUND 0
#define TRUE_NEXT_BUILD nextBuild - 1
#define dWINSN baseSn + dWINSZ
#define MAX_FILE_PATH 256
#define MAX_FOLDERS 32
#define DATA_SIZE atoi(MTU) - MAX_FILE_PATH - MAX_FOLDERS - sizeof(int) - sizeof(uint32_t) - sizeof(uint32_t) - sizeof(int) - sizeof(int) - sizeof(int)
#define NOT_SEEKING_RETRANSMIT 0
#define SEEKING_RETRANSMIT 1
#define LOOKING_FOR_SERVER 0
#define SERVER_FOUND 1
#define NOT_FINAL_PACKET 0
#define FINAL_PACKET 1
#define FINAL_ACKET_UNRECEIVED 0
#define FINAL_ACKET_RECIEVED 1
#define NOT_FINAL_ACKET 0
#define FINAL_ACKET 1
#define MAX_CLIENTS 16
#define MAX_SERVER_PATH 20

#define UPDATE_TIME time(&now);\
    struct tm *tx = gmtime(&now);\
    hours = tx ->tm_hour;\
    minutes = tx ->tm_min;\
    seconds = tx ->tm_sec;\
    day = tx ->tm_mday;\
    month = tx ->tm_mon + 1;\
    year = tx ->tm_year + 1900;\
    gettimeofday(&ms, NULL);\

struct threadArgs {
    char** argv;
};

void *main_thread(void *argv);

typedef struct trueArg {
    char ip[24];
    char port[8];
    char mtu[16];
    char winsz[16];
    char in_file_path[MAX_FILE_PATH];
    char out_file_path[MAX_FILE_PATH];
} trueArg;

int main(int argc, char **argv) {
    //pthread_t thread1, thread2;
    //int iret1, iret2;
    //char *otherArg[7];
    //otherArg[1] ="127.0.0.1";
    //otherArg[2] ="9091";
    //otherArg[3] = "1024";
    //otherArg[4] = "4";
    //otherArg[5] = "letters1.txt";
    //otherArg[6] = "gamma1/gamma2/gamma3.txt";
    //printf("%s\n", otherArg[3]);
    if(argc != 7) {
        fprintf(stderr,"Not enough/too many initial parameters.\n");
        exit(1);
    }
    if (atoi(argv[1]) <1) {
        fprintf(stderr,"Positive, non-zero number of servers are required.\n");
        exit(1);
    }

    const char whiteSpace[2] = " ";
    const char newLine[3] = "\n";
    char* lineToken;
    FILE *confFile;
    char confString[1024];
    snprintf(confString, 1024, "%s", argv[2]);
    confFile = fopen(confString, "r");
    
    
    char buffer[1024];
    char *b = buffer;
    size_t bufferSize = 1024;
    size_t lineLength;
    int badLine = 0;
    int serverFound = 0;
    int fileChar;
    char portServer[atoi(argv[1])][2][MAX_SERVER_PATH];
    for (int x = 0; x < atoi(argv[1]); x++) {
        lineLength = getline(&b, &bufferSize, confFile);
        //printf("%s",buffer);
        for (int y = 0; y < bufferSize; y++) {
            if (buffer[y] == '#') {
                badLine = 1;
                y = 1025;
            }
            if (buffer[y] == '\n') {
                y = 1025;
            }
        }
        //printf("Ping!\n");
        if (badLine == 0) {
            lineToken = strtok(buffer, whiteSpace);
            strcpy(portServer[x][0], lineToken);
            lineToken = strtok(NULL, whiteSpace);
            strcpy(portServer[x][1], lineToken);
            for (int T = 4; T < MAX_SERVER_PATH; T++) {
                portServer[x][1][T] = '\0';
            }
        }
        else {
            x--;
        }
        badLine = 0;
        for (int k = 0; k < 1024; k++) {
            buffer[k] = '\0';
        }
    }
    pthread_t thread[atoi(argv[1])];
    trueArg finalArg[atoi(argv[1])];
    for (int i = 0; i < atoi(argv[1]); i++) {
        //printf("Ping!\n");
        strncpy(finalArg[i].ip, portServer[i][0], 16);
        strncpy(finalArg[i].port, portServer[i][1], 5);
        strncpy(finalArg[i].mtu, argv[3], strlen(argv[3])+1);
        strncpy(finalArg[i].winsz, argv[4], strlen(argv[4])+1);
        strncpy(finalArg[i].in_file_path, argv[5], strlen(argv[5])+1);
        strncpy(finalArg[i].out_file_path, argv[6], strlen(argv[6])+1);
        //for (int j = 3; j < 7; j++) {
        //    strncpy(finalArg[i].mtu, argv[j], strlen(argv[j])+1);
        //}
        
    }
    /*
    for (int i = 0; i < atoi(argv[1]); i++) {
        printf("%s", finalArg[i].ip);
        printf("%s", finalArg[i].port);
        printf("%s", finalArg[i].mtu);
        printf("%s", finalArg[i].winsz);
        printf("%s", finalArg[i].in_file_path);
        printf("%s", finalArg[i].out_file_path);
        //printf("%s", finalArg[i].ip);
        printf("\n");
    }
    */
    //printf("%p\n", *argArray[0]);
    for (int i = 0; i < atoi(argv[1]); i++) {
        //char ** tempArg = *(argArray[i][1]);
        pthread_create(&thread[i], NULL, *main_thread, (void*) &finalArg[i]);
    }
    //iret1 = pthread_create( &thread1, NULL, *main_thread, (void*) argv);
    //iret2 = pthread_create( &thread2, NULL, *main_thread, (void*) otherArg);
    int exitWithFailure = 0;
    int errorCheck = 0;
    fclose(confFile);
    for (int j = 0; j < atoi(argv[1]); j++) {
        pthread_join(thread[j], (void*)&errorCheck);
        if ((int)errorCheck == -1) {
            exitWithFailure = 1;
        }
    }
    if (exitWithFailure == 1) {
        fprintf(stderr, "Exited with error!\n");
        exit(1);
    }
    //pthread_join(thread1, NULL);
    //pthread_join(thread2, NULL);

    exit(0);
}

void *main_thread(void *ptr) {
    //printf("%p\n", ptr);
    trueArg argv = *(trueArg*)ptr;
    //printf("Hi: %s", argv[1]);
    //printf("Hello %s\n", SERVER_IP);
    typedef struct pkt {
        int index;
        uint32_t thissn;
        uint32_t nextsn;
        int strLength;
        int finalPacketFlag;
        int folderNumber;
        int folderSplit[MAX_FOLDERS];
        char path[MAX_FILE_PATH];
        char data[DATA_SIZE];
    } pkt;
    
    typedef struct ack {
        uint32_t thissn;
        uint32_t nextsn;
        int index;
        int finalAcketFlag;
    } ack;
    int sockfd;
    struct sockaddr_in servaddr;
    
    FILE *inputFile;
    char inFileString[1024];
    int trueServerPort = 9877;
    
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    //if(argc != 7) {
    //    fprintf(stderr,"Not enough/too many initial parameters.\n");
    //    return (void*)1;
    //}

    int n;
    int dMTU = atoi(MTU);
    //printf("The MTU is %d \n", dMTU);
    if (dMTU > 32768) {
        dMTU = 32768;
    }
    if (dMTU < (MAX_FILE_PATH + sizeof(int) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(int))) {
        fprintf(stderr, "The MTU is invalid!\n");
        pthread_exit((void*)-1);;
    }
    int dWINSZ = atoi(WINSZ);
    if (dWINSZ > 1024) {
        dWINSZ = 1024;
    }
    if (dWINSZ <= 1) {
        fprintf(stderr, "The WINSZ is invalid!\n");
        pthread_exit((void*)-1);;
    }
    if (DATA_SIZE <= 4) {
        fprintf(stderr, "The MTU is too small!\n");
        pthread_exit((void*)-1);;
    }
    //pkt* packet = malloc(dWINSZ * sizeof(*packet));
    pkt packet[atoi(WINSZ)];
    int tempFolderHolder[MAX_FOLDERS];
    int folderLength = 0;
    int fileWidth = 0;
    if (IN_FILE_PATH[0] == '/') {
        fprintf(stderr, "Invalid in_file_path.\n");
        pthread_exit((void*)-1);;
    }
    if (OUT_FILE_PATH[0] == '/') {
        fprintf(stderr, "Invalid out_file_path.\n");
        pthread_exit((void*)-1);;
    }
    for (int folderSetup = 0; folderSetup <= strlen(OUT_FILE_PATH); folderSetup++) {
        
        if (OUT_FILE_PATH[folderSetup] == '/') {
            tempFolderHolder[fileWidth] = folderLength;
            fileWidth++;
            folderLength = -1;
            //printf("Found slash!\n");
        }
        else if (OUT_FILE_PATH[folderSetup] == '\0') {
            tempFolderHolder[fileWidth] = folderLength;
            //fileWidth++;
            //printf("Found end!\n");
            break;
        }
        folderLength++;
    }
    for (int arraySetup = 0; arraySetup < dWINSZ; arraySetup++) {
        packet[arraySetup].index = arraySetup;
        packet[arraySetup].strLength = 0;
        packet[arraySetup].thissn = 0;
        packet[arraySetup].nextsn = 0;
        packet[arraySetup].folderNumber = fileWidth;
        for (int folderarraySetup = 0; folderarraySetup <= fileWidth; folderarraySetup++) {
            packet[arraySetup].folderSplit[folderarraySetup] = tempFolderHolder[folderarraySetup];
            //printf("Ping! %d\n", packet[arraySetup].folderSplit[folderarraySetup]);
        }
        //packet[arraySetup].folderSplit[0] = 0;
        packet[arraySetup].finalPacketFlag = NOT_FINAL_PACKET;
        //packet[arraySetup].path = malloc(sizeof(OUT_FILE_PATH)+1);
        strcpy(packet[arraySetup].path, OUT_FILE_PATH);
        
        //packet[arraySetup].data = malloc(sizeof(char)*(DATA_SIZE)+1);
    }
    ack acket;
    acket.thissn = 128;
    acket.finalAcketFlag = NOT_FINAL_ACKET;
    trueServerPort = atoi(SERVER_PORT);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(trueServerPort);
    inet_pton(AF_INET, SERVER_IP, &servaddr.sin_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    struct sockaddr_in justgivemetheport;
    socklen_t socketLength = sizeof(justgivemetheport);

    int imTiredOfFlags = 0;
    uint32_t baseSn = 0;
    uint32_t nextSn = 1;
    uint32_t thisSn = 0;
    uint32_t winSn = baseSn + dWINSZ;
    uint32_t lastMaxSn = 0;
    uint32_t totalPacketsBuilt = 0;
    uint32_t maxSn = 0;
    int64_t ackSn = -1;
    uint32_t lastmaxSn = -1;
    int finalPacketFlag = 0;
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
    int finalAckStatus = FINAL_ACKET_UNRECEIVED;

    snprintf(inFileString, 1024, "%s", IN_FILE_PATH);
    inputFile = fopen(inFileString, "r");
    int hours, minutes, seconds, day, month, year;
    struct timeval ms;
    
    time_t now;
    uint16_t finalInt = 0;
    char serverIP[20] = {0};
    char serverPort[16] = {0};
    strcpy(serverPort, SERVER_PORT);
    strcpy(serverIP, SERVER_IP);
    
    while ((ackSn < maxSn) && (finalAckStatus == FINAL_ACKET_UNRECEIVED)) {
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
            totalPacketsBuilt++;
            while (packetIndex < DATA_SIZE) {
                nextChar = fgetc(inputFile);
                packet[nextBuild].data[packetIndex] = nextChar;
                //printf("%c", nextChar);
                
                //printf("%c", packet[nextBuild].data[packetIndex]);
                packetIndex++;
                //packet[nextBuild].finalPacketFlag = NOT_FINAL_PACKET;
                if (feof(inputFile) != 0) {
                    //returnFlag = 0;
                    eofFlag = EOF_FOUND;
                    finalPacket = nextBuild;
                    packet[nextBuild].finalPacketFlag = FINAL_PACKET;
                    //printf("\n-----FINAL PACKET NUMBER: %d-----\n", totalPacketsBuilt);
                    break;
                }
                packet[nextBuild].strLength = packetIndex;
            }
            builtPackets++;
            packet[nextBuild].index = nextBuild;
            
            //printf("Last built! %d \n", nextBuild);
            nextBuild++;
        }

        //Transmit Mode
        if ((thisSn <= maxSn) && (thisSn < totalPacketsBuilt)) {
            packet[thisSn % dWINSZ].thissn = thisSn;
            packet[thisSn % dWINSZ].nextsn = nextSn;
            sendto(sockfd, &packet[thisSn % dWINSZ], sizeof(packet[thisSn % dWINSZ]), 0, PSERVADDR, SERVLEN);
            if (imTiredOfFlags == 0) {
                imTiredOfFlags = 1;
                getsockname(sockfd, (struct sockaddr*)&justgivemetheport, &socketLength);
                finalInt = ntohs(justgivemetheport.sin_port);
            }
            UPDATE_TIME
            fprintf(stdout, "%d-%d-%dT%02d:%02d:%02d.%ldZ, %u, %s, %s, DATA,  %d, %d, %d, %d\n", year, month, day, hours, minutes, seconds, ms.tv_usec/1000, finalInt, serverIP, serverPort, thisSn, baseSn, nextSn, dWINSN);
            if (maxSn < dWINSN) {
                if ((eofFlag != EOF_FOUND) || (packet[thisSn % dWINSZ].index != finalPacket)) {
                    maxSn++;
                }
                else if ((eofFlag == EOF_FOUND) && (packet[thisSn % dWINSZ].index < (finalPacket))) {
                    maxSn++;
                }
            }
            thisSn++;
            if (nextSn <= maxSn) {
                nextSn++;
            }
        }
        
        //Receive Mode
        if ((builtPackets == dWINSZ) || (maxSn == lastmaxSn)) {
            k = recvfrom(sockfd, &acket, sizeof(acket), 0, NULL, NULL);
            if (k < 0) {
                if (errno == EWOULDBLOCK) {
                    if (serverTimeout == LOOKING_FOR_SERVER) {
                        fatalTimeouts++;
                        thisSn = baseSn;
                        nextSn = baseSn + 1;
                        if (fatalTimeouts > 5) {
                            fprintf(stderr, "Cannot detect server\n");
                            fclose(inputFile);
                            close(sockfd);
                            pthread_exit((void*)-1);;
                        }
                    } else if (serverTimeout == SERVER_FOUND) {
                        timeouts++;
                        //printf("DESPAIR SEEKING\n");
                        thisSn = baseSn;
                        if (timeouts > 5) {
                            fprintf(stderr, "Reached max re-transmission limit\n");
                            fclose(inputFile);
                            close(sockfd);
                            pthread_exit((void*)-1);;
                        }
                        else {
                            thisSn = baseSn;
                            nextSn = baseSn + 1;
                            //maxSn = lastmaxSn;
                            fprintf(stderr, "Packet loss detected\n");
                        }
                    }
                } else {
                    fprintf(stderr, "Unknown error\n");
                    fclose(inputFile);
                    close(sockfd);
                    pthread_exit((void*)-1);
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
                UPDATE_TIME
                fprintf(stdout, "%d-%d-%dT%02d:%02d:%02d.%ldZ, %u, %s, %s, ACK, %d, %d, %d, %d\n", year, month, day, hours, minutes, seconds, ms.tv_usec/1000, finalInt, serverIP, serverPort, thisSn, baseSn, nextSn, dWINSN);
                nextBuild = acket.index % dWINSZ;
                //printf("basesn %d, thissn %d, nextsn %d, acksn %d, maxsn %d, winSn %d, nextBuild %d\n", baseSn, thisSn, nextSn, ackSn, maxSn, winSn, nextBuild);
                baseSn++;
                ackSn++;
                builtPackets--;
                //printf("nextBuild: %d\n", nextBuild);
                //if (nextBuild >= dWINSZ) {
                //    printf("Dark world.\n");
                //    nextBuild = nextBuild % dWINSZ;
                //}
                //printf("Looking onwards?\n");
                winSn = baseSn + dWINSZ;
                //nextSn++;
                lastMaxSn = maxSn;
                timeouts = 0;
                if (acket.finalAcketFlag == FINAL_ACKET) {
                    finalAckStatus = FINAL_ACKET_RECIEVED;
                }
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
    //printf("\n-----Shutting down!-----\n");
    pthread_exit(0);
}