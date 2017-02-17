#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>

#define ECHOCMD "echo "
#ifdef __APPLE__
#define MD5SUM " | md5 -q"
#else
#define MD5SUM " | md5sum"
#endif

#define MD5SUMLEN 32

#define MAXNUMCONNECTIONS 20
#define CACHEFOLDER "cache"
#define GETCMD "GET"
#define CMDERR "HTTP/1.1 501 GET is the only command supported by the proxy.\r\n"
#define TESTBUFFERMAX 1000 //TODO: get rid of this!
#define MAXRECVBUFFERSIZE 10000 //TODO: get rid of this!

//TODO: telnet is acting weird
//TODO: look for memory leak

int proxySocket;
struct timeval timeout;

typedef struct http_request {
    char *cmd, *fullPath, *service, *url, *port, *version, *body;
} http_request;

int recv_whole_msg(int sock, char** wholeMessage)
{
	printf("this is some stuff to print2\n");
    //populate the timeout struct
    bzero(&timeout, sizeof(timeout));
    timeout.tv_sec = 60;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));

    int currentMessageLength = 0;
    int currentBytesRecieved = 0;

    *wholeMessage = calloc(MAXRECVBUFFERSIZE, sizeof(char));
    currentMessageLength = MAXRECVBUFFERSIZE;
    bzero(*wholeMessage, currentMessageLength);

    int bytesReceived;
    if((bytesReceived = recv(sock, *wholeMessage, currentMessageLength - 1, 0)) < 0)
    {
        if(errno == EAGAIN || errno == EWOULDBLOCK)
        {
            perror("Timeout trying to receive the whole message");
            return -1;
        }
    }

    currentBytesRecieved = bytesReceived;

