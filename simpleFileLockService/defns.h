#include <stdio.h>      /* for printf() and fprintf() */
#include <errno.h>      /* for errno */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */

#define printError(errorMsg, ...) fprintf(stderr, "Error: %s:%d %s() - " errorMsg "\n", __FILE__, __LINE__, __func__, __VA_ARGS__)
#define printWarning(errorMsg, ...) fprintf(stderr, "Warning: %s:%d %s() - " errorMsg "\n", __FILE__, __LINE__, __func__, __VA_ARGS__)
#define printInfo(errorMsg, ...) fprintf(stderr, "Info: %s:%d %s() - " errorMsg "\n", __FILE__, __LINE__, __func__, __VA_ARGS__)

#define printErrno(errorMsg, ...) fprintf(stderr, "Error: %s:%d %s() - " errorMsg ":%s\n", __FILE__, __LINE__, __func__, __VA_ARGS__, strerror(errno))

#define INCARNATION_LOCKFILE "incarnation_LOCK_"
#define INCARNATION_FILE "incarnation_"

#define MAX_CMD_LEN 200

typedef int bool;
#define true 1
#define false 0

typedef struct ClientRequest_t
{
	char machineName[100]; /* Name of machine on which client is running */
	int clientNumber;      /* Client number */
	int requestNumber;     /* Request number of client */
	int clientIncarnation; /* Incarnation number of client’s machine */
	char operation[200];   /* File operation (actual request) client sends to server */
}ClientRequest_t;

typedef struct ServerResponse_t
{
    int returnValue;         /* Integer return value of the operation */
    char returnString[1024]; /* Ascii string associated with the return value */
}ServerResponse_t;

typedef struct ClientStruct_t
{
    int sockfd;                    /* Socket descriptor */
    struct sockaddr_in serverAddr; /* Server address */
    struct sockaddr_in clientAddr; /* Client address */
    char *serverIpAddress;         /* Server IP address */
    char *machineName;             /* Client machine name */
    int serverPortNumber;          /* Server port number */
    char *scriptFileName;          /* Full path to script */
	int clientNumber;              /* Client number */
	int requestNumber;             /* Current request number */
	int numCommands;               /* Number of commands in the script */
	int clientIncarnation;         /* Current incarnation number of client */
	char **commandArray;           /* Array of commands to be sent */
}ClientStruct_t;

typedef struct ServerStruct_t
{
    int sockfd;                    /* Socket descriptor */
    struct sockaddr_in serverAddr; /* Server address */
    struct sockaddr_in clientAddr; /* Client address */
    int serverPortNumber;          /* Server port number */
}ServerStruct_t;

typedef struct ClientTableNode_t
{
	struct ClientTableNode_t *next;  /* Pointer to next entry in the list */
    char machineName[100];           /* Client machine name */
	int clientNumber;                /* Client number */
	int requestNumber;               /* Current request number */
	int clientIncarnation;           /* Current incarnation number of client */
	ServerResponse_t storedResponse; /* Result of the last operation */
}ClientTableNode_t;


typedef enum LockType_t
{
	NO_LOCK         = 0,
	READ_LOCK       = 1,
	WRITE_LOCK      = 2,
}LockType_t;

typedef enum RequestAction_t
{
    DROP_REQUEST_SEND_NOTHING     = 0, /* r < R or first third of r > R requests */
    PROCESS_REQUEST_SEND_NOTHING  = 1, /* Second third of r > R requests */
    PROCESS_REQUEST_SEND_RESPONSE = 2, /* If client is new, or last third of r > R requests */
    SEND_STORED_RESPONSE          = 3  /* r = R */
}RequestAction_t;

typedef struct LockTableNode_t
{
	struct LockTableNode_t *next;
	char fileName[200];
	char machineName[100];
	int clientNumber;
	LockType_t lockStatus;
	FILE *fileHandle;
}LockTableNode_t;


typedef enum status_t
{
	OK =     0,
	ERROR = -1
}status_t;
