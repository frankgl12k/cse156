#include <time.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
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
#define ROOT_FOLDER_PATH argv[3]
#define NUM_ARGS argc
#define MAX_CLIENTS 64
#define MAX_FILE_PATH 256
#define MAX_FOLDERS 32
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
#define NOT_FINAL_PACKET 0
#define FINAL_PACKET 1
#define KEEP_WRITING 0
#define NEVER_WRITE_AGAIN 1
#define KEEP_GOING 0
#define PREPARE_FOR_SHUTDOWN 1
#define NOT_FINAL_ACKET 0
#define FINAL_ACKET 1

#define UPDATE_TIME time(&now);\
    struct tm *tx = gmtime(&now);\
    hours = tx ->tm_hour;\
    minutes = tx ->tm_min;\
    seconds = tx ->tm_sec;\
    day = tx ->tm_mday;\
    month = tx ->tm_mon + 1;\
    year = tx ->tm_year + 1900;\
    gettimeofday(&ms, NULL);\


typedef struct pkt {
    int index;
    uint32_t thissn;
    uint32_t nextsn;
    int strLength;
    int finalPacketFlag;
    int folderNumber;
    int folderSplit[MAX_FOLDERS];
    char path[MAX_FILE_PATH];
    char data[MAX_DATA_SIZE];
} pkt;

typedef struct ack {
    uint32_t thissn;
    uint32_t nextsn;
    int index;
    int finalAcketFlag;
} ack;


int main(int argc, char **argv){
    int sockfd;
    int trueServerPort = 9877;
    struct sockaddr_in servaddr, cliaddr;
    int dropChance = 0;
    

    trueServerPort = atoi(SERVER_PORT);
    if (argc != 4) {
        fprintf(stderr, "Incorrect initial arguments! Too many/not enough!\n");
        exit(1);
    }

    pkt packet;
    ack acket;
    acket.nextsn = 1;
    acket.finalAcketFlag = NOT_FINAL_ACKET;
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
    uint32_t nextSn[MAX_CLIENTS];
    int writingStatus[MAX_CLIENTS];
    int shutdownStatus[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        nextSn[i] = 1;
        writingStatus[i] = KEEP_WRITING;
        shutdownStatus[i] = KEEP_GOING;
    }
    int duplicateFlag = NO_DUPLICATES;
    int packetFound = PACKET_FOUND_FALSE;
    int packetLost = PACKET_LOST_FALSE;
    int clearFile = APPEND;
    char outFileString[MAX_FILE_PATH];
    char singlePathFolder[MAX_FILE_PATH/4];
    socklen_t cliLen;
    time_t t;
    srand((unsigned) time(&t));
    int64_t eternalTimer = 0;
    int weNeedtoUnACK = 0;
    int hours, minutes, seconds, day, month, year;
    struct timeval ms;
    

    //printf("%d\n",mkdir("testDir/", S_IRUSR | S_IWUSR | S_IXUSR)); Works
    //printf("%d\n", mkdir("/otherDir/", S_IRUSR | S_IWUSR | S_IXUSR)); Does not work
    //printf("%d\n",mkdir("/thirdDir", S_IRUSR | S_IWUSR | S_IXUSR)); Does not work
    if (chdir(ROOT_FOLDER_PATH) == -1) {
        if (mkdir(ROOT_FOLDER_PATH, 0777) == -1) {
            fprintf(stderr, "Invalid root folder path.\n");
            exit(1);
        }
        else {
            chdir(ROOT_FOLDER_PATH);
        }
    }
    time_t now;
    uint32_t lastSn = 128;

    while (1) {
        
        cliLen = sizeof(cliaddr);
        recvfrom(sockfd, &packet, sizeof(packet), 0, PCLIADDR, &cliLen);
        //Drop Data
        if (dropChance > (rand() % 100)) {
            //Drop
            packetLost = PACKET_LOST_TRUE;
            UPDATE_TIME
            fprintf(stdout, "%d-%d-%dT%02d:%02d:%02d.%ldZ, DATA, %d\n", year, month, day, hours, minutes, seconds, ms.tv_usec/1000, acket.thissn);
            //printf("We're getting lost!\n");
        }

        //Receive Data
        if (packetLost == PACKET_LOST_FALSE) {
            if (packet.thissn == 0) {
                //printf("New client!\n");
                for (int i = 0; i < totalClients; i++) {
                    if (strcmp(packet.path, files[i]) == 0) {
                        duplicateFlag = DUPLICATES_FOUND;
                    }
                }
                if (duplicateFlag == NO_DUPLICATES) {
                    strcpy(files[totalClients], packet.path);
                    clearFile = CLEAR;
                    currentClient = totalClients;
                    totalClients++;
                    packetFound = PACKET_FOUND_TRUE;
                    int totalPath = 0;
                    int layersDeep = 0;
                    for (int folderChecker = 0; folderChecker < packet.folderNumber; folderChecker++) {
                        for (int singleFolder = 0; singleFolder < packet.folderSplit[folderChecker]; singleFolder++) {
                            //printf("%d\n", singleFolder);
                            singlePathFolder[singleFolder] = packet.path[totalPath];
                            totalPath++;
                        }
                        totalPath++;
                        if (chdir(singlePathFolder) == -1) {
                            if (mkdir(singlePathFolder, 0777) == 0) {
                                chdir(singlePathFolder);
                            }
                        }
                        layersDeep++;
                        for (int i = 0; i < sizeof(singlePathFolder);i++) {
                            //printf("Hi\n");
                            singlePathFolder[i] = '\0';
                            //printf("Bye!\n");
                        }
                        //printf("Other way out!\n");
                    }
                    //printf("did we get here\n");
                    for (int j = 0; j < layersDeep; j++) {
                        chdir("..");
                    }
                    //printf("%d\n", nextSn[currentClient]);
                }
            }
            else {
                //printf("Returning client!\n");
                for (int i = 0; i < totalClients; i++) {
                    if (strcmp(packet.path, files[i]) == 0) {
                        //printf("uhh %d\n", packet.finalPacketFlag);
                        if ((packet.nextsn == nextSn[i]) && (packet.finalPacketFlag == NOT_FINAL_PACKET)) {
                            currentClient = i;
                            packetFound = PACKET_FOUND_TRUE;
                            acket.finalAcketFlag = NOT_FINAL_ACKET;
                            //printf("Soaring!\n");
                        }
                        else if ((packet.nextsn != nextSn[i]) && (packet.finalPacketFlag == NOT_FINAL_PACKET)) {
                            duplicateFlag = DUPLICATES_FOUND;
                            currentClient = i;
                            packetFound = PACKET_FOUND_TRUE;
                            acket.finalAcketFlag = NOT_FINAL_ACKET;
                            //printf("Hold on\n");
                        }
                        else if ((packet.nextsn == nextSn[i]) && (packet.finalPacketFlag == FINAL_PACKET)) {
                            currentClient = i;
                            packetFound = PACKET_FOUND_TRUE;
                            acket.finalAcketFlag = FINAL_ACKET;
                            shutdownStatus[i] = PREPARE_FOR_SHUTDOWN;
                            //printf("Cutting through!\n");
                        }
                        else if ((packet.nextsn != nextSn[i]) && (packet.finalPacketFlag == FINAL_PACKET)) {
                            duplicateFlag = DUPLICATES_FOUND;
                            currentClient = i;
                            acket.finalAcketFlag = FINAL_ACKET;
                            packetFound = PACKET_FOUND_TRUE;
                            //printf("No way\n");
                        }
                        else {
                            //printf("I should not exist.\n");
                        }
                    }
                }
            }
        }

        //Write
        if ((packetFound == PACKET_FOUND_TRUE) && (packetLost == PACKET_LOST_FALSE) && (writingStatus[currentClient] == KEEP_GOING)) {
            //eternalTimer++;
            if (duplicateFlag == NO_DUPLICATES) {
                if (clearFile == APPEND) {
                    //printf("We're appending! %d %d\n", packet.thissn, currentClient);
                    snprintf(outFileString, MAX_FILE_PATH, "%s", packet.path);
                    outputFile = fopen(outFileString, "a");
                }
                else if (clearFile == CLEAR) {
                    //printf("We're writing! %s\n", packet.path);
                    snprintf(outFileString, MAX_FILE_PATH, "%s", packet.path);
                    outputFile = fopen(outFileString, "w");
                }
                for (int i = 0; i < packet.strLength; i++) {
                    //printf("%c", packet.data[i]);
                    //printf("We in here?\n");
                    fputc(packet.data[i], outputFile);
                    //if (packet.data[i] == 'A') {
                    //    printf("SECRET PATH: thissn %d, nextsn %d, nextSn %d\n", acket.thissn, acket.nextsn, nextSn[currentClient]);
                    //}
                }
                //char buffer[24];
                //sprintf(buffer, "%d", packet.thissn);
                //fputc(buffer[0], outputFile);
                //if (packet.thissn > 9) {
                //    fputc(buffer[1], outputFile);
                //}
                fclose(outputFile);
                lastSn = nextSn[currentClient];
                nextSn[currentClient]++;
                if (shutdownStatus[currentClient] == PREPARE_FOR_SHUTDOWN) {
                    writingStatus[currentClient] = NEVER_WRITE_AGAIN;
                    //printf("hol up\n");
                }
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
                UPDATE_TIME
                fprintf(stdout, "%d-%d-%dT%02d:%02d:%02d.%ldZ, ACK, %d\n", year, month, day, hours, minutes, seconds, ms.tv_usec/1000, acket.thissn);
                //printf("We'll never be found!\n");
                if (duplicateFlag == DUPLICATES_FOUND) {
                    //nextSn[currentClient]--;
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
        acket.finalAcketFlag = NOT_FINAL_ACKET;
    }


    //dg_echo(sockfd, (SA *) &cliaddr, sizeof(cliaddr));
}