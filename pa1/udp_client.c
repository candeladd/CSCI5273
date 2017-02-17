#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <sys/sendfile.h>

#define MAXBUFSIZE 100


int main (int argc, char * argv[])
{
	
	
	int nbytes;                             
	char buf[MAXBUFSIZE];
	size_t buflen;
	ssize_t bytes_written;
	int sockfd;                             //this will be our socket
	struct addrinfo hints, *servinfo, *p;
	int rv, val, i, of, bytes_read;
	char string1[30];
	int fp, cf;
	char command[100];
	char filename[30];
	struct sockaddr_in from_addr;
	int addr_len = sizeof(struct sockaddr);
	char msg[10240];
	int msglen = sizeof(msg);
	
	
	if (argc < 3)
	{
		printf("USAGE:  <server_ip> <server_port>\n");
		exit(1);
	}
	
	
	/******************
	  Here we populate a servinfo struct with
	  information regarding where we'd like to send our packet 
	  i.e the Server.
	 ******************/
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	//Causes the system to create a generic socket of type UDP (datagram)
	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo:%s\n", gai_strerror(rv));
		return 1;
	}
	
	//loop through the results from getaddrinfo and make a socket
	for (p= servinfo; p!= NULL; p = p->ai_next)
	{
		if ((sockfd= socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
				perror("talker: socket");
				continue;
		}
		
		break;
	}
	
	if (p== NULL)
	{
		fprintf(stderr, "talker: failed to creat socket\n");
		return 2;
	}
	
	/******************
	  sendto() sends immediately.  
	  it will report an error if the message fails to leave the computer
	  however, with UDP, there is no error if the message is lost in the network once it leaves the computer.
	 ******************/
	// run this loop until exit command is given
	while(1)
	{
		bzero(command, sizeof command);
		printf("Please type one of the following commands: \n1. get[file_name]\n2. put[file_name]\n3. ls\n4. exit\n");
		fgets (command, 100, stdin);
		
		val = 0;
		strcpy(string1, command);
		if ((nbytes = sendto(sockfd, command, strlen(command), 0, p->ai_addr, p->ai_addrlen)) == -1)
		{
			perror("talker: sendto");
			exit(1);
		}
		printf("sent %d bytes\n", nbytes);
		// Blocks till bytes are received
		i=1;
		
		if ((val = strncmp(string1, "get", 3)) == 0)
		{
			memmove(filename, string1+3, strlen (buf));
			printf("string = %s\n", filename);
			
			fp = open(filename, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			if (cf == -1) {
				fprintf(stderr, "Can't open input file %s\n", filename);
				exit(1);
			}
			printf("waiting for requested file...\n");
			while(1)
			{	
				bzero(buf,sizeof(buf));
				if((nbytes = recvfrom(sockfd, buf, MAXBUFSIZE-1, 0, 
				(struct sockaddr *)&from_addr, &addr_len)) == -1)
				{
					perror("recvfrom");
					exit(1);
				}

				if((val=strncmp(buf, "EOF", 3)) == 0)
				{
					printf("file recieved \n");
					break;
				}
				buflen = strlen(buf);
				
				bytes_written=write(fp, buf, buflen);
				/*if(bytes_written<0);
				{					
					printf("File write Error: %s\n", strerror(errno));
					exit(1);
				}*/
							
			}
			
		
		}
		else if((val = strncmp(string1, "put", 3)) == 0)
		{
			memmove(filename, string1+3, strlen (string1));
			printf("filname = %s\n", filename);
			of = open(filename,  O_RDONLY);
			if (of == -1) {
				perror("Cannot open output file\n");
				exit(1);
				
			}
			printf("sending file to server...\n");
			while (1)
			{
				if((bytes_read = read(of, msg, msglen))==-1)
				{
					perror("File Read Error");
					exit(1);
				}
				if ((nbytes = sendto(sockfd, msg, strlen(msg), 0, p->ai_addr, p->ai_addrlen)) == -1)
				{
					perror("talker: sendto");
					exit(1);
				}
				
				if(bytes_read == 0)
				{
					strcpy(msg, "EOF");					
					if ((nbytes = sendto(sockfd, msg, strlen(msg), 0, p->ai_addr, p->ai_addrlen)) == -1)
					{
						perror("talker: sendto");
						exit(1);
					}
								printf("%s\n", msg);
					break;
				}
			}
		}
		else if((val = strncmp(string1, "ls", 2)) == 0)
		{
			bzero(buf,sizeof(buf));
			if((nbytes = recvfrom(sockfd, buf, MAXBUFSIZE-1, 0, 
				(struct sockaddr *)&from_addr, &addr_len)) == -1)
				{
					perror("recvfrom");
					exit(1);
				}


			printf("Server says: %s\n", buf);
		}
		else if((val = strncmp(string1, "exit", 4)) == 0)
		{	
			bzero(buf,sizeof(buf));
			
			if((nbytes = recvfrom(sockfd, buf, MAXBUFSIZE-1, 0, 
			  (struct sockaddr *)&from_addr, &addr_len)) == -1)
			{
				perror("recvfrom");
				exit(1);
			}

			printf("Acknowledge: %s\n", buf);
			bzero(string1, sizeof string1);
			
			strcpy(string1, buf);
			if((val=strncmp(string1, "exit", 4))==0)
			{
				printf("closing client \n");
				close(sockfd);
				return 0;
			}
		}
	}
	close(sockfd);

}

