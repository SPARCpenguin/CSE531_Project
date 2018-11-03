#include "defns.h"

#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket() and bind() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <string.h>     /* for strtok() */
#include <time.h>       /* for time() */

/* Globals */
static ClientTableNode_t *clientTableList;
static LockTableNode_t *lockTableList;
int commFailureCounter;

/* Function Prototypes */
status_t HandleRequest(ServerStruct_t, ClientRequest_t);
RequestAction_t ValidateClient(ClientRequest_t, ClientTableNode_t **);
ClientTableNode_t *GetClient(ClientRequest_t);
status_t DeleteClient(char *, int);
ClientTableNode_t *AddClient(ClientRequest_t);
status_t ReleaseLock(char *, char *, int);
status_t ReleaseClientLocks(char *, int);
LockTableNode_t *GetLock(char *,char *);
LockTableNode_t *AddLock(char *,char *, int, LockType_t);

int main(int argc, char *argv[])
{
	ServerStruct_t serverStruct;
	int recvMsgSize = 0;
	ClientRequest_t request;

	/* Initialize structures */
	clientTableList = NULL;
	lockTableList = NULL;
    memset(&serverStruct, 0, sizeof(ServerStruct_t));
    commFailureCounter = 0;

    printf("Sean Gatenby\nCSE531 Lab2 Server\ns");

    /* Initialize random number generator */
    srand(time(NULL));

    /* Validate arguments */
	if (argc == 2)
    {
		serverStruct.serverPortNumber = strtol(argv[1], NULL, 10); /* First arg: server port number (decimal number 1024-65535) */

		/* Create socket for sending/receiving datagrams */
		if ((serverStruct.sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) >= 0)
		{
			/* Construct local address structure */
			memset(&(serverStruct.serverAddr), 0, sizeof(serverStruct.serverAddr));   /* Zero out structure */
			serverStruct.serverAddr.sin_family = AF_INET;                /* Internet address family */
			serverStruct.serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
			serverStruct.serverAddr.sin_port = htons(serverStruct.serverPortNumber);      /* Local port */

			/* Bind to the local address */
			if (bind(serverStruct.sockfd, (struct sockaddr *) &(serverStruct.serverAddr), sizeof(serverStruct.serverAddr)) >= 0)
			{
				for (;;) /* Run forever */
				{
					/* Set the size of the in-out parameter */
				    socklen_t clientAddrLen = sizeof(serverStruct.clientAddr);

					/* Block until receive message from a client */
					if ((recvMsgSize = recvfrom(serverStruct.sockfd, &request, sizeof(ClientRequest_t), 0, (struct sockaddr *) &(serverStruct.clientAddr), &clientAddrLen)) == sizeof(ClientRequest_t))
					{
#ifdef DEBUG
						printf("%s:%d.%d_%d - %s", request.machineName, request.clientNumber, request.clientIncarnation, request.requestNumber, request.operation);
#endif
						/* Parse request */
						if(HandleRequest(serverStruct, request) == ERROR)
						{
							printError("Failed to process request: %s", request.operation);
						}
					}
					else
					{
						printErrno("Read %d bytes instead of %d", recvMsgSize, (int)sizeof(ClientRequest_t));
					}
				}
			}
			else
			{
				printErrno("Can't bind to port %d", serverStruct.serverPortNumber);
			}
		}
		else
		{
			printErrno("Can't create socket%s", "");
		}
    }
    else
    {
		printError("Usage: %s <service port>", argv[0]);
    }
}

status_t HandleRequest(ServerStruct_t serverStruct, ClientRequest_t request)
{
	status_t status = ERROR;
	status_t validArgs = ERROR;
	status_t gotLock = ERROR;
	status_t readyToTransmit = ERROR;
	char *commandString;
	char *fileNameString;
	char *modeString;
	char *numBytesString;
	char *messageString;
	char mode[3];
	int numBytes;
	RequestAction_t action;
	ClientTableNode_t *clientNode = NULL;
	LockTableNode_t *lockNode = NULL;
	LockType_t lockType = NO_LOCK;
	int bytesSent = 0;
	char filePath[100];

	/* Based on client table, determine what action to take as well
	 * as populating clientNode */
	action = ValidateClient(request, &clientNode);

	/* Only act on  */
	if(action == DROP_REQUEST_SEND_NOTHING)
	{
	    status = OK;
	}
	else if(action == SEND_STORED_RESPONSE)
    {
        readyToTransmit = OK;
    }
    /* PROCESS_REQUEST_SEND_RESPONSE and PROCESS_REQUEST_SEND_NOTHING */
    else
    {
        if((commandString = strtok(request.operation, " \r\n")) != NULL)
        {
            if((fileNameString = strtok(NULL, " \r\n")) != NULL)
            {
                /* Build file path */
                strcpy(filePath, request.machineName);
                strcat(filePath, ":");
                strcat(filePath, fileNameString);

                if(strcmp(commandString, "open") == 0)
                {
                    if((modeString = strtok(NULL, " \r\n")) != NULL)
                    {
                        /* Build the mode string and lock type */
                        if(strcmp(modeString, "read") == 0)
                        {
                            lockType = READ_LOCK;
                            strcpy(mode, "r");
                            validArgs = OK;
                        }
                        else if(strcmp(modeString, "write") == 0)
                        {
                            lockType = WRITE_LOCK;
                            strcpy(mode, "w");
                            validArgs = OK;
                        }
                        else if(strcmp(modeString, "readwrite") == 0)
                        {
                            lockType = READ_LOCK | WRITE_LOCK;
                            strcpy(mode, "r+");
                            validArgs = OK;
                        }
                        else
                        {
                            printError("Invalid open 'mode': %s", modeString);
                        }
                    }
                    else
                    {
                        printError("Invalid 'open' arguments: %s", request.operation);
                    }
                }
                else if(strcmp(commandString, "close") == 0)
                {
                    lockType = READ_LOCK | WRITE_LOCK;
                    validArgs = OK;
                }
                else if(strcmp(commandString, "read") == 0)
                {
                    if((numBytesString = strtok(NULL, " \r\n")) != NULL)
                     {
                         if((numBytes = strtol(numBytesString, NULL, 10)) > 0)
                         {
                             lockType = READ_LOCK;
                             validArgs = OK;
                         }
                         else
                         {
                             printError("Invalid read 'numBytes': %d", numBytes);
                         }
                     }
                    else
                    {
                        printError("Invalid 'read' arguments: %s", request.operation);
                    }
                }
                else if(strcmp(commandString, "write") == 0)
                {
                    if((messageString = strtok(NULL, "\"")) != NULL)
                     {
                        lockType = WRITE_LOCK;
                        validArgs = OK;
                     }
                    else
                    {
                        printError("Invalid 'write' arguments: %s", request.operation);
                    }
                }
                else if(strcmp(commandString, "lseek") == 0)
                {
                    if((numBytesString = strtok(NULL, " \r\n")) != NULL)
                    {
                        if((numBytes = strtol(numBytesString, NULL, 10)) > 0)
                        {
                            lockType = READ_LOCK | WRITE_LOCK;
                            validArgs = OK;
                        }
                        else
                        {
                            printError("Invalid lseek 'position': %d", numBytes);
                        }
                    }
                    else
                    {
                        printError("Invalid 'lseek' arguments: %s", request.operation);
                    }
                }
                else
                {
                    printError("Invalid command: %s\n", request.operation);
                }
            }
            else
            {
                printError("Invalid argument: %s\n", request.operation);
            }
        }
        else
        {
            printError("Invalid argument: %s\n", request.operation);
        }

        /* If args are OK, create lock and open file */
        if(validArgs == OK)
        {
            /* Check if any locks exist for the client and make sure the lockType supports the request */
            if((lockNode = GetLock(request.machineName, fileNameString)) != NULL)
            {
                if(lockNode->clientNumber == request.clientNumber)
                {
                    if((lockNode->lockStatus == lockType) ||
                       (strcmp(commandString, "close") == 0) ||
                       (strcmp(commandString, "lseek") == 0))
                    {
                        gotLock = OK;
                    }
                    else
                    {
                        clientNode->storedResponse.returnValue = ERROR;
                        snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Invalid lock type for %s operation\n", commandString);
                        printError("%s", clientNode->storedResponse.returnString);
                        clientNode->requestNumber = request.requestNumber;
                        readyToTransmit = OK;
                    }
                }
                else
                {
                    clientNode->storedResponse.returnValue = ERROR;
                    snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Can't get lock for %s:%s for client %d as %d has it already\n", lockNode->machineName, lockNode->fileName, request.clientNumber, lockNode->clientNumber);
                    printError("%s", clientNode->storedResponse.returnString);
                    clientNode->requestNumber = request.requestNumber;
                    readyToTransmit = OK;
                }
            }
            /* Create new lock for open commands only */
            else if((strcmp(commandString, "open") == 0))
            {
                if((lockNode = AddLock(request.machineName, fileNameString, request.clientNumber, lockType)) != NULL)
                {
                    gotLock = OK;
                }
                else
                {
                    clientNode->storedResponse.returnValue = ERROR;
                    snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Can't create lock for %s:%s for client %d\n", lockNode->machineName, lockNode->fileName, request.clientNumber);
                    printError("%s", clientNode->storedResponse.returnString);
                    clientNode->requestNumber = request.requestNumber;
                    readyToTransmit = OK;
                }
            }
            else
            {
                clientNode->storedResponse.returnValue = ERROR;
                snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "No lock found for %s:%s\n", lockNode->machineName, lockNode->fileName);
                printError("%s", clientNode->storedResponse.returnString);
                clientNode->requestNumber = request.requestNumber;
                readyToTransmit = OK;
            }

            if(gotLock == OK)
            {
                if(strcmp(commandString, "open") == 0)
                {
                    if(lockNode->fileHandle == NULL)
                    {
                        if((lockNode->fileHandle = fopen(filePath,mode)) != NULL)
                        {
                            clientNode->storedResponse.returnValue = OK;
                            snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Opened %s\n", filePath);
                        }
                        else
                        {
                            ReleaseLock(request.machineName, fileNameString, request.clientNumber);
                            clientNode->storedResponse.returnValue = ERROR;
                            snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Can't open %s: %s\n", filePath, strerror(errno));
                            printError("%s", clientNode->storedResponse.returnString);
                        }
                    }
                    else
                    {
                        ReleaseLock(request.machineName, fileNameString, request.clientNumber);
                        clientNode->storedResponse.returnValue = ERROR;
                        snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "File handle not NULL, is %s already open\n", filePath);
                        printError("%s", clientNode->storedResponse.returnString);
                    }

                    clientNode->requestNumber = request.requestNumber;
                    readyToTransmit = OK;
                }
                else if(strcmp(commandString, "close") == 0)
                {
                    if(lockNode->fileHandle != NULL)
                    {
                        if((fclose(lockNode->fileHandle)) == 0)
                        {
                            if(ReleaseLock(request.machineName, fileNameString, request.clientNumber) == OK)
                            {
                                clientNode->storedResponse.returnValue = OK;
                                snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Closed %s\n", filePath);
                            }
                            else
                            {
                                clientNode->storedResponse.returnValue = ERROR;
                                snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Can't release lock for %s for client %d\n", filePath, request.clientNumber);
                                printError("%s", clientNode->storedResponse.returnString);
                            }
                        }
                        else
                        {
                            clientNode->storedResponse.returnValue = ERROR;
                            snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Can't close %s: %s\n", filePath, strerror(errno));
                            printError("%s", clientNode->storedResponse.returnString);
                        }
                    }
                    else
                    {
                        clientNode->storedResponse.returnValue = ERROR;
                        snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "File handle NULL, is %s open?\n", filePath);
                        printError("%s", clientNode->storedResponse.returnString);
                    }

                    clientNode->requestNumber = request.requestNumber;
                    readyToTransmit = OK;
                }
                else if(strcmp(commandString, "read") == 0)
                {
                    if(lockNode->fileHandle != NULL)
                    {
                        char temp[2];
                        int bytesRead = 0;

                        memset(temp, 0, sizeof(temp));

                        strcpy(clientNode->storedResponse.returnString, "Read '");

                        for(int i = 0; i < numBytes; i++)
                        {
                            if((temp[0] = fgetc(lockNode->fileHandle)) != ERROR)
                            {
                                strcat(clientNode->storedResponse.returnString, temp);
                                bytesRead++;
                            }
                            else
                            {
                                break;
                            }
                        }

                        if(bytesRead == numBytes)
                        {
                            clientNode->storedResponse.returnValue = OK;
                            strcat(clientNode->storedResponse.returnString, "' from ");
                            strcat(clientNode->storedResponse.returnString, filePath);
                            strcat(clientNode->storedResponse.returnString, "\n");
                        }
                        else
                        {
                            clientNode->storedResponse.returnValue = ERROR;
                            snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Encountered EOF during read: only read %d bytes\n", bytesRead);
                            printError("%s", clientNode->storedResponse.returnString);
                        }
                    }
                    else
                    {
                        clientNode->storedResponse.returnValue = ERROR;
                        snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "File handle NULL, is %s open?\n", filePath);
                        printError("%s", clientNode->storedResponse.returnString);
                    }

                    clientNode->requestNumber = request.requestNumber;
                    readyToTransmit = OK;
                }
                else if(strcmp(commandString, "write") == 0)
                {
                    if(lockNode->fileHandle != NULL)
                    {
                        if(fputs(messageString, lockNode->fileHandle) != EOF)
                        {
                            clientNode->storedResponse.returnValue = OK;
                            snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Wrote '%s' to %s\n", messageString, filePath);
                        }
                        else
                        {
                            clientNode->storedResponse.returnValue = ERROR;
                            snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Can't write to %s\n", filePath);
                            printError("%s", clientNode->storedResponse.returnString);
                        }
                    }
                    else
                    {
                        clientNode->storedResponse.returnValue = ERROR;
                        snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "File handle NULL, is %s open?\n", filePath);
                        printError("%s", clientNode->storedResponse.returnString);
                    }

                    clientNode->requestNumber = request.requestNumber;
                    readyToTransmit = OK;
                }
                else if(strcmp(commandString, "lseek") == 0)
                {
                    if(lockNode->fileHandle != NULL)
                    {
                        if(fseek(lockNode->fileHandle, numBytes, SEEK_SET) == OK)
                        {
                            clientNode->storedResponse.returnValue = OK;
                            snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Moved %s file pointer to %d bytes from start\n", filePath, numBytes);
                        }
                        else
                        {
                            clientNode->storedResponse.returnValue = ERROR;
                            snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Can't move %s file pointer to %d bytes from start\n", filePath, numBytes);
                            printError("%s", clientNode->storedResponse.returnString);
                        }
                    }
                    else
                    {
                        clientNode->storedResponse.returnValue = ERROR;
                        snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "File handle NULL, is %s open?\n", filePath);
                        printError("%s", clientNode->storedResponse.returnString);
                    }

                    clientNode->requestNumber = request.requestNumber;
                    readyToTransmit = OK;
                }
                else
                {
                    clientNode->storedResponse.returnValue = ERROR;
                    snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "File handle NULL, is %s open?\n", filePath);
                    printError("%s", clientNode->storedResponse.returnString);
                    clientNode->requestNumber = request.requestNumber;
                    readyToTransmit = OK;
                }
            }
        }
        else
        {
            clientNode->storedResponse.returnValue = ERROR;
            snprintf(clientNode->storedResponse.returnString, sizeof(clientNode->storedResponse.returnString), "Invalid command arguments: %s\n", request.operation);
            printError("%s", clientNode->storedResponse.returnString);
            clientNode->requestNumber = request.requestNumber;
            readyToTransmit = OK;
        }

        /* Set done flag if we finished processing, but don't want to send anything */
        if((readyToTransmit == OK) && (action == PROCESS_REQUEST_SEND_NOTHING))
        {
            status = OK;
        }
    }

	/* Everything else checks out, but we haven't transmitted yet */
    if((status != OK) &&
       (readyToTransmit == OK))
    {
        /* Transmit response */
        if ((bytesSent = sendto(serverStruct.sockfd, &clientNode->storedResponse, sizeof(clientNode->storedResponse), 0, (struct sockaddr *) &(serverStruct.clientAddr), sizeof(serverStruct.clientAddr))) == sizeof(clientNode->storedResponse))
        {
            status = OK;
        }
        else
        {
            printErrno("Sent a different number of bytes than expected: %d instead of %d", bytesSent, (int)sizeof(clientNode->storedResponse));
        }
    }

	return status;
}

RequestAction_t ValidateClient(ClientRequest_t request, ClientTableNode_t **clientNode)
{
    ClientTableNode_t *tempNode = NULL;
    RequestAction_t action = DROP_REQUEST_SEND_NOTHING;

    /* Client with same machine name and client number is already in the list */
    if((tempNode = GetClient(request)) != NULL)
    {
        /* Client crashed! */
        if(request.clientIncarnation != tempNode->clientIncarnation)
        {
#ifdef DEBUG
            printf("%s:%d.%d_%d - Client Crashed: Deleting Client Entry, Freeing Locks\n", request.machineName, request.clientNumber, request.clientIncarnation, request.requestNumber);
#endif
            /* Remove all locks associated with that machine */
            ReleaseClientLocks(tempNode->machineName, tempNode->clientNumber);

            if(DeleteClient(tempNode->machineName, tempNode->clientNumber) != OK)
            {
                printError("Can't remove client entry: %s:%d", tempNode->machineName, tempNode->clientNumber);
            }

#ifdef DEBUG
            printf("%s:%d.%d_%d - New Client: Process Request, Send Response\n", request.machineName, request.clientNumber, request.clientIncarnation, request.requestNumber);
#endif
            tempNode = AddClient(request);
            action = PROCESS_REQUEST_SEND_RESPONSE;
        }
        else
        {
            /* Stale request, drop it */
            if(request.requestNumber < tempNode->requestNumber)
            {
#ifdef DEBUG
                printf("%s:%d.%d_%d - Stale Request: Drop Request, Send Nothing\n", request.machineName, request.clientNumber, request.clientIncarnation, request.requestNumber);
#endif
                action = DROP_REQUEST_SEND_NOTHING;
            }

            /* Client requesting duplicate request, send stored response */
            else if(request.requestNumber == tempNode->requestNumber)
            {
#ifdef DEBUG
                printf("%s:%d.%d_%d - Duplicate Request: Send Stored Response\n", request.machineName, request.clientNumber, request.clientIncarnation, request.requestNumber);
#endif
                action = SEND_STORED_RESPONSE;
            }

            /* Simulate com failure */
            else if(request.requestNumber > tempNode->requestNumber)
            {
                switch(rand()%3)
                {
                    case 0:
                    {
#ifdef DEBUG
                        printf("%s:%d.%d_%d - Comm Failure: Drop Request, Send Nothing\n", request.machineName, request.clientNumber, request.clientIncarnation, request.requestNumber);
#endif
                        action = DROP_REQUEST_SEND_NOTHING;
                        break;
                    }
                    case 1:
                    {
#ifdef DEBUG
                        printf("%s:%d.%d_%d - Comm Failure: Process Request, Send Nothing\n", request.machineName, request.clientNumber, request.clientIncarnation, request.requestNumber);
#endif
                        action = PROCESS_REQUEST_SEND_NOTHING;
                        break;
                    }
                    case 2:
                    {
#ifdef DEBUG
                        printf("%s:%d.%d_%d - Comm Failure: Process Request, Send Response\n", request.machineName, request.clientNumber, request.clientIncarnation, request.requestNumber);
#endif
                        action = PROCESS_REQUEST_SEND_RESPONSE;
                        break;
                    }
                }

                /* Increment counter */
                commFailureCounter++;
            }
        }
    }
    /* No client with the requested machine name and client number exists in the list */
    else
    {
#ifdef DEBUG
            printf("%s:%d.%d_%d - New Client: Process Request, Send Response\n", request.machineName, request.clientNumber, request.clientIncarnation, request.requestNumber);
#endif
        tempNode = AddClient(request);
        action = PROCESS_REQUEST_SEND_RESPONSE;
    }

    *clientNode = tempNode;

    return action;
}

/* NOTE: getClientNode MUST have been called previously and returned NULL */
ClientTableNode_t *AddClient(ClientRequest_t request)
{
    ClientTableNode_t *newNode = NULL;

    if((newNode = malloc(sizeof(ClientTableNode_t))) != NULL)
    {
        memset(newNode, 0, sizeof(ClientTableNode_t));

        /* Initialize new client node */
        strcpy(newNode->machineName, request.machineName);
        newNode->clientNumber = request.clientNumber;
        newNode->requestNumber = request.requestNumber;
        newNode->clientIncarnation = request.clientIncarnation;

        /* Append node */
        if(clientTableList != NULL)
        {
            ClientTableNode_t *tempNode = clientTableList;

            while(tempNode->next != NULL)
            {
                tempNode = tempNode->next;
            }

            tempNode->next = newNode;
        }
        else
        {
            clientTableList = newNode;
        }
    }
    else
    {
        printErrno("Malloc failed%s", "");
    }

    return newNode;
}

status_t DeleteClient(char *machineName, int clientNumber)
{
    ClientTableNode_t *prevNode = NULL;
    ClientTableNode_t *tempNode = clientTableList;
    status_t status = ERROR;
    bool isNodeFound = false;

    /* Search for node with matching machine name and client number */
    while(tempNode != NULL)
    {
        /* Check machineName and clientNumber */
        if((strcmp(tempNode->machineName, machineName) == 0) &&
           (tempNode->clientNumber == clientNumber))
        {
            isNodeFound = true;

            /* Deleting root node */
            if(prevNode == NULL)
            {
                clientTableList = tempNode->next;
                free(tempNode);
                status = OK;
                break;
            }
            /* Deleting other node */
            else
            {
                prevNode->next = tempNode->next;
                free(tempNode);
                status = OK;
                break;
            }
        }
        else
        {
            prevNode = tempNode;
            tempNode = tempNode->next;
        }
    }

    if(isNodeFound == false)
    {
        printInfo("Client %d on machine %s doesnt exist", clientNumber, machineName);
    }

    return status;
}

ClientTableNode_t *GetClient(ClientRequest_t request)
{
    ClientTableNode_t *tempNode = clientTableList;
    ClientTableNode_t *node = NULL;

    /* Search for node with matching machine name and client number */
    while(tempNode != NULL)
    {
        /* Check machineName and clientNumber */
        if((strcmp(tempNode->machineName, request.machineName) == 0) &&
           (tempNode->clientNumber == request.clientNumber))
        {
            node = tempNode;
            break;
        }
        tempNode = tempNode->next;
    }

    return node;
}

status_t ReleaseLock(char *machineName, char *fileName, int clientNumber)
{
    LockTableNode_t *prevNode = NULL;
    LockTableNode_t *tempNode = lockTableList;
    status_t status = ERROR;

    /* Search for node with matching machine name and client number */
    while(tempNode != NULL)
    {
        /* Check machineName and clientNumber */
        if((strcmp(tempNode->machineName, machineName) == 0) &&
           (strcmp(tempNode->fileName, fileName) == 0))
        {
            if(tempNode->clientNumber == clientNumber)
            {
                /* Deleting root node */
                if(prevNode == NULL)
                {
                    lockTableList = tempNode->next;
                    free(tempNode);
                    status = OK;
                    break;
                }
                /* Deleting other node */
                else
                {
                    prevNode->next = tempNode->next;
                    free(tempNode);
                    status = OK;
                    break;
                }
            }
            else
            {
                printError("Client %d attempting to delete lock for %s:%s which is owned by client %d", clientNumber, tempNode->machineName, tempNode->fileName, clientNumber);
            }
        }
        prevNode = tempNode;
        tempNode = tempNode->next;
    }

    return status;
}


status_t ReleaseClientLocks(char *machineName, int clientNumber)
{
    LockTableNode_t *prevNode = NULL;
    LockTableNode_t *tempNode = lockTableList;
    status_t status = ERROR;

    /* Search for node with matching machine name and client number */
    while(tempNode != NULL)
    {
        /* Check machineName and clientNumber */
        if(strcmp(tempNode->machineName, machineName) == 0)
        {
            if(tempNode->clientNumber == clientNumber)
            {
                /* Deleting root node */
                if(prevNode == NULL)
                {
                    free(tempNode);
                    lockTableList = lockTableList->next;
                    tempNode = lockTableList;
                    status = OK;
                }
                /* Deleting other node */
                else
                {
                    prevNode->next = tempNode->next;
                    free(tempNode);
                    tempNode = prevNode->next;
                    status = OK;
                }
            }
            else
            {
                //printError("Client %d attempting to delete lock for %s:%s which is owned by client %d", clientNumber, tempNode->machineName, tempNode->fileName, tempNode->clientNumber);
            }
        }
        prevNode = tempNode;
        tempNode = tempNode->next;
    }

    return status;
}


/* Check if anyone has a lock on a particular machine:file.
 * The caller must handle differentiating between other client's
 * locks, and it's own locks as well as lockType */
LockTableNode_t *GetLock(char *machineName,char *fileName)
{
    LockTableNode_t *tempNode = lockTableList;

    /* Search for node with matching machine name and client number */
    while(tempNode != NULL)
    {
        /* Check machineName and clientNumber */
        if((strcmp(tempNode->machineName, machineName) == 0) &&
           (strcmp(tempNode->fileName, fileName) == 0))
        {
            break;
        }
        tempNode = tempNode->next;
    }

    return tempNode;
}

LockTableNode_t *AddLock(char *machineName,char *fileName, int clientNumber, LockType_t lockType)
{
    LockTableNode_t *newNode = NULL;

    if((newNode = malloc(sizeof(LockTableNode_t))) != NULL)
    {
        memset(newNode, 0, sizeof(LockTableNode_t));

        /* Initialize new lock node */
        strcpy(newNode->machineName, machineName);
        strcpy(newNode->fileName, fileName);
        newNode->clientNumber = clientNumber;
        newNode->lockStatus = lockType;

        /* Append node */
        if(lockTableList != NULL)
        {
            LockTableNode_t *tempNode = lockTableList;

            while(tempNode->next != NULL)
            {
                tempNode = tempNode->next;
            }

            tempNode->next = newNode;
        }
        else
        {
            lockTableList = newNode;
        }
    }
    else
    {
        printErrno("Malloc failed%s", "");
    }

    return newNode;
}
