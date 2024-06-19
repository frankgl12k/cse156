#include <time.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
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
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <regex.h>
#include <netdb.h>




#define LISTEN_PORT argv[1]
#define FORBIDDEN_SITES argv[2]
#define ACCESS_LOG argv[3]
#define MAX_SERVER_PATH 1024
#define MAX_FILE_PATH 1024
#define MAX_COMMAND_LENGTH 16384
#define MAX_FOLDERS 32
#define	MAXLINE	4096
#define LISTENQ	1024
#define	SERV_PORT 9877
#define	SA	struct sockaddr
#define MAX_CLIENTS 100
#define forever for (; ;)


void *proxy(void *ptr);
void updateForbidden(int none);
typedef struct proxyPacket {
        int client;
        char forbiddenStringThread[MAX_FILE_PATH];
        char clientIP[16];
    } proxyPacket;

FILE *accessFile;
int FORBIDDEN_UPDATE = 1;
int GLOBAL_LAST_UPDATE = 0;
pthread_mutex_t forbiddenMTX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t logMTX = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t firstLogMTX = PTHREAD_MUTEX_INITIALIZER;
int firstLog = -1;
char forbiddenFileString[MAX_FILE_PATH];
char forbiddenFileCopy[65535];
uint16_t fileSize;


int main(int argc, char **argv) {
    

    if(argc != 4) {
        fprintf(stderr,"Not enough/too many initial parameters.\n");
        exit(1);
    }
    if (atoi(LISTEN_PORT) < 0) {
        fprintf(stderr,"Positive port required.\n");
        exit(1);
    }


    FILE *forbiddenFile;
    int trueListenPort = atoi(LISTEN_PORT);
    
    int tempForbiddenHolder[MAX_FOLDERS];
    int tempAccessHolder[MAX_FOLDERS];
    int folderLengthForbidden = 0;
    int folderLengthAccess = 0;
    int fileWidthForbidden = 0;
    int fileWidthAccess = 0;
    if (FORBIDDEN_SITES[0] == '/') {
        fprintf(stderr, "Invalid forbidden_sites_file_path 1.\n");
        exit(1);
    }
    if (ACCESS_LOG[0] == '/') {
        fprintf(stderr, "Invalid access_log_file_path.\n");
        exit(1);
    }
    for (int folderSetup = 0; folderSetup <= strlen(FORBIDDEN_SITES); folderSetup++) {
        
        if (FORBIDDEN_SITES[folderSetup] == '/') {
            tempForbiddenHolder[fileWidthForbidden] = folderLengthForbidden;
            fileWidthForbidden++;
            folderLengthForbidden = -1;
            //printf("Found slash!\n");
        }
        else if (FORBIDDEN_SITES[folderSetup] == '\0') {
            tempForbiddenHolder[fileWidthForbidden] = folderLengthForbidden;
            //fileWidth++;
            //printf("Found end!\n");
            break;
        }
        folderLengthForbidden++;
    }
    for (int folderSetup = 0; folderSetup <= strlen(ACCESS_LOG); folderSetup++) {
        
        if (ACCESS_LOG[folderSetup] == '/') {
            tempAccessHolder[fileWidthAccess] = folderLengthAccess;
            fileWidthAccess++;
            folderLengthAccess = -1;
            //printf("Found slash!\n");
        }
        else if (ACCESS_LOG[folderSetup] == '\0') {
            tempAccessHolder[fileWidthAccess] = folderLengthAccess;
            //fileWidth++;
            //printf("Found end!\n");
            break;
        }
        folderLengthAccess++;
    }
    int totalPath = 0;
    int layersDeep = 0;
    char singlePathFolder[MAX_FILE_PATH];
    //int absolutePathLength;
    for (int folderChecker = 0; folderChecker < fileWidthAccess; folderChecker++) {
        for (int singleFolder = 0; singleFolder < tempAccessHolder[folderChecker]; singleFolder++) {
            //printf("%d\n", singleFolder);
            singlePathFolder[totalPath] = ACCESS_LOG[totalPath];
            totalPath++;
        }
        totalPath++;
        mkdir(singlePathFolder, 0777);
        //if (chdir(singlePathFolder) == -1) {
        //    if (mkdir(singlePathFolder, 0777) == 0) {
        //        chdir(singlePathFolder);
        //    }
        //}
        layersDeep++;
        //memset(singlePathFolder, 0, MAX_FILE_PATH);
        //for (int i = 0; i < sizeof(singlePathFolder);i++) {
        //    //printf("Hi\n");
        //    singlePathFolder[i] = '\0';
        //    //printf("Bye!\n");
        //}
        //printf("Other way out!\n");
    }
    //printf("did we get here\n");
    //for (int j = 0; j < layersDeep; j++) {
    //    chdir("..");
    //}

    
    char accessFileString[MAX_FILE_PATH];
    


    snprintf(forbiddenFileString, MAX_FILE_PATH, "%s", FORBIDDEN_SITES);
    pthread_mutex_lock(&forbiddenMTX);
    forbiddenFile = fopen(forbiddenFileString, "r");
    if (forbiddenFile == NULL) {
        fprintf(stderr, "Invalid forbidden_sites_file_path 2.\n");
        exit(1);
    }
    fseek(forbiddenFile, 0, SEEK_END);
    fileSize = ftell(forbiddenFile);
    rewind(forbiddenFile);
    fclose(forbiddenFile);
    pthread_mutex_unlock(&forbiddenMTX);
    //fread

    pthread_mutex_lock(&logMTX);
    snprintf(accessFileString, MAX_FILE_PATH, "%s", ACCESS_LOG);
    accessFile = fopen(accessFileString, "a+");
    pthread_mutex_unlock(&logMTX);


    int listenfd;
    int connections[MAX_CLIENTS];
    pid_t childpid;
    socklen_t clilen;
    struct sockaddr_in cliaddr, servaddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    //servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(trueListenPort);

    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
    
    proxyPacket myProxy[MAX_CLIENTS];

    bind(listenfd, (SA *) &servaddr, sizeof(servaddr));
    listen(listenfd, LISTENQ);
    int connectNum = 0;
    pthread_t thread[MAX_CLIENTS];
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, updateForbidden);
    forever {
        clilen = sizeof(cliaddr);
        connections[connectNum] = accept(listenfd, (SA *) &cliaddr, &clilen);
        
        if (connections[connectNum] != -1) {
            myProxy[connectNum].client = connections[connectNum];
            strcpy(myProxy[connectNum].forbiddenStringThread, forbiddenFileString);
            struct sockaddr_in* miniAddress = (struct sockaddr_in*)&cliaddr;
            struct in_addr theIPAddr = miniAddress->sin_addr;
            char clientIP_Main[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &theIPAddr, clientIP_Main, INET_ADDRSTRLEN);
            strcpy(myProxy[connectNum].clientIP, clientIP_Main);
            //myProxy[connectNum].log = accessFile;
            pthread_create(&thread[connectNum], NULL, *proxy, (void*) &myProxy[connectNum]);
            pthread_detach(thread[connectNum]);
            //printf("-------------%d-----------\n", connectNum);
        }
        

        connectNum++;
        if (connectNum > MAX_CLIENTS) {
            connectNum = 0;
        }
    }
    // ---------FIX THIS LATER IT WILL ERROR OUT ------
    // ---------IT MIGHT NOT ERROR NOW ----------------
    //for (int t = 0; t < connectNum; t++) {
    //    close(connections[t]);
    //}
    exit(0);
}

void updateForbidden(int none) {
    FORBIDDEN_UPDATE++;
    //pthread_mutex_lock(&forbiddenMTX);
    //fclose(forbiddenFile);
    //forbiddenFile = fopen(forbiddenFileString, "r");
    //pthread_mutex_unlock(&forbiddenMTX);
}

#define NOT_IMPLEMENTED 0
#define GET_REQUEST 1
#define HEAD_REQUEST 2
#define MAX_WEBSITE_LENGTH 512
#define HOST_SEEKING 0
#define HOST_FOUND 1
#define HOST_COMPLETE 2
#define NO_PORT 0
#define PORT_FOUND 1
#define PORT_COMPLETE 2
#define PORT_DELAY 3
#define IP_ADDRESS addressList[0]->s_addr
#define PORT_FREE if (lookingForPort == PORT_COMPLETE) { \
                        free(portNum);\
                        }\

#define UPDATE_TIME time(&now);\
    struct tm *tx = gmtime(&now);\
    hours = tx ->tm_hour;\
    minutes = tx ->tm_min;\
    seconds = tx ->tm_sec;\
    day = tx ->tm_mday;\
    month = tx ->tm_mon + 1;\
    year = tx ->tm_year + 1900;\
    gettimeofday(&ms, NULL);\

#define HEADER firstLine
#define NO_ERRORS 0
#define EXIT_WITH_ERROR 1

#define ALL_GOOD 0
#define DEAD_CLIENT 1
#define NOT_IMPLEMENTED_STATUS 2
#define INVALID_HOSTNAME 3
#define IPV6_NOT_SUPPORTED 4
#define COULD_NOT_CREATE_CONTEXT 5
#define CANNOT_REACH_SERVER 6
#define CANNOT_REACH_SERVER_SSL 7
#define DEAD_SERVER_SSL 8
#define CANNOT_WRITE_SSL 9
#define CANNOT_READ_SSL_FIRST 10
#define CANNOT_READ_SSL_OTHER 11
#define FORBIDDEN_AT_HOST 12
#define FORBIDDEN_AT_SSL_WRITE 13
#define FORBIDDEN_AT_SSL_READ_FIRST 14
#define FORBIDDEN_AT_SSL_READ_OTHER 15
#define FORBIDDEN_AT_WRITE 16


#define IGNORE_IT 0
#define SHUTDOWN_SSL 1
#define FREE_SSL 1
#define CLOSE_SERVER 1
#define FREE_CTX 1
#define CLOSE_CLIENT 1
#define MEMSET_COMMAND 1
#define MEMSET_COMMAND2 1
#define MEMSET_ACCESS 1
#define FREE_FIRSTLINE 1
#define FREE_HOSTNAME 1

#define MAX_URL 2048
#define FILE_STRING myProxy.forbiddenStringThread

#define evaluateForbidden1(forbiddenMacroFlag) if (lastUpdate != FORBIDDEN_UPDATE) { \
        fclose(localForbiddenFile); \
        localForbiddenFile = fopen(FILE_STRING, "r"); \
        lastUpdate = FORBIDDEN_UPDATE; \
    } \
    rewind(localForbiddenFile); \
    memset(holdURL, 0, sizeof(holdURL)); \
    while (fscanf(localForbiddenFile, "%s", holdURL) != EOF) { \
        if (strcmp(hostName, holdURL) == 0) { \
            errorFlag = EXIT_WITH_ERROR; \
            errorStatus = forbiddenMacroFlag; \
            fprintf(stderr, "Forbidden site attempted access.\n", sizeof("Forbidden site attempted access.\n")); \
            goto finale; \
        } \
        if (strcmp(hostIP, holdURL) == 0) { \
            errorFlag = EXIT_WITH_ERROR; \
            errorStatus = forbiddenMacroFlag; \
            fprintf(stderr, "Forbidden site attempted access.\n", sizeof("Forbidden site attempted access.\n")); \
            goto finale; \
        } \
            if (lastUpdate != FORBIDDEN_UPDATE) { \
            fclose(localForbiddenFile); \
            localForbiddenFile = fopen(FILE_STRING, "r"); \
            lastUpdate = FORBIDDEN_UPDATE; \
            rewind(localForbiddenFile); \
        } \
    } \
    memset(holdURL, 0, sizeof(holdURL));

#define evaluateForbidden2(forbiddenMacroFlag) if (lastUpdate != FORBIDDEN_UPDATE) { \
        fprintf(stderr,"hey there\n"); \
        fclose(localForbiddenFile); \
        localForbiddenFile = fopen(FILE_STRING, "r"); \
        lastUpdate = FORBIDDEN_UPDATE; \
        fseek(localForbiddenFile, 0, SEEK_END);\
        fileSize = ftell(localForbiddenFile);\
        rewind(localForbiddenFile);\
        free(fileCopy);\
        char *fileCopy = malloc(fileSize+1);\
        memset(fileCopy, 0, sizeof(fileCopy));\
        fread(fileCopy, fileSize, 1, localForbiddenFile);\
        startingLine = fileCopy;\
    } \
    memset(holdURL2, 0, sizeof(holdURL2)); \
    startingLine = fileCopy;\
    while (sscanf(startingLine, "%s%n", holdURL2, &theLetterN) == 1) {\
        if (strcmp(hostName, holdURL2) == 0) { \
            errorFlag = EXIT_WITH_ERROR; \
            errorStatus = forbiddenMacroFlag; \
            fprintf(stderr, "Forbidden site attempted access.\n", sizeof("Forbidden site attempted access.\n")); \
            goto finale; \
        } \
        if (strcmp(hostIP, holdURL2) == 0) { \
            errorFlag = EXIT_WITH_ERROR; \
            errorStatus = forbiddenMacroFlag; \
            fprintf(stderr, "Forbidden site attempted access.\n", sizeof("Forbidden site attempted access.\n")); \
            goto finale; \
        } \
            startingLine += theLetterN;\
        if (lastUpdate != FORBIDDEN_UPDATE) { \
            fprintf(stderr, "Something's wrong.\n");\
            fclose(localForbiddenFile); \
            localForbiddenFile = fopen(FILE_STRING, "r"); \
            lastUpdate = FORBIDDEN_UPDATE; \
            fseek(localForbiddenFile, 0, SEEK_END);\
            fileSize = ftell(localForbiddenFile);\
            rewind(localForbiddenFile);\
            free(fileCopy);\
            char *fileCopy = malloc(fileSize+1);\
            memset(fileCopy, 0, sizeof(fileCopy));\
            fread(fileCopy, fileSize, 1, localForbiddenFile);\
            startingLine = fileCopy;\
        } \
        memset(holdURL2, 0, sizeof(holdURL2));   \
        fprintf(stderr, "Let's go!\n"); \
    } \
    memset(holdURL2, 0, sizeof(holdURL2));

#define evaluateForbidden(forbiddenMacroFlag) pthread_mutex_lock(&forbiddenMTX); \
if (GLOBAL_LAST_UPDATE != FORBIDDEN_UPDATE) { \
        fclose(localForbiddenFile); \
        localForbiddenFile = fopen(FILE_STRING, "r"); \
        GLOBAL_LAST_UPDATE = FORBIDDEN_UPDATE; \
        fseek(localForbiddenFile, 0, SEEK_END);\
        fileSize = ftell(localForbiddenFile);\
        rewind(localForbiddenFile);\
        memset(forbiddenFileCopy, 0, sizeof(forbiddenFileCopy));\
        fread(forbiddenFileCopy, fileSize, 1, localForbiddenFile);\
        startingLine = forbiddenFileCopy;\
    } \
    rewind(localForbiddenFile); \
    memset(holdURL, 0, sizeof(holdURL)); \
    startingLine = forbiddenFileCopy;\
    while (sscanf(startingLine, "%s%n", holdURL, &theLetterN) == 1) {\
        if (strcmp(hostName, holdURL) == 0) { \
            errorFlag = EXIT_WITH_ERROR; \
            errorStatus = forbiddenMacroFlag; \
            fprintf(stderr, "Forbidden site attempted access.\n", sizeof("Forbidden site attempted access.\n")); \
            pthread_mutex_unlock(&forbiddenMTX); \
            goto finale; \
        } \
        if (strcmp(hostIP, holdURL) == 0) { \
            errorFlag = EXIT_WITH_ERROR; \
            errorStatus = forbiddenMacroFlag; \
            fprintf(stderr, "Forbidden site attempted access.\n", sizeof("Forbidden site attempted access.\n")); \
            pthread_mutex_unlock(&forbiddenMTX); \
            goto finale; \
        } \
        startingLine += theLetterN;\
        if (GLOBAL_LAST_UPDATE != FORBIDDEN_UPDATE) { \
            fclose(localForbiddenFile); \
            localForbiddenFile = fopen(FILE_STRING, "r"); \
            GLOBAL_LAST_UPDATE = FORBIDDEN_UPDATE; \
            fseek(localForbiddenFile, 0, SEEK_END);\
            fileSize = ftell(localForbiddenFile);\
            rewind(localForbiddenFile);\
            memset(forbiddenFileCopy, 0, sizeof(forbiddenFileCopy));\
            fread(forbiddenFileCopy, fileSize, 1, localForbiddenFile);\
            startingLine = forbiddenFileCopy;\
        } \
        memset(holdURL, 0, sizeof(holdURL));   \
    } \
    memset(holdURL, 0, sizeof(holdURL)); \
    pthread_mutex_unlock(&forbiddenMTX);


void *proxy(void *ptr) {
    FILE* localForbiddenFile;
    int numRead = 0;
    int totalRead = 0;

    int errorFlag = NO_ERRORS;
    int errorStatus = ALL_GOOD;
    int shutdownFlag = IGNORE_IT;
    int freeSSLFlag = IGNORE_IT;
    int closeServerFlag = IGNORE_IT;
    int freeCTXFlag = IGNORE_IT;
    int closeClientFlag = IGNORE_IT;
    int memsetFlag1 = IGNORE_IT;
    int memsetFlag2 = IGNORE_IT;
    int memsetFlag3 = IGNORE_IT;
    int freeLineFlag = IGNORE_IT;
    int hostFlag = IGNORE_IT;
    char holdURL[MAX_URL];
    char holdURL2[MAX_URL];
    memset(holdURL, 0, sizeof(holdURL));
    memset(holdURL2, 0, sizeof(holdURL2));

    proxyPacket myProxy = *(proxyPacket*)ptr;
    int client = myProxy.client;
    localForbiddenFile = fopen(FILE_STRING, "r");
    /*
    
    fseek(localForbiddenFile, 0, SEEK_END);
    uint64_t fileSize = ftell(localForbiddenFile);
    rewind(localForbiddenFile);
    char *fileCopy = malloc(fileSize+1);
    memset(fileCopy, 0, sizeof(fileCopy));
    fread(fileCopy, fileSize, 1, localForbiddenFile);
    
    */
   uint64_t fileSize;
   fseek(localForbiddenFile, 0, SEEK_END);
        fileSize = ftell(localForbiddenFile);
        rewind(localForbiddenFile);
    char *startingLine = forbiddenFileCopy;
    
    int theLetterN;
    int lastUpdate = GLOBAL_LAST_UPDATE;


    char command[MAX_COMMAND_LENGTH];
    char command2[MAX_COMMAND_LENGTH];
    memset(command, 0, MAX_COMMAND_LENGTH);
    memset(command2, 0, MAX_COMMAND_LENGTH);
    char requestString[5];
    memset(requestString, 0, sizeof(requestString));
    //char hostName[MAX_WEBSITE_LENGTH];
    int requestType = -1;
    int lookingForHost = HOST_SEEKING;
    int lookingForPort = NO_PORT;
    int defPort = 443;
    
    //char* miniToken;
    int readErrorCheck = read(client, command, MAX_COMMAND_LENGTH);

    if (readErrorCheck == 0) {
        errorFlag = EXIT_WITH_ERROR;
        errorStatus = DEAD_CLIENT;
        fprintf(stderr, "Client has shut down, read.\n");
        //close(client);
        //memset(command, 0, MAX_COMMAND_LENGTH);
        //memset(command2, 0, MAX_COMMAND_LENGTH);
        goto finale;
        //pthread_exit((void*)-1);
    }
    else if (readErrorCheck < 0) {
        errorFlag = EXIT_WITH_ERROR;
        errorStatus = DEAD_CLIENT;
        fprintf(stderr, "Invalid client connection, read.\n");
        //close(client);
        //memset(command, 0, MAX_COMMAND_LENGTH);
        //memset(command2, 0, MAX_COMMAND_LENGTH);
        goto finale;
        //pthread_exit((void*)-1);
    }
    //printf("%s\n",command);

    for (int i = 0; i < 6; i++) {
        requestString[i] = command[i];
    }
    if (requestString[2] == 'T') {
        requestString[3] = '\0';
        requestString[4] = '\0';
    }
    if (requestString[3] == 'D') {
        requestString[4] = '\0';
    }
    requestString[5] = '\0';
    //printf("%s\n",requestString);
    if (strcmp("GET", requestString) == 0) {
        requestType = GET_REQUEST;
    }
    else if (strcmp("HEAD", requestString) == 0) {
        requestType = HEAD_REQUEST;
    }
    else {
        requestType = NOT_IMPLEMENTED;
        errorFlag = EXIT_WITH_ERROR;
        errorStatus = NOT_IMPLEMENTED_STATUS;
        fprintf(stderr, "501, Not Implemented.\n");
        //write(client, "501. Not Implemented", sizeof("501. Not Implemented"));
        //close(client);
        //memset(command, 0, MAX_COMMAND_LENGTH);
        //memset(command2, 0, MAX_COMMAND_LENGTH);
        goto finale;
        //pthread_exit((void*)-1);
    }


    int tickingHost = 0;
    int linkingHost = 0;
    int hostStart = 0;
    int hostEnd = 0;
    int hostLength = 0;
    int portStart = 0;
    int portEnd = 0;
    int portLength = 0;
    for (int i = 0; i < MAX_COMMAND_LENGTH; i++) {
        if (i < (MAX_COMMAND_LENGTH - 7)) {
            if (lookingForHost == HOST_SEEKING) {
                if (command[i] == 'H') {
                    if (command[i+1] == 'o') {
                        if (command[i+2] == 's') {
                            if (command[i+3] == 't') {
                                if (command[i+4] == ':') {
                                    if (command[i+5] == ' ') {
                                        lookingForHost = HOST_FOUND;
                                        //memset(hostName, 0, sizeof(hostName));
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (lookingForHost == HOST_FOUND) {
                tickingHost++;
            }
            if (tickingHost > 6) {
                if (hostStart == 0) {
                    hostStart = i;
                }
                if ((command[i] != '\n') && (command[i] != '\r') && (command[i] != ':') && (lookingForHost != HOST_COMPLETE)) {
                    hostLength++;
                    //hostName[linkingHost] = command[i];
                }
                else if ((command[i] == ':') && (lookingForHost != HOST_COMPLETE)) {
                    hostLength++;
                    hostEnd = i;
                    lookingForHost = HOST_COMPLETE;
                    lookingForPort = PORT_DELAY;
                }
                else {
                    //hostName[linkingHost] = '\0';
                    if ((lookingForPort != PORT_FOUND) && (lookingForPort != PORT_COMPLETE) && (lookingForPort != PORT_DELAY)) {
                        hostLength++;
                        hostEnd = i;
                        lookingForHost = HOST_COMPLETE;
                        break;
                    }
                }
                if (lookingForPort == PORT_FOUND) {
                    if (portStart == 0) {
                        portStart = i;
                    }
                    if ((command[i] != '\n') && (command[i] != '\r')) {
                        portLength++;
                    }
                    else {
                        portLength++;
                        portEnd = i;
                        lookingForPort = PORT_COMPLETE;
                        break;
                    }

                }
                if (lookingForPort == PORT_DELAY) {
                    lookingForPort = PORT_FOUND;
                }
                //linkingHost++;
            }
        }
    }

    char *hostName;
    char single[2] = "\0";
    hostName = malloc(hostLength);
    memset(hostName, 0, sizeof(hostName));
    for (int j = 0; j < hostLength; j++) {
        if ((j == 0) && (j < hostLength-1)) {
            strncat(hostName, &command[hostStart], 1);
        }
        else if ((j != 0) && (j < hostLength-1)) {
            strncat(hostName, &command[hostStart+j], 1);
        }
        else if (j == hostLength-1) {
            strncat(hostName, single, 1);
        }
    }

    //printf("The Hostname: %s %d\n", hostName, strlen(hostName));
    //pthread_mutex_lock(&forbiddenMTX);

    //Do forbiddenFile stuff
    /*
    if (lastUpdate != FORBIDDEN_UPDATE) {
        fclose(localForbiddenFile);
        localForbiddenFile = fopen(FILE_STRING, "r");
        lastUpdate = FORBIDDEN_UPDATE;
    }
    rewind(localForbiddenFile);
    memset(holdURL, 0, sizeof(holdURL));
    while (fscanf(localForbiddenFile, "%s", holdURL) != EOF) {
        if (strcmp(hostName, holdURL) == 0) {
            //Raise error for forbidden
            errorFlag = EXIT_WITH_ERROR;
            errorStatus = FORBIDDEN_AT_HOST;
        }
        if (lastUpdate != FORBIDDEN_UPDATE) {
            fclose(localForbiddenFile);
            localForbiddenFile = fopen(FILE_STRING, "r");
            lastUpdate = FORBIDDEN_UPDATE;
        }
    }
    */
    //memset(holdURL, 0, sizeof(holdURL));
    //pthread_mutex_unlock(&forbiddenMTX);

    

    char *portNum;
    if (lookingForPort == PORT_COMPLETE) {
        
        portNum = malloc(portLength);
        memset(portNum, 0, sizeof(portNum));
        for (int j = 0; j < portLength; j++) {
            if ((j == 0) && (j < portLength-1)) {
                strncat(portNum, &command[portStart], 1);
            }
            else if ((j != 0) && (j < portLength-1)) {
                strncat(portNum, &command[portStart+j], 1);
            }
            else if (j == portLength-1) {
                strncat(portNum, single, 1);
            }
        }
        //printf("The Port: %s %d\n", portNum, strlen(portNum));
        defPort = atoi(portNum);
    }
    
    
    
    //if (lookingForPort == PORT_COMPLETE) {
        
    //}
    
    
    struct in_addr **addressList;
    struct hostent *hostStruct = gethostbyname(hostName);
    if (hostStruct == NULL) {
        errorFlag = EXIT_WITH_ERROR;
        errorStatus = INVALID_HOSTNAME;
        /*
        fprintf(stderr, "Invalid hostname.\n");
        close(client);
        free(hostName);
        PORT_FREE
        memset(command, 0, MAX_COMMAND_LENGTH);
        memset(command2, 0, MAX_COMMAND_LENGTH);
        pthread_exit((void*)-1);
        */
        goto finale;
    }
    if (hostStruct->h_addrtype == AF_INET) {
        addressList = (struct in_addr **)hostStruct->h_addr_list;
    }
    else if (hostStruct->h_addrtype == AF_INET6) {
        errorFlag = EXIT_WITH_ERROR;
        errorStatus = IPV6_NOT_SUPPORTED;
        fprintf(stderr, "iPv6 is not supported.\n");
        /*
        close(client);
        free(hostName);
        PORT_FREE
        memset(command, 0, MAX_COMMAND_LENGTH);
        memset(command2, 0, MAX_COMMAND_LENGTH);
        pthread_exit((void*)-1);
        */
        goto finale;
    }
    //free(hostName);
    char hostIP[INET_ADDRSTRLEN];
    memset(hostIP, 0, sizeof(hostIP));
    inet_ntop(AF_INET, &IP_ADDRESS, hostIP, INET_ADDRSTRLEN);
    //fprintf(stderr, "The HostIP: %s", hostIP);

    evaluateForbidden(FORBIDDEN_AT_HOST);

    //struct sockaddr_in* miniAddress = (struct sockaddr_in*)&cliaddr;
    //        struct in_addr theIPAddr = miniAddress->sin_addr;
    //        char clientIP_Main[INET_ADDRSTRLEN];
    //        inet_ntop(AF_INET, &theIPAddr, clientIP_Main, INET_ADDRSTRLEN);
    //printf("The IP: %s\n", hostIP);
    /*
    lookingForHost = HOST_SEEKING;
    lookingForPort = NO_PORT;
    tickingHost = 0;
    linkingHost = 0;
    hostStart = 0;
    hostEnd = 0;
    hostLength = 0;
    portStart = 0;
    portEnd = 0;
    portLength = 0;
    */

    

    SSL_CTX *ctao;
    SSL *ssl;
    //SSL_METHOD *method;
    int server;
    struct hostent *host;
    struct sockaddr_in addr;

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();  
    const SSL_METHOD *method = TLSv1_2_client_method();  
    ctao = SSL_CTX_new(method);   
    if ( ctao == NULL ){
        errorFlag = EXIT_WITH_ERROR;
        errorStatus = COULD_NOT_CREATE_CONTEXT;
        fprintf(stderr, "Could not create new context.\n");
        /*
        PORT_FREE
        SSL_CTX_free(ctao);
        close(client);
        memset(command, 0, MAX_COMMAND_LENGTH);
        memset(command2, 0, MAX_COMMAND_LENGTH);
        pthread_exit((void*)-1);
        */
        goto finale;
    }

    //server = OpenConnection(hostname, atoi(portnum));
    server = socket(PF_INET, SOCK_STREAM, 0);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(defPort);
    addr.sin_addr.s_addr = IP_ADDRESS;
    PORT_FREE
    struct timeval timeout;      
    timeout.tv_sec = 15;
    timeout.tv_usec = 0;
    
    setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof (timeout));
    setsockopt(server, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof (timeout));

    if (connect(server, (struct sockaddr*)&addr, sizeof(addr)) != 0 ) {
        errorFlag = EXIT_WITH_ERROR;
        errorStatus = CANNOT_REACH_SERVER;
        fprintf(stderr, "Could not connect to server.\n");
        /*
        close(server);
        SSL_CTX_free(ctao);
        close(client);
        memset(command, 0, MAX_COMMAND_LENGTH);
        memset(command2, 0, MAX_COMMAND_LENGTH);
        pthread_exit((void*)-1);
        */
        goto finale;
    }

    ssl = SSL_new(ctao);      /* create new SSL connection state */
    SSL_set_fd(ssl, server);    /* attach the socket descriptor */
    if ( SSL_connect(ssl) != 1 ){
        errorFlag = EXIT_WITH_ERROR;
        errorStatus = CANNOT_REACH_SERVER_SSL;
        fprintf(stderr, "Could not connect to SSL server.\n");
        /*
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(server);
        SSL_CTX_free(ctao);
        close(client);
        memset(command, 0, MAX_COMMAND_LENGTH);
        memset(command2, 0, MAX_COMMAND_LENGTH);
        pthread_exit((void*)-1);
        */
        goto finale;
    }
    //pthread_mutex_lock(&forbiddenMTX);

    //Do forbiddenFile stuff
    //rewind(localForbiddenFile);
    //memset(holdURL, 0, sizeof(holdURL));
    //while (fscanf(localForbiddenFile, "%s", holdURL) != EOF) {
    //    if (strcmp(hostName, holdURL) == 0) {
    //        //Raise error for forbidden
    //        errorFlag = EXIT_WITH_ERROR;
    //        errorStatus = FORBIDDEN_AT_SSL_WRITE;
    //    }
    //}
    //memset(holdURL, 0, sizeof(holdURL));
    //pthread_mutex_unlock(&forbiddenMTX);
    evaluateForbidden(FORBIDDEN_AT_SSL_WRITE);
    int sslWriteError = SSL_write(ssl, command, sizeof(command));
    if (sslWriteError <= 0) {
        switch(SSL_get_error(ssl,sslWriteError)) {
            case(SSL_ERROR_ZERO_RETURN):
                errorFlag = EXIT_WITH_ERROR;
                errorStatus =  DEAD_SERVER_SSL;
                fprintf(stderr, "OpenSSL connection closed.\n");
                /*
                SSL_shutdown(ssl);
                SSL_free(ssl);
                close(server);
                SSL_CTX_free(ctao);
                close(client);
                memset(command, 0, MAX_COMMAND_LENGTH);
                memset(command2, 0, MAX_COMMAND_LENGTH);
                pthread_exit((void*)-1);
                */
                goto finale;
                break;
            default:
                errorFlag = EXIT_WITH_ERROR;
                errorStatus = CANNOT_WRITE_SSL;
                fprintf(stderr, "OpenSSL write failure.\n");
                /*
                SSL_shutdown(ssl);
                SSL_free(ssl);
                close(server);
                SSL_CTX_free(ctao);
                close(client);
                memset(command, 0, MAX_COMMAND_LENGTH);
                memset(command2, 0, MAX_COMMAND_LENGTH);
                pthread_exit((void*)-1);
                */
                goto finale;
                break;
        }
        
    }
    
    int sslReadError = 0;
    int writeErrorCheck = 0;
    

    
    
    int firstRead = 0;
    int firstLineLength = 0;
    char *firstLine;
    do {
        if (firstRead == 0) {
            evaluateForbidden(FORBIDDEN_AT_SSL_READ_FIRST);
        }
        else {
            evaluateForbidden(FORBIDDEN_AT_SSL_READ_OTHER);
        }
        numRead = SSL_read(ssl, command2, sizeof(command2));
        if (numRead >= 0) {
            totalRead = totalRead + numRead;
        }
        
        
        sslReadError = numRead;
        if ((SSL_get_error(ssl, sslReadError) != SSL_ERROR_NONE) && (SSL_get_error(ssl, sslReadError) != SSL_ERROR_ZERO_RETURN) && (firstRead == 0)) {
                if (SSL_get_error(ssl, sslReadError) != 5) {
                    errorFlag = EXIT_WITH_ERROR;
                    errorStatus = CANNOT_READ_SSL_FIRST;
                    fprintf(stderr, "OpenSSL read failure. %d\n", SSL_get_error(ssl,sslReadError));
                    //printf("%d\n", errno);
                    //perror("Error");
                    //SSL_shutdown(ssl);
                    //SSL_free(ssl);
                    //close(server);
                    //SSL_CTX_free(ctao);
                    //close(client);
                    //memset(command, 0, MAX_COMMAND_LENGTH);
                    //memset(command2, 0, MAX_COMMAND_LENGTH);
                    //free(firstLine);
                    goto finale;
                    //pthread_exit((void*)-1);
                }
                
        }
        if ((SSL_get_error(ssl, sslReadError) != SSL_ERROR_NONE) && (SSL_get_error(ssl, sslReadError) != SSL_ERROR_ZERO_RETURN) && (firstRead == 1)) {
                if (SSL_get_error(ssl, sslReadError) != 5) {
                    errorFlag = EXIT_WITH_ERROR;
                    errorStatus = CANNOT_READ_SSL_OTHER;
                    fprintf(stderr, "OpenSSL read failure. %d\n", SSL_get_error(ssl,sslReadError));
                    //printf("%d\n", errno);
                    //perror("Error");
                    //SSL_shutdown(ssl);
                    //SSL_free(ssl);
                    //close(server);
                    //SSL_CTX_free(ctao);
                    //close(client);
                    //memset(command, 0, MAX_COMMAND_LENGTH);
                    //memset(command2, 0, MAX_COMMAND_LENGTH);
                    //free(firstLine);
                    goto finale;
                    //pthread_exit((void*)-1);
                }
                
        }
        if ((firstRead == 0) && (numRead > 0)) {
            firstRead = 1;
            for (int i = 0; i < MAX_COMMAND_LENGTH; i++) {
                if ((command2[i] == '\r') || (command2[i] == '\n')) {
                    break;
                }
                firstLineLength++;
            }
            firstLine = malloc(firstLineLength);
            memset(firstLine, 0, sizeof(firstLine));
            for (int j = 0; j < firstLineLength; j++) {
                if ((j == 0) && (j < firstLineLength-1)) {
                    strncat(firstLine, &command2[0], 1);
                }
                else if ((j != 0) && (j < firstLineLength-1)) {
                    strncat(firstLine, &command2[j], 1);
                }
                else if (j == firstLineLength-1) {
                    strncat(firstLine, single, 1);
                }
            }
        }

        evaluateForbidden(FORBIDDEN_AT_WRITE);
        writeErrorCheck = write(client, command2, numRead);
        if (writeErrorCheck == 0) {
            //fprintf(stderr, "Client has shut down, write.\n");
            /*
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(server);
            SSL_CTX_free(ctao);
            close(client);
            memset(command, 0, MAX_COMMAND_LENGTH);
            memset(command2, 0, MAX_COMMAND_LENGTH);
            pthread_exit((void*)-1);
            */
           break;
        }
        else if (writeErrorCheck < 0) {
            //fprintf(stderr, "Invalid client connection, write.\n");
            //Success?
            /*
            SSL_shutdown(ssl);
            SSL_free(ssl);
            close(server);
            SSL_CTX_free(ctao);
            close(client);
            memset(command, 0, MAX_COMMAND_LENGTH);
            memset(command2, 0, MAX_COMMAND_LENGTH);
            pthread_exit((void*)-1);
            */
           break;
        }
    } while (numRead > 0);
    //if ( <= 0) {
    //    
    //}
    //printf(command2);
   finale:
    struct timeval ms;
    int hours, minutes, seconds, day, month, year;
    time_t now;
    UPDATE_TIME

    

    char *statusCode;
    char *clientRequest;
    if (errorFlag == EXIT_WITH_ERROR) {
        switch(errorStatus) {
            case DEAD_CLIENT:
                write(client, "HTTP/1.1 500 Internal Server Error", sizeof("HTTP/1.1 500 Internal Server Error"));
                statusCode = "500";
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                break;
            case NOT_IMPLEMENTED_STATUS:
                write(client, "HTTP/1.1 501 Not Implemented", sizeof("HTTP/1.1 501 Not Implemented"));
                statusCode = "501";
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                break;
            case INVALID_HOSTNAME:
                write(client, "HTTP/1.1 502 Bad Gateway", sizeof("HTTP/1.1 502 Bad Gateway"));
                statusCode = "502";
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                hostFlag = FREE_HOSTNAME;
                //free(hostName);
                PORT_FREE
                break;
            case IPV6_NOT_SUPPORTED:
                write(client, "HTTP/1.1 502 Bad Gateway", sizeof("HTTP/1.1 502 Bad Gateway"));
                statusCode = "502";
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                hostFlag = FREE_HOSTNAME;
                //free(hostName);
                PORT_FREE
                break;
            case COULD_NOT_CREATE_CONTEXT:
                write(client, "HTTP/1.1 500 Internal Server Error", sizeof("HTTP/1.1 500 Internal Server Error"));
                statusCode = "500";
                freeCTXFlag = FREE_CTX;
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                hostFlag = FREE_HOSTNAME;
                PORT_FREE
                break;
            case CANNOT_REACH_SERVER:
                write(client, "HTTP/1.1 500 Internal Server Error", sizeof("HTTP/1.1 500 Internal Server Error"));
                statusCode = "500";
                closeServerFlag = CLOSE_SERVER;
                freeCTXFlag = FREE_CTX;
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                hostFlag = FREE_HOSTNAME;
                break;
            case CANNOT_REACH_SERVER_SSL:
                write(client, "HTTP/1.1 500 Internal Server Error", sizeof("HTTP/1.1 500 Internal Server Error"));
                statusCode = "500";
                shutdownFlag = SHUTDOWN_SSL;
                freeSSLFlag = FREE_SSL;
                closeServerFlag = CLOSE_SERVER;
                freeCTXFlag = FREE_CTX;
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                hostFlag = FREE_HOSTNAME;
                break;
            case DEAD_SERVER_SSL:
                write(client, "HTTP/1.1 504 Gateway Timeout", sizeof("HTTP/1.1 504 Gateway Timeout"));
                statusCode = "504";
                shutdownFlag = SHUTDOWN_SSL;
                freeSSLFlag = FREE_SSL;
                closeServerFlag = CLOSE_SERVER;
                freeCTXFlag = FREE_CTX;
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                hostFlag = FREE_HOSTNAME;
                break;
            case CANNOT_WRITE_SSL:
                write(client, "HTTP/1.1 500 Internal Server Error", sizeof("HTTP/1.1 500 Internal Server Error"));
                statusCode = "500";
                shutdownFlag = SHUTDOWN_SSL;
                freeSSLFlag = FREE_SSL;
                closeServerFlag = CLOSE_SERVER;
                freeCTXFlag = FREE_CTX;
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                hostFlag = FREE_HOSTNAME;
                break;
            case CANNOT_READ_SSL_FIRST:
                write(client, "HTTP/1.1 500 Internal Server Error", sizeof("HTTP/1.1 500 Internal Server Error"));
                statusCode = "500";
                shutdownFlag = SHUTDOWN_SSL;
                freeSSLFlag = FREE_SSL;
                closeServerFlag = CLOSE_SERVER;
                freeCTXFlag = FREE_CTX;
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                hostFlag = FREE_HOSTNAME;
                break;
            case CANNOT_READ_SSL_OTHER:
                write(client, "HTTP/1.1 500 Internal Server Error", sizeof("HTTP/1.1 500 Internal Server Error"));
                statusCode = "500";
                shutdownFlag = SHUTDOWN_SSL;
                freeSSLFlag = FREE_SSL;
                closeServerFlag = CLOSE_SERVER;
                freeCTXFlag = FREE_CTX;
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                freeLineFlag = FREE_FIRSTLINE;
                hostFlag = FREE_HOSTNAME;
                break;
            case FORBIDDEN_AT_HOST:
                write(client, "HTTP/1.1 403 Forbidden", sizeof("HTTP/1.1 403 Forbidden"));
                statusCode = "403";
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                hostFlag = FREE_HOSTNAME;
                break;
            case FORBIDDEN_AT_SSL_WRITE:
                write(client, "HTTP/1.1 403 Forbidden", sizeof("HTTP/1.1 403 Forbidden"));
                statusCode = "403";
                shutdownFlag = SHUTDOWN_SSL;
                freeSSLFlag = FREE_SSL;
                closeServerFlag = CLOSE_SERVER;
                freeCTXFlag = FREE_CTX;
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                hostFlag = FREE_HOSTNAME;
                break;
            case FORBIDDEN_AT_SSL_READ_FIRST:
                write(client, "HTTP/1.1 403 Forbidden", sizeof("HTTP/1.1 403 Forbidden"));
                statusCode = "403";
                shutdownFlag = SHUTDOWN_SSL;
                freeSSLFlag = FREE_SSL;
                closeServerFlag = CLOSE_SERVER;
                freeCTXFlag = FREE_CTX;
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                hostFlag = FREE_HOSTNAME;
                break;
            case FORBIDDEN_AT_SSL_READ_OTHER:
                write(client, "HTTP/1.1 403 Forbidden", sizeof("HTTP/1.1 403 Forbidden"));
                statusCode = "403";
                shutdownFlag = SHUTDOWN_SSL;
                freeSSLFlag = FREE_SSL;
                closeServerFlag = CLOSE_SERVER;
                freeCTXFlag = FREE_CTX;
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                freeLineFlag = FREE_FIRSTLINE;
                hostFlag = FREE_HOSTNAME;
                break;
            case FORBIDDEN_AT_WRITE:
                write(client, "HTTP/1.1 403 Forbidden", sizeof("HTTP/1.1 403 Forbidden"));
                statusCode = "403";
                shutdownFlag = SHUTDOWN_SSL;
                freeSSLFlag = FREE_SSL;
                closeServerFlag = CLOSE_SERVER;
                freeCTXFlag = FREE_CTX;
                closeClientFlag = CLOSE_CLIENT;
                memsetFlag1 = MEMSET_COMMAND;
                memsetFlag2 = MEMSET_COMMAND2;
                freeLineFlag = FREE_FIRSTLINE;
                hostFlag = FREE_HOSTNAME;
                break;
            default:
                break;
        }
    }
    else {
        shutdownFlag = SHUTDOWN_SSL;
        freeSSLFlag = FREE_SSL;
        closeServerFlag = CLOSE_SERVER;
        freeCTXFlag = FREE_CTX;
        closeClientFlag = CLOSE_CLIENT;
        memsetFlag1 = MEMSET_COMMAND;
        memsetFlag2 = MEMSET_COMMAND2;
        memsetFlag3 = MEMSET_ACCESS;
        freeLineFlag = FREE_FIRSTLINE;
        hostFlag = FREE_HOSTNAME;
        char *chain = firstLine;
        statusCode = strtok_r(chain, " ", &chain);
        if (statusCode != NULL) {
            statusCode = strtok_r(NULL, " ", &chain);
        }
        else {
            //Do some error stuff
        }
    }

    ////////REMEMBER TO DO HOSTNAME TO IP STRING AND CHECK IF THAT EXISTS IN THE FORBIDDEN FILE

    
    //printf("IP address is: %s\n", inet_ntoa(miniAddress->sin_addr));
    //ms.tv_usec/1000
    
    //char *statusCode;
    

    

    //char *clientRequest;
    char *link = command;
    clientRequest = strtok_r(link, "\r\n", &link);
    
    //printf("%s\n",  token);
    char accessString[MAX_COMMAND_LENGTH/2];
    memset(accessString, 0, sizeof(accessString));
    sprintf(accessString, "%d-%02d-%02dT%02d:%02d:%02d.%ldZ %s \"%s\" %s %d", year, month, day, hours, minutes, seconds, ms.tv_usec/1000, myProxy.clientIP, clientRequest, statusCode, totalRead);

    
    pthread_mutex_lock(&logMTX);
    fseek(accessFile, 0L, SEEK_END);
    int32_t fileLength = ftell(accessFile);
    //fprintf(stderr, "Hi %s", accessString);
    if (fileLength > 4) {
        fprintf(accessFile, "\n");
    }
    fprintf(accessFile, "%s", accessString);
    fflush(accessFile);
    //fseek(fp, 0L, SEEK_SET);

    //fileLength = ftell(accessFile);
    //if (fileLength < 2) {
    //    fprintf(accessFile, "%s", accessString);
    //}
    pthread_mutex_unlock(&logMTX);
    if (shutdownFlag != IGNORE_IT) {
        SSL_shutdown(ssl);
    }
    if (freeSSLFlag != IGNORE_IT) {
        SSL_free(ssl);
    }
    if (closeServerFlag != IGNORE_IT) {
        close(server);
    }
    if (freeCTXFlag != IGNORE_IT) {
        SSL_CTX_free(ctao);
    }
    if (closeClientFlag != IGNORE_IT) {
        close(client);
    }
    if (memsetFlag1 != IGNORE_IT) {
        memset(command, 0, MAX_COMMAND_LENGTH);
    }
    if (memsetFlag2 != IGNORE_IT) {
        memset(command2, 0, MAX_COMMAND_LENGTH);
    }
    memset(accessString, 0, sizeof(accessString));
    if (freeLineFlag != IGNORE_IT) {
        free(firstLine);
    }
    
    if (hostFlag != IGNORE_IT) {
        free(hostName);
    }
    
    
    memset(holdURL, 0, sizeof(holdURL));
    memset(hostIP, 0, sizeof(hostIP));
    //free(fileCopy);
    fclose(localForbiddenFile);
    if (errorFlag == EXIT_WITH_ERROR) {
        pthread_exit((void*)-1);
    }
    
    pthread_exit(0);
}