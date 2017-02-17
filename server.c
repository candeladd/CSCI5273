/*********************************************************************
 * Andrew Candelaresi
 * University of Colorado
 * Network Systems Fall 2016
 * 
 * Help from Taylor J Andrews
 * Refrence Tinyhttp
 * *****************************************************************/

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <ctype.h>

#define BACKLOG 10
#define SERVER_STRING "Server: simpleHTTP\r\n"
#define BUFFER_SIZE 1024

struct conf_t {
	char port[5];
	char root[BUFFER_SIZE];
	char home[BUFFER_SIZE];
	char data[BUFFER_SIZE];
	char kt[BUFFER_SIZE];
} config;

int bindSocket();
void* threadRequest(void* client);
void error_is(const char *sc);
char * capturestr(char* search, char * source, char * word, int i, int j);
int errorResponse(int client, const char *errstr, char *verstr);
void sendResponse(int client, char* request, char* uri,  char *version, char *rest);
int pipeline(int client,char*rest1);
char* getFileType(char* path);


/*********************************************************************
 * takes uri from http request and compares it against
 * config.data to find the correct file type for response
 * *****************************************************************/
char* getFileType(char* path)
{
	if (strcmp(path, "/") == 0)
	{
		printf("text/html\n");
		return "text/html";
	}
	else
	{
		char *file_data = strdup(config.data);
		
		char *line = strtok(file_data, "\n");
		while(line)
		{
			
			char in[BUFFER_SIZE];
			static char out[20];
			memset(&in[0], 0, sizeof(in));
			memset(&out[0], 0, sizeof(out));
			sscanf(line, "%s %s", in, out);
			
			if (strstr(path, in) != NULL)
			{
				return out;
			}
			
			line = strtok(NULL, "/n");
		}
		free(file_data);
		
		
	}
	
	return "0";
}


/*********************************************************************
 * Read config file info
 * call capture string to get values
 * to populate the config array
 * *****************************************************************/
void setConfig()
{
		/* open the config file and read the deets*/
	FILE *fp;
	char buffer1[BUFFER_SIZE];
	char buffer2[BUFFER_SIZE];
	char buffer3[BUFFER_SIZE];
	char buffer4[BUFFER_SIZE];
	char buffer5[BUFFER_SIZE];
	int bytes_read;
	char*source = NULL;
	if ((fp=fopen("ws.conf", "r"))==NULL)
	{
		error_is("Missing Configuration File ");
		exit(1);
	}
	if (fp != NULL) {
		/* Go to the end of the file. */
		if (fseek(fp, 0L, SEEK_END) == 0) 
		{
			/* Get the size of the file. */
			long bufsize = ftell(fp);
			if (bufsize == -1) 
			{ 
				perror("buffsize:");
			}

			/* Allocate our buffer to that size. */
			source = malloc(sizeof(char) * (bufsize + 1));

			/* Go back to the start of the file. */
			if (fseek(fp, 0L, SEEK_SET) != 0) 
			{
				perror("LF:");
			}

			/* Read the entire file into memory. */
			size_t newLen = fread(source, sizeof(char), bufsize, fp);
			if ( ferror( fp ) != 0 ) {
				fputs("Error reading file", stderr);
			} else {
				source[newLen++] = '\0'; /* Just to be safe. */
			}
		}
		
	fclose(fp);
	}
	
    char search[] = "Listen ";
	capturestr(search, source,buffer1, 7, 11);
	strcpy(config.port, buffer1);

	char search2[] = "DocumentRoot ";	
	capturestr(search2, source, buffer2, 14,49);
	strcpy(config.root, buffer2);
    
    char search3[] = "DirectoryIndex ";
	capturestr(search3, source, buffer3, 15, 44);
	strcpy(config.home, buffer3);
	
    char search4[] = "handles";
    capturestr(search4, source, buffer4, 8, 152);
	strcpy(config.data, buffer4);
	
	char search5[] = "keepaliveTime";
    capturestr(search5, source, buffer5, 14, 16);
	strcpy(config.kt, buffer5);
	
	free(source);	
}

/**********************************************************************
 * build error response for http and send to clients
 * 
 * *******************************************************************/
int errorResponse(int client, const char *errstr, char *reason)
{	
	char error[BUFFER_SIZE];
	memset(&error[0], 0, sizeof(error));
	strcpy(error, errstr);
	strcat(error, "Conten-Type: text/html\n\n");
	
	char buf[BUFFER_SIZE];
	memset(&buf[0], 0, sizeof(buf));
	sprintf(buf, "<html>\n<body>\n <p>%s ",reason);
	strcat(buf, " </p>\n</body>\n</html>\n");
	strcat(error, buf);

	strcat(error, "\n");
	char ret[strlen(error)+1];
	memset(&ret[0], 0, sizeof(ret));
	/*copy the message to ret so we dont send more than 
	 * is needed */
	strncpy(ret, error, strlen(error));
	printf("%s\n", error);
	if(send(client, ret, sizeof(ret), 0) > 0)
	{
		send(client, "\n", 0 , 2);

		return 0;
	}	
}


/******************************
 * prints out error message with 
 * relavant string
 * **************************/
void error_is(const char *sc)
{
 perror(sc);
 exit(1);
}
/********************************************************************
 * check to see if  HTTO version is valid, and that is is a valid 
 * HTTP  request 
 * 
 * *******************************************************************/
int Verror(int client, char* request, char* uri, char* version)
{
	if((strcmp(request, "GET") ==0) 
		|| (strcmp(request, "HEAD") == 0)
		|| (strcmp(request, "POST") == 0)
		|| (strcmp(request, "PUT") == 0)
		|| (strcmp(request, "DELETE") == 0)
		|| (strcmp(request, "CONNECT") == 0)
		|| (strcmp(request, "OPTIONS") == 0)
		|| (strcmp(request, "TRACE") == 0))
	{
		char http[BUFFER_SIZE];
		char Vnum[BUFFER_SIZE];
		memset(&http[0], 0, sizeof(http));
		memset(&Vnum[0], 0, sizeof(Vnum));
		strncpy(http, version, 5);
		strncpy(Vnum, version+5, 3);
		
		if (strcmp(http, "HTTP/") ==0)
		{
			if ((strcmp(Vnum, "1.0") == 0) 
				|| (strcmp(Vnum, "1.1") == 0)
				|| (strcmp(Vnum, "2") == 0))
			{
				return 0;
			}
			else
			{	
				printf("here is the error\n");
				char error[] = "HTTP/1.1 400 BAD REQUEST\n";
				char version_error[BUFFER_SIZE];
				memset(&version_error[0], 0, sizeof(version_error));
				strcpy(version_error, "HTTP/1.1 400 Bad Request Reason: Invalid HTTP-Version: ");
				strcat(version_error, version);
				if(errorResponse(client, error, version_error) == 0)
				{
					return -1;
				}	
			} 
		} 
		else
		{
			char error[] = "HTTP/1.1 400 Bad Request\n";
			char version_error[BUFFER_SIZE];
			memset(&version_error[0], 0, sizeof(version_error));
			strcpy(version_error, "HTTP/1.1 400 Bad Request Reason: Invalid HTTP-Version: ");
			strcat(version_error, version);
			if(errorResponse(client, error, version_error) == 0)
			{
				return -1;
			}
		}	
			
	}
	else
	{
		char error[] = "HTTP/1.1 400 Bad Request\n";
		char version_error[BUFFER_SIZE];
		memset(&version_error[0], 0, sizeof(version_error));
		strcpy(version_error, "HTTP/1.1 400 Bad Request Reason: Invalid Method: ");
		strcat(version_error, request);
		if(errorResponse(client, error, version_error) == 0)
		{
			return -1;
		}
	}
}

/********************************************************************
 *does our real heavy lifting forms proper response to request
 * or send appropriate error message.  
 * 
 * *******************************************************************/
void sendResponse(int sockfd, 
				  char *request1, 
				  char *uri1, 
				  char *version1,
				  char *rest)
{
	int err;
	struct timeval t;
	t.tv_sec = 10;
	int c_alive;
	int found = 0;
	char request[255];
	memset(&request[0], 0, sizeof(request));
	strcpy(request, request1);
	char uri[255];
	memset(&uri[0], 0, sizeof(uri));
	strcpy(uri, uri1);
	char version[255];
	memset(&version[0], 0, sizeof(version));
	strcpy(version, version1);
	
	err = Verror(sockfd, request, uri, version);
	if(err == 0)
	{	
		/* Follow the http request */
		if(strstr(rest, "Connection: keep-alive"))
		{
			
			t.tv_sec = 10;
			c_alive = 1;
			
		}
		else
		{
			t.tv_sec = 0;
			c_alive = 0;
		}
		if ((strcmp(request, "GET") == 0))
		{
			
			
			int pathlen = strlen(config.root + strlen(uri));
			char path[pathlen];
			int file_flag = 0;
			char ok[17] = "HTTP/1.1 200 OK\r\n" ;
			FILE *fp;
			
			strcpy(path, config.root);
			strcat(path, uri);
			
			if(strcmp(uri, "/")==0)
			{
				char* type = strtok(config.home, " ");
				//search for the right html format
				while(type != NULL)
				{
					strcat(path, type);
					fp = fopen(path, "rb");
					if (fp != NULL)
					{
						//the file exists
						found = 1;
						break;
					}
					
					type =strtok(NULL, " ");
					//didnt find the right file clear, search again
					memset(&path[0], 0, sizeof(path));
					strcpy(path, config.root);
					strcat(path, uri);
					
				}
			}
			else
			{
				fp = fopen(path, "rb");
				if(fp!=NULL)
				{
					found = 1;
				}
			}
			if(!found)
			{
				
				char errorF[BUFFER_SIZE];
				memset(&errorF[0], 0, sizeof(errorF));					
				strcpy(errorF, "HTTP/1.1 404 Not Found\n");
				strcat(errorF, "Conten-Type: text/html\n\n");
				
				char buf[BUFFER_SIZE];
				memset(&buf[0], 0, sizeof(buf));
				strcat(buf, "<html>\n<body>\n <p>404 Not Found Reason URL does not exist : ");
				strcat(buf, uri);
				strcat(buf, " </p>\n</body>\n</html>\n");
				strcat(errorF, buf);
			
				printf("%s\n", errorF);
				
				char ret[strlen(errorF)+1];
				memset(&ret[0], 0, sizeof(ret));
				strncpy(ret, errorF, strlen(errorF));					
				send(sockfd,ret, sizeof(ret), 0);
				send(sockfd, "\n", 0 , 2);
				/*
				char error[] = "HTTP/1.1 404 NOT FOUND\n";
				char reason[BUFFER_SIZE];
				memset(&reason[0], 0, sizeof(reason));
				sprintf(reason,"404 Not Found Reason URL does not exist : %s", uri);
				errorResponse(sockfd, error, reason);
				*/
			}
			else
			{
				char c_len[BUFFER_SIZE];
				char response[BUFFER_SIZE];
				
				char* file_type = getFileType(uri);
				
				if(strcmp(file_type, "0") == 0)
				{
					char error[] = "HTTP/1.1 501 Not Implemented";
					char reason[BUFFER_SIZE];
					memset(&reason[0], 0, sizeof(reason));
					sprintf(reason,"501 Not Implemented %s : %s", request, uri);
					errorResponse(sockfd, error, reason);
					
				}
				
				else
				{
					//ok
					//c_type
					//c_len
					//c_alive?
					//f_content
					char c_type[50] = "Content-Type: ";
					strcat(c_type, file_type);
					strcat(c_type, "\r\n");
					
					//printf("%s\n", c_type);
					long file_len;
					if (fseek(fp, 0L, SEEK_END) == 0) 
					{
						/* Get the size of the file. */
						file_len = ftell(fp);
						if (file_len == -1) 
						{ 
							perror("file_len");
						}

						/* Allocate our buffer to that size. */
						//source = malloc(sizeof(char) * (bufsize + 1));

						/* Go back to the start of the file. */
						if (fseek(fp, 0L, SEEK_SET) != 0) 
						{
							perror("LF:");
						}
					}
					sprintf(c_len, "Content-Length: %ld\n", file_len);
					
					strcat(response, ok);
					strcat(response, c_type);
					strcat(response, c_len);
					if(c_alive == 1)
					{
						//printf("keep-alive= %d\n", c_alive);
						strcat(response, "Connection: keep-alive\n");
					}
					else
					{
						strcat(response, "Connection: Close\n");
					}
					strcat(response, "\r\n");
					printf("response: %s\n", response);
					
					send(sockfd, response, strlen(response), 0);
					
					char* buf = 0;
					buf = malloc(BUFFER_SIZE);
					size_t bytes_read = 0;
					//printf("%s\n", path);
					
					while((bytes_read = fread(buf, 1, BUFFER_SIZE, fp)) > 0)
					{		
						write(sockfd, buf, (int)bytes_read);
					}
					
					send(sockfd, "\r\n", 0 , 2);
					
					free(buf);
					if (c_alive == 0)
					{
						fclose(fp);
					}
				}
			}	
		}
		else
		{
			char error[] = "HTTP/1.1 501 Not Implemented\n";
			char reason[BUFFER_SIZE];
			memset(&reason[0], 0, sizeof(reason));
			sprintf(reason,"501 Not Implemented %s : %s",request, uri);
			errorResponse(sockfd, error, reason);
		}
	}
}
/********************************************************************
 * parses the buffer and puts the proper pieces into the 
 * char arrays to feed to send request
 * *******************************************************************/
void* threadRequest(void* client)
{
	int sockfd = (intptr_t) client;
	char buffer[1024];
	
	char request[255];
	char uri[255];
	char version[255];
	int chars_read;
	size_t i, j;
	struct stat statbuff;
	int err;
	struct timeval t;
	t.tv_sec = atoi(config.kt);
	int c_alive;
	int found = 0;
	char rest[512];
	
	
	if(chars_read = recv(sockfd, &buffer, BUFFER_SIZE, 0) >0)
	{
		//printf("buffer= %s\n\n", buffer);
		memset(&request[0], 0, sizeof(request));
		memset(&uri[0], 0, sizeof(uri));
		memset(&version[0], 0, sizeof(version));
		
		sscanf(buffer, "%s %s %s %[^\t]", request, uri, version,rest);
		//printf("request: %s\n", request);
		//printf("uri:%s\n", uri);
		//printf("version: %s\n\n", version);
		
		sendResponse(sockfd, request, uri, version, rest);
		
		pipeline(sockfd, rest);
	}
	printf("thread complete\n\n");
	close(sockfd);
	
}
/**********************************************************************
 * function checks if there is another get request in the reamaining
 * buffer after is has been parsed once.
 * if found it calls send request again then calls itself to 
 * check again 
 * ******************************************************************/
int pipeline(int client, char*rest1)
{
	const char *ptr1;
	char rest[512];
	char request[255];
	char version[255];
	char uri[255];
	memset(&request[0], 0 , sizeof(request));
	memset(&uri[0], 0, sizeof(uri));
	memset(&version[0], 0, sizeof(version));
	memset(&rest[0], 0, sizeof(rest));
	memset(&rest[0], 0, sizeof(rest));
	strcpy(rest, rest1);
	if((ptr1=strstr(rest, "GET"))!=NULL)
	{
		char *result = strstr(rest, "/1.1");
		int position = result - ptr1;
		int len = strlen(ptr1) - position;
		char *res = (char*)malloc(sizeof(char)*(len+1));
		memcpy(res, ptr1, len);
		printf("%s\n", res);
		sscanf(res, "%s %s %s %[^\t]", request, uri, version, rest);
		free(res);
		sendResponse(client, request, uri, version,rest);
	}
	else
	{
		return 0;
	}
	pipeline(client, rest);
}


/********************************************************************
 * using the config find infor for the struct
 * *****************************************************************/
char * capturestr(char* search, char* source, char * word, int i, int j)
{
	
	const char *p1 = strstr(source, search)+i;
	
	const char *p2 = strstr(p1, search)+j;
	/*printf("%s\n", search);*/
	size_t len = j-i;
	char *res = (char*)malloc(sizeof(char)*(len+1));
	memcpy(res, p1, len);
	res[len] = '\0';
	strcpy(word, res);
	free(res);
	/*printf("%s\n", word);*/
	return word;
}

/**********************************************************************
 * create a socket file descriptor
 * and bind and listen on it
 * *******************************************************************/
int bindSocket()
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;\
	socklen_t sin_size;
	char s[INET6_ADDRSTRLEN];
	int va;
	char PORT[5]; //get from config file
	int yes=1;
	
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;  // IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; //a TCP connection
	hints.ai_flags = AI_PASSIVE; // get my own IP
	
	if ((va = getaddrinfo(NULL, config.port, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "gettaddrinfo: %s\n", gai_strerror(va));
		return 1;
	}
	
	    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            error_is("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            error_is("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            error_is("server: bind");
            continue;
        }

        break;
    }
	freeaddrinfo(servinfo);  //no longer need this struct

	if(p==NULL)
	{
		fprintf(stderr, "server:failed to bind\n");
		exit(1);
	}
	
	if (listen(sockfd, BACKLOG) == -1)
	{
		error_is("listen");
		exit(1);
	}
	
	return sockfd;
}



/********************************
 * Main Function
 * *****************************/
int main(void)
{
	int new_fd;  //sock_fd listen on, use new_fd for new connections	
	struct sockaddr_storage their_addr;  // clients address		
	int client_sock = -1;
	struct sockaddr_in client_name;
	int client_name_len = sizeof(client_name);
	
	
	setConfig();
	//printf("%s\n", config.port);
	//printf("%s\n", config.root);
	//printf("%s\n", config.home);
	//printf("%s\n", config.kt);
	
	int sockfd = bindSocket();

	pthread_t newthread;	
	
	printf("server waiting for connection...\n\n");
    while (1)
	{
		client_sock = accept(sockfd,
							(struct sockaddr *)&client_name,
							&client_name_len);
		/*
		if(client_sock < 1024)
		{
			error_is("bad client socket");
		}*/
		if (client_sock == -1)
		{
			error_is("accept");
			exit(EXIT_FAILURE);
		}
		
		if (pthread_create(&newthread , NULL, threadRequest, (void*) (intptr_t) client_sock) != 0)
		{
			perror("pthread_create");
		}
		
	}
	close(sockfd);
    return 0;

}
