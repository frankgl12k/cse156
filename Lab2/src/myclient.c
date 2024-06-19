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
#define IN_FILE_PATH argv[4]
#define OUT_FILE_PATH argv[5]
#define NUM_ARGS argc
#define PSERVADDR (SA *) &servaddr
#define SERVLEN sizeof(servaddr)
#define DMTU_LOOP dMTU
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
#define TIMEOUT_CHECK if (n < 0) { \
    if (errno == EINTR) { \
        fprintf(stderr, "Unknown error occured. Check IP and Ports.\n"); \
        fclose(inputFile); \
        fclose(outputFile); \
        close(sockfd); \
        exit(1);  \
    } else if (errno == EWOULDBLOCK) { \
        fprintf(stderr, "Cannot detect server\n"); \
        fclose(inputFile); \
        fclose(outputFile); \
        close(sockfd); \
        exit(1); \
    } else { \
    fprintf(stderr, "recvfrom error\n"); \
    fclose(inputFile); \
    fclose(outputFile); \
    close(sockfd); \
    exit(1); \
    } \
}

void dg_cli(FILE *fp, int sockfd, const SA *pservaddr, socklen_t servlen, FILE *outputFile) {
    int n;
    char sendline[MAXLINE], recvline[MAXLINE + 1];

    while (fgets(sendline, MAXLINE, fp) != NULL) {
        sendto(sockfd, sendline, strlen(sendline), 0, pservaddr, servlen);

        n = recvfrom(sockfd, recvline, MAXLINE, 0, NULL, NULL);

        recvline[n] = 0; /* null terminate */
        
        fputs(recvline, outputFile);
    }
}

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

int main(int argc, char **argv) {
    int sockfd;
    struct sockaddr_in servaddr;
    FILE *inputFile;
    FILE *outputFile;
    int trueServerPort = 9877;

    struct timeval tv;
    tv.tv_sec = 60;
    tv.tv_usec = 0;
    if(argc != 6) {
        fprintf(stderr,"Not enough/too many initial parameters.\n");
        exit(1);
    }

    int n;
    int dMTU = atoi(MTU);
    if (dMTU > 32768) {
        dMTU = 32768;
    }
    if (dMTU <= 1) {
        fprintf(stderr, "The MTU is invalid!\n");
        exit(1);
    }
    char packet1[dMTU];
    char packet2[dMTU];
    char packet3[dMTU];
    char packet4[dMTU];
    char packet5[dMTU];
    char packet6[dMTU];
    char packet7[dMTU];
    char packet8[dMTU];
    char packetf[dMTU];
    const char packetSorter[9] = {'1','2','3','4','5','6','7','8','\0'};
    //int dMTU = atoi(MTU);
    char sendline[MAXLINE], recvline[MAXLINE + 1];
    char inFileString[1024];
    char outFileString[1024];
    int64_t fileSize;
    int sendIndex = 0;
    int nextChar = 0;
    int endOfFileFlag = EOF_NOT_REACHED;
    int currentSend = 0;
    int currentReceive = 0;
    int transferMode = 0;
    int finalLength = 0;
    int machineState = FALSE_END;

    //printf("%s and %s and %s and %s and %s\n", SERVER_IP, SERVER_PORT, MTU, IN_FILE_PATH, OUT_FILE_PATH);
    //printf("%d\n", sizeof(packet1));
    

    trueServerPort = atoi(SERVER_PORT);
    //printf("client port: %d\n", trueServerPort);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(trueServerPort);
    inet_pton(AF_INET, SERVER_IP, &servaddr.sin_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    snprintf(inFileString, 1024, "%s", IN_FILE_PATH);
    snprintf(outFileString, 1024, "%s", OUT_FILE_PATH);
    inputFile = fopen(inFileString, "r");
    outputFile = fopen(outFileString, "w");
    fseek(inputFile, 0, SEEK_END);
    fileSize = ftell(inputFile);
    //printf("File Size: %d bytes\n", fileSize);
    fseek(inputFile, 0, SEEK_SET);
    //if (fileSize == 0) {
    //    fclose(inputFile);
    //    fclose(outputFile);
    //    exit(0);
    //    //Success! Empty file created.
    //}
    //dg_cli(inputFile, sockfd, PSERVADDR, SERVLEN, outputFile);
    int currentIndex = 0;
    while (machineState == FALSE_END) {
        //Looking for edge cases.
        //Edge case 1: Less than 8 packets left.
        //Adding 1 for safety.
        if (transferMode == BUILD_MODE) {
            if ((fileSize - sendIndex) < (((dMTU-1) * 8) + 1)) {
                //Switch to only sending 1 packet.
                currentIndex = 0;
                packetf[0] = '0';
                currentIndex = 1;
                while (currentIndex < dMTU) {
                    nextChar = fgetc(inputFile);
                    packetf[currentIndex] = nextChar;
                    currentIndex++;
                    sendIndex++;
                    if (feof(inputFile) != 0) {
                        endOfFileFlag = EOF_REACHED;
                        finalLength = currentIndex;
                        if (currentIndex < dMTU) {
                            packetf[currentIndex] = '\0';
                        }
                        transferMode = MUTATION_MODE;
                        break;
                    }
                    transferMode = MUTATION_MODE;
                    endOfFileFlag = EOF_SEEKING;
                }
            }
            else {
            //Standard case: 8 packets to send.
            //Count to 8, then stop sending packets. Wait until receive clears to send.
                currentIndex = 0;
                packet1[0] = '1';
                currentIndex = 1;
                while (currentIndex < DMTU_LOOP) {
                    nextChar = fgetc(inputFile);
                    packet1[currentIndex] = nextChar;
                    currentIndex++;
                    sendIndex++;
                }
                currentIndex = 0;
                packet2[0] = '2';
                currentIndex = 1;
                while (currentIndex < DMTU_LOOP) {
                    nextChar = fgetc(inputFile);
                    packet2[currentIndex] = nextChar;
                    currentIndex++;
                    sendIndex++;
                }
                currentIndex = 0;
                packet3[0] = '3';
                currentIndex = 1;
                while (currentIndex < DMTU_LOOP) {
                    nextChar = fgetc(inputFile);
                    packet3[currentIndex] = nextChar;
                    currentIndex++;
                    sendIndex++;
                }
                currentIndex = 0;
                packet4[0] = '4';
                currentIndex = 1;
                while (currentIndex < DMTU_LOOP) {
                    nextChar = fgetc(inputFile);
                    packet4[currentIndex] = nextChar;
                    currentIndex++;
                    sendIndex++;
                }
                currentIndex = 0;
                packet5[0] = '5';
                currentIndex = 1;
                while (currentIndex < DMTU_LOOP) {
                    nextChar = fgetc(inputFile);
                    packet5[currentIndex] = nextChar;
                    currentIndex++;
                    sendIndex++;
                }
                currentIndex = 0;
                packet6[0] = '6';
                currentIndex = 1;
                while (currentIndex < DMTU_LOOP) {
                    nextChar = fgetc(inputFile);
                    packet6[currentIndex] = nextChar;
                    currentIndex++;
                    sendIndex++;
                }
                currentIndex = 0;
                packet7[0] = '7';
                currentIndex = 1;
                while (currentIndex < DMTU_LOOP) {
                    nextChar = fgetc(inputFile);
                    packet7[currentIndex] = nextChar;
                    currentIndex++;
                    sendIndex++;
                }
                currentIndex = 0;
                packet8[0] = '8';
                currentIndex = 1;
                while (currentIndex < DMTU_LOOP) {
                    nextChar = fgetc(inputFile);
                    packet8[currentIndex] = nextChar;
                    currentIndex++;
                    sendIndex++;
                }
                currentSend = 1;
                transferMode = MUTATION_MODE;
            }
        }

        ///fread(sendline, 1, dMTU, inputFile);
        sendIndex++;
        //Edge case 2: End of File reached.
        //if feof 
        //  send current packet (Due to edge case 1, this will ALWAYS be a single packet round.)
        //  set a flag to kill the while loop (or set a flag to kill all the send stuff, just in case it needs to receive still)
        //Edge case 1: Only one being sent.
        if (transferMode == MUTATION_MODE) {
            if (endOfFileFlag == EOF_REACHED) {
                sendto(sockfd, packetf, finalLength, 0, PSERVADDR, SERVLEN);
                currentReceive = 9;
                transferMode = MUTATION_MODE;

            }
            else if (endOfFileFlag == EOF_SEEKING) {
                sendto(sockfd, packetf, dMTU, 0, PSERVADDR, SERVLEN);
                currentReceive = 10;
                transferMode = MUTATION_MODE;
            }
            else {
                switch(currentSend) {
                    case 1:
                        sendto(sockfd, packet1, dMTU, 0, PSERVADDR, SERVLEN);
                        currentSend++;
                        currentReceive = 1;
                        break;
                    case 2:
                        sendto(sockfd, packet2, dMTU, 0, PSERVADDR, SERVLEN);
                        currentSend++;
                        currentReceive++;
                        break;
                    case 3:
                        sendto(sockfd, packet3, dMTU, 0, PSERVADDR, SERVLEN);
                        currentSend++;
                        currentReceive++;
                        break;
                    case 4:
                        sendto(sockfd, packet4, dMTU, 0, PSERVADDR, SERVLEN);
                        currentSend++;
                        currentReceive++;
                        break;
                    case 5:
                        sendto(sockfd, packet5, dMTU, 0, PSERVADDR, SERVLEN);
                        currentSend++;
                        currentReceive++;
                        break;
                    case 6:
                        sendto(sockfd, packet6, dMTU, 0, PSERVADDR, SERVLEN);
                        currentSend++;
                        currentReceive++;
                        break;
                    case 7:
                        sendto(sockfd, packet7, dMTU, 0, PSERVADDR, SERVLEN);
                        currentSend++;
                        currentReceive++;
                        break;
                    case 8:
                        sendto(sockfd, packet8, dMTU, 0, PSERVADDR, SERVLEN);
                        currentSend++;
                        currentReceive = 8;
                        //transferMode = MUTATION_MODE;
                        break;
                    default:
                        transferMode = MUTATION_MODE;
                }
            }

            switch(currentReceive) {
                case 1:
                    n = recvfrom(sockfd, packet1, dMTU, 0, NULL, NULL);
                    TIMEOUT_CHECK
                    break;
                case 2:
                    n = recvfrom(sockfd, packet2, dMTU, 0, NULL, NULL);
                    TIMEOUT_CHECK
                    break;
                case 3:
                    n = recvfrom(sockfd, packet3, dMTU, 0, NULL, NULL);
                    TIMEOUT_CHECK
                    break;
                case 4:
                    n = recvfrom(sockfd, packet4, dMTU, 0, NULL, NULL);
                    TIMEOUT_CHECK
                    break;
                case 5:
                    n = recvfrom(sockfd, packet5, dMTU, 0, NULL, NULL);
                    TIMEOUT_CHECK
                    break;
                case 6:
                    n = recvfrom(sockfd, packet6, dMTU, 0, NULL, NULL);
                    TIMEOUT_CHECK
                    break;
                case 7:
                    n = recvfrom(sockfd, packet7, dMTU, 0, NULL, NULL);
                    TIMEOUT_CHECK
                    break;
                case 8:
                    n = recvfrom(sockfd, packet8, dMTU, 0, NULL, NULL);
                    TIMEOUT_CHECK
                    transferMode = ORDER_MODE;
                    break;
                case 9:
                    n = recvfrom(sockfd, packetf, finalLength, 0, NULL, NULL);
                    TIMEOUT_CHECK
                    transferMode = FINAL_MODE;
                    break;
                case 10:
                    n = recvfrom(sockfd, packetf, dMTU, 0, NULL, NULL);
                    TIMEOUT_CHECK
                    transferMode = SINGLE_MODE;
                    break;
                default:
                    currentSend = 1;
                    currentReceive = 0;
            }
        }
        
        if (transferMode == ORDER_MODE) {
            int reOrderScope = 0;
            int currentScope = 0;
            while (reOrderScope < 8) {
                currentScope = reorderReturner(packet1, packet2, packet3, packet4, packet5, packet6, packet7, packet8, dMTU, packetSorter[reOrderScope]);
                switch(currentScope) {
                    case 1:
                        for (int x = 1; x < dMTU; x++) {
                            fputc(packet1[x], outputFile);
                        }
                        break;
                    case 2:
                        for (int x = 1; x < dMTU; x++) {
                            fputc(packet2[x], outputFile);
                        }
                        break;
                    case 3:
                        for (int x = 1; x < dMTU; x++) {
                            fputc(packet3[x], outputFile);
                        }
                        break;
                    case 4:
                        for (int x = 1; x < dMTU; x++) {
                            fputc(packet4[x], outputFile);
                        }
                        break;
                    case 5:
                        for (int x = 1; x < dMTU; x++) {
                            fputc(packet5[x], outputFile);
                        }
                        break;
                      case 6:
                        for (int x = 1; x < dMTU; x++) {
                            fputc(packet6[x], outputFile);
                        }
                        break;
                    case 7:
                        for (int x = 1; x < dMTU; x++) {
                            fputc(packet7[x], outputFile);
                        }
                        break;
                    case 8:
                        for (int x = 1; x < dMTU; x++) {
                            fputc(packet8[x], outputFile);
                        }
                        break;
                    default:
                        reOrderScope = 9;
                }
                reOrderScope++;
            }
            transferMode = BUILD_MODE;
        }
        else if (transferMode == SINGLE_MODE) {
            for (int x = 1; x < dMTU; x++) {
                fputc(packetf[x], outputFile);
            }
            transferMode = BUILD_MODE;
        }
        else if (transferMode == FINAL_MODE) {
            for (int x = 1; x < finalLength-1; x++) {
                fputc(packetf[x], outputFile);
            }
            machineState = TRUE_END;
        }
        //recvline[n] = 0; /* null terminate */
        //Standard case: 8 packets to receive. Hold the 8 packets in arrays 1-8 until all 8 have been received, then re-order.
        //  After completion, clear send to send.
        //Edge case 1: Less than 8 packets left. Just write the packet.
        //fwrite(recvline, 1, dMTU, outputFile);
        //printf("%s\n",recvline);
    }
    fclose(inputFile);
    fclose(outputFile);
    close(sockfd);
    exit(0);
}