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
#include <time.h>  
#include <stdint.h>

#define MAXBUFSIZE 100


int main (int argc, char * argv[])
{
	
	
	int nbytes;                             
	char buf[MAXBUFSIZE];
	size_t buflen;
	
	int sockfd;                             //this will be our socket
	struct addrinfo hints, *servinfo, *p;
	int rv, val, of, bytes_read;
	char string1[30];
	int fp;
	char command[100];
	char filename[30];
	struct sockaddr_in from_addr;
	socklen_t addr_len = sizeof(struct sockaddr);
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
	
	bzero(command, sizeof command);
	printf("Please type one of the following commands: \n1. send a time stamped message\n");
	//fgets (command, 100, stdin);
	
	
	time_t rawtime;
    struct tm * send_time;

    time (&rawtime);
    send_time = localtime (&rawtime);
    printf ("send time and date: %s", asctime(send_time));
    strftime(buf, sizeof buf, "%F %T", send_time);
    
    
	val = 0;
	strcpy(string1, command);
	if ((nbytes = sendto(sockfd, buf, strlen(buf), 0, p->ai_addr, p->ai_addrlen)) == -1)
	{
		perror("talker: sendto");
		exit(1);
	}
	printf("sent %d bytes\n", nbytes);
	
	bzero(buf,sizeof(buf));
	if((nbytes = recvfrom(sockfd, buf, MAXBUFSIZE-1, 0, 
						 (struct sockaddr *)&from_addr, &addr_len)) == -1)
	{
		perror("recvfrom");
		exit(1);
	}

    printf("listner: packet is %d bytes long\n", nbytes);
	printf("Server says: %s\n", ascitime(buf));

    struct tm tm;

	if (nbytes > 0)
	{   
		printf("got %d	bytes\n", nbytes);
		strptime(buf, "%F %T", &tm);
        time_t t = mktime(&tm);
		struct tm * recieve_time;
		recieve_time = localtime (&rawtime);
		printf("I got this time %s", gmtime(tm));
		printf ("recieve time and date: %s", asctime(send_time));
	}	
		// Blocks till bytes are received
		/*
		if ((val = strncmp(string1, "get", 3)) == 0)
		{
			memmove(filename, string1+3, strlen (string1));
			printf("string = %s\n", filename);
			
			fp = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			if (fp == -1) {
				fprintf(stderr, "Can't open input file %s\n", filename);
				exit(1);
			}
			printf("waiting for requested file...\n");
			while(1)
			{	
				bzero(msg,sizeof(msg));
				if((nbytes = recvfrom(sockfd, msg, sizeof(msg)-1, 0, 
				(struct sockaddr *)&from_addr, &addr_len)) == -1)
				{
					perror("recvfrom");
					exit(1);
				}
				
				if ((nbytes = sendto(sockfd, &nbytes, strlen(command), 0, p->ai_addr, p->ai_addrlen)) == -1)
				{
					perror("talker: sendto");
					exit(1);
				}
				if((val=strncmp(msg, "EOF", 3)) == 0)
				{
					printf("file recieved \n");
					break;
				}
				buflen = strlen(msg);
				
				write(fp, msg, buflen);
							
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
		*/
	
	close(sockfd);
	return 0;

}

