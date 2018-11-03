#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <fcntl.h>

#include "defns.h"

/* Function Prototypes */
status_t parseScript(char *, ClientStruct_t *);
int countLines(FILE *);
status_t executeCommands(ClientStruct_t *);

int main(int argc, char *argv[])
{
    status_t status = ERROR;       /* Return status */
    ClientStruct_t clientStruct;  /* Structure to hold commands */

    /* Initialize structures */
    memset(&clientStruct, 0, sizeof(ClientStruct_t));

    printf("Sean Gatenby\nCSE531 Lab2 Client\ns");

    /* Validate arguments */
    if (argc == 6)
    {
        /* Populate client structure */
        clientStruct.serverIpAddress = argv[1];                    /* First arg: server IP address (dotted decimal) */
        clientStruct.machineName = argv[2];                        /* Second arg: client name (string w/o spaces) */
        clientStruct.clientNumber = strtol(argv[3], NULL, 10);     /* Third arg: client number (decimal client number) */
        clientStruct.serverPortNumber = strtol(argv[4], NULL, 10); /* Fourth arg: server port number (decimal number 1024-65535) */
        clientStruct.scriptFileName = argv[5];                     /* Fifth arg: script file name (string full path to file) */

        /* Open script file and read command into a command buffer */
        if(parseScript(clientStruct.scriptFileName, &clientStruct) == OK)
        {
            /* Execute commands */
            if(executeCommands(&clientStruct) == OK)
            {
                status = OK;
            }
            else
            {
                printError("One or more commands failed to execute%s", "");
            }
        }
        else
        {
            printError("Can't parse commands%s", "");
        }
    }
    else
    {
        printError("Usage: %s <Server IP address (dotted decimal)> <client machine name> <client number> <service port> <script file name>", argv[0]);
    }

    /* Clean up malloc's */
    for(int i = 0; i < clientStruct.numCommands; i++)
    {
        if(clientStruct.commandArray[i] != NULL)
        {
            free(clientStruct.commandArray[i]);
        }
    }

    if(clientStruct.commandArray != NULL)
    {
        free(clientStruct.commandArray);
    }

    return status;
}

status_t parseScript(char *fileName, ClientStruct_t *clientStruct)
{
    FILE *file_ptr = NULL;
    status_t status = ERROR;
    size_t lineLength = 0;

    /* Open script file for reading */
    if((file_ptr = fopen(fileName, "r")) != NULL)
    {
        /* Count the commands in the script */
        if((clientStruct->numCommands = countLines(file_ptr)) != 0)
        {
#ifdef DEBUG
            printf("Found %d commands in %s\n", clientStruct->numCommands, fileName);
#endif
            /* Allocate enough space to store 'numLines' commands */
            if((clientStruct->commandArray = malloc(sizeof(char *) * clientStruct->numCommands)) != NULL)
            {
                for(int i = 0; i < clientStruct->numCommands; i++)
                {
                    /* Let getLine allocate the correct amount of space for the command */
                    lineLength = 0;
                    clientStruct->commandArray[i] = NULL;

                    /* Error out if an error occurs, or no characters were read before EOF  */
                    if(getline(&(clientStruct->commandArray[i]), &lineLength, file_ptr) == ERROR)
                    {
                        printErrno("Failed to read line %d", i);
                        break;
                    }
                    else
                    {
#ifdef DEBUG
                        printf("%s", clientStruct->commandArray[i]);
#endif
                    }
                }

                status = OK;
            }
            else
            {
                printErrno("Malloc failed%s", "");
            }
        }
        else
        {
            printError("Unexpected line count:%d", clientStruct->numCommands);
        }

        fclose(file_ptr);
    }
    else
    {
        printErrno("Can't open %s for reading", fileName);
    }

    return status;
}

status_t executeCommands(ClientStruct_t *clientStruct)
{
    ClientRequest_t request;
    bool executeFailure = false;
    char *incarnationLockfileName = NULL;
    char *incarnationFileName = NULL;
    int incarnationLockFile = -1;
    FILE *incarnationFile_ptr = NULL;
    struct flock lock;
    status_t status = ERROR;
    ServerResponse_t response;
    struct timeval tv;
    int bytesReceived = 0;

    /* Initialize structures */
    memset(&request, 0, sizeof(ClientRequest_t));
    memset(&response, 0, sizeof(ServerResponse_t));
    memset(&tv, 0, sizeof(struct timeval));

    /* Allocate string to hold full path to the lock file for the incarnation number */
    if((incarnationLockfileName = malloc(sizeof(char) * (strlen(INCARNATION_LOCKFILE) + strlen(clientStruct->machineName) + 1))) != NULL)
    {
        /* Build incarnation lock file name */
        strcpy(incarnationLockfileName, INCARNATION_LOCKFILE);
        strcat(incarnationLockfileName, clientStruct->machineName);

        /* Allocate string to hold full path to file holding incarnation number */
        if((incarnationFileName = malloc(sizeof(char) * (strlen(INCARNATION_FILE) + strlen(clientStruct->machineName) + 1))) != NULL)
        {
            /* Build incarnation file name */
            strcpy(incarnationFileName, INCARNATION_FILE);
            strcat(incarnationFileName, clientStruct->machineName);

            /* Create a datagram/UDP socket */
            if ((clientStruct->sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) >= 0)
            {
                /* Set receive timeout to 1 second */
                struct timeval tv;
                tv.tv_sec = 1;
                tv.tv_usec = 0;
                if (setsockopt(clientStruct->sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
                    printErrno("Can't set socket timeout%s", "");
                }

                /* Construct the server address structure */
                memset(&(clientStruct->serverAddr), 0, sizeof(clientStruct->serverAddr));           /* Zero out structure */
                clientStruct->serverAddr.sin_family = AF_INET;                                      /* Internet addr family */
                clientStruct->serverAddr.sin_addr.s_addr = inet_addr(clientStruct->serverIpAddress);/* Server IP address */
                clientStruct->serverAddr.sin_port   = htons(clientStruct->serverPortNumber);        /* Server port */

                for(int i = 0; i < clientStruct->numCommands; i++)
                {
                    /* Initialize flag */
                    executeFailure = false;

                    /* Check if command is the 'fail' command
                     * This is not sent to the server and special actions must be taken */
                    if(strncmp(clientStruct->commandArray[i], "fail", 4) == 0)
                    {
                        executeFailure = true;
                    }

                    /* Get file handle for incarnation lock file */
                    if((incarnationLockFile = open(incarnationLockfileName, O_CREAT | O_RDWR, 0644)) != -1)
                    {
                        /* Wait until we get a read lock on the incarnation lock file */
                        memset(&lock, 0, sizeof(lock));
                        lock.l_type = F_RDLCK;
                        fcntl(incarnationLockFile, F_SETLKW, &lock);

                        /* If the incarnation file doesn't exist, create one with a value of 0 */
                        if((incarnationFile_ptr = fopen(incarnationFileName, "r+")) == NULL)
                        {
                            incarnationFile_ptr = fopen(incarnationFileName, "w");
                            if(fprintf(incarnationFile_ptr, "%d\n", 0) < 0)
                            {
                                printErrno("Error writing to  %s", incarnationFileName);
                            }
                            clientStruct->clientIncarnation = 0;
                        }
                        /* Else read current incarnation number */
                        else
                        {
                            if(fscanf(incarnationFile_ptr, "%d\n", &clientStruct->clientIncarnation) < 0)
                            {
                                printErrno("Error reading from  %s", incarnationFileName);
                            }

                            if(executeFailure == true)
                            {
                                clientStruct->clientIncarnation++;

                                fseek(incarnationFile_ptr, 0, SEEK_SET);

                                if(fprintf(incarnationFile_ptr, "%d\n", clientStruct->clientIncarnation) < 0)
                                {
                                    printErrno("Error writing to  %s", incarnationFileName);
                                }
                            }
                        }

                        /* Close incarnation file */
                        fclose(incarnationFile_ptr);

                        /* Release lock */
                        fcntl(incarnationLockFile, F_UNLCK, &lock);

                        /* Close incarnation lock file */
                        close(incarnationLockFile);

                        request.clientNumber = clientStruct->clientNumber;
                        request.requestNumber = clientStruct->requestNumber;
                        request.clientIncarnation = clientStruct->clientIncarnation;
                        strcpy(request.operation, clientStruct->commandArray[i]);
                        strcpy(request.machineName, clientStruct->machineName);

                        /* Process command */
                        /* Send the struct to the server IFF request was NOT "failure" */
                        if(executeFailure == false)
                        {
                            do
                            {
                                if (sendto(clientStruct->sockfd, &request, sizeof(ClientRequest_t), 0, (struct sockaddr *) &(clientStruct->serverAddr), sizeof(clientStruct->serverAddr)) == sizeof(ClientRequest_t))
                                {
#ifdef DEBUG
                                    printf("%s:%d.%d_%d - Sent %s", request.machineName, request.clientNumber, request.clientIncarnation, request.requestNumber, request.operation);
#endif

                                    /* Set the size of the in-out parameter */
                                    socklen_t serverAddrLen = sizeof(clientStruct->serverAddr);

                                    bytesReceived = recvfrom(clientStruct->sockfd, &response, sizeof(ServerResponse_t), 0, (struct sockaddr *) &(clientStruct->serverAddr), &serverAddrLen);
                                }
                                else
                                {
                                    printErrno("Didn't send expected number of bytes%s", "");
                                }

                                if(bytesReceived == ERROR)
                                {
#ifdef DEBUG
                                    printf("%s:%d.%d_%d - Request timed out\n",request.machineName, request.clientNumber, request.clientIncarnation, request.requestNumber);
#endif
                                }

                            }while(bytesReceived == ERROR);

                            if (bytesReceived == sizeof(ServerResponse_t))
                            {
                                printf("%s:%d.%d_%d - Return value: %d\n", request.machineName, request.clientNumber, request.clientIncarnation, request.requestNumber, response.returnValue);
                                printf("%s:%d.%d_%d - Return msg: %s", request.machineName, request.clientNumber, request.clientIncarnation, request.requestNumber, response.returnString);
                            }
                            else
                            {
                                printErrno("Didn't send expected number of bytes%s", "");
                            }

                            /* Increment request count */
                            clientStruct->requestNumber++;
                        }
                        else
                        {
                            /* Reset request count */
                            clientStruct->requestNumber = 0;
                        }
                    }
                    else
                    {
                        printErrno("Can't open %s for reading", incarnationLockfileName);
                    }
                }

                close(clientStruct->sockfd);
            }
            else
            {
                printErrno("Can't create socket%s", "");
            }
        }
        else
        {
            printErrno("Malloc failed%s", "");
        }
    }
    else
    {
        printErrno("Malloc failed%s", "");
    }

    /* Cleanup malloc's */
    if(incarnationLockfileName != NULL)
    {
        free(incarnationLockfileName);
    }

    if(incarnationFileName != NULL)
    {
        free(incarnationFileName);
    }

    return status;
}

int countLines(FILE *file_ptr)
{
    char c = 0;
    int lineCount = 0;

    if(file_ptr != NULL)
    {
        /* Ensure file pointer is to start of file */
        fseek(file_ptr, 0, SEEK_SET);

        /* Loop through file and count all carriage returns/newlines */
        do
        {
            c = fgetc(file_ptr);
            if(c == '\n')
            {
                lineCount++;
            }
        }while(!feof(file_ptr));

        /* Ensure file pointer is to start of file */
        fseek(file_ptr, 0, SEEK_SET);
    }
    else
    {
        printError("Invalid file handle%s", "");
    }

    return lineCount;
}
