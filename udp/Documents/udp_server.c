// created by Andrew Candelaresi
// help from Beej's guide to Network Programming Author Brian Hall
// help from https://stackoverflow.com/questions/16185919/redirecting-output-of-server-to-client-socket

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>



#define MAXBUFSIZE 100

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main (int argc, char * argv[] )
{
	int sockfd;                           //This will be our socket
	int child_pid;                        //child pid when fork 
	int nbytes, nbytes2;                          // number of bytes sent
	int bytes_read;     
	int size;                              //used for error checking packet loss                  
	struct addrinfo hints, *servinfo, *p; //"Internet socket address structure"
	int rv, val, i, sd, inf, of, pf;
	off_t offset = 0;
	                          //number of bytes we receive in our message
	struct sockaddr_in their_addr;   //a struct to store the senders address info
	socklen_t addrlen = sizeof(their_addr);            /* length of addresses */
	char buf[MAXBUFSIZE];              //a buffer to store our received message
	socklen_t addr_len;
	char s[INET6_ADDRSTRLEN];
	char ipstr[INET6_ADDRSTRLEN];
	char msg[10240];
	int msglen = sizeof(msg);
	char filename[30];
	struct stat stat_buf;
	char c;                             //used to read in chars from file
	size_t buflen;
	ssize_t bytes_written;
	
		
	if (argc != 2)
	{
		printf ("USAGE:  <port>\n");
		exit(1);
	}

	/******************
	  This code populates the sockaddr_in struct with
	  the information about our socket
	 ******************/	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;  // use IPv4
	hints.ai_socktype = SOCK_DGRAM; //use a unconnected DataGram Socket
	hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
	
	if ((rv=getaddrinfo(NULL, argv[1], &hints, &servinfo))!=0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	
	//Causes the system to create a generic socket of type UDP (datagram)
	for (p = servinfo; p!=NULL; p=p->ai_next){
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
			perror("listner: socket");
			continue;
		}
		/******************
		Once we've created a socket, we must bind that socket to the 
		local address and port we've supplied in the sockaddr_in struct
		******************/
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockfd);
			perror("listener: bind");
			continue;
		}
		
		break;
	}

	if (p == NULL)
	{
		fprintf(stderr, "listner: failed to bind socket\n");
		return 2;
	}
	
	freeaddrinfo(servinfo);
	//run this loop until the client enters an exit command
	while(1)
	{
		printf("listner: waiting to recvfrom \n");


		//waits for an incoming message
		addr_len = sizeof their_addr;

		bzero(buf,sizeof(buf));
		bzero(msg, sizeof(msg));
		
		if((nbytes = recvfrom(sockfd, buf, MAXBUFSIZE-1, 0, 
		  (struct sockaddr *)&their_addr, &addr_len)) == -1)
		{
			perror("recvfrom");
			exit(1);
		}

		printf("listner: got packet from %s\n", inet_ntop(their_addr.sin_family, 
			  get_in_addr((struct sockaddr*)&their_addr), s, sizeof s));
		printf("listner: packet is %d bytes long\n", nbytes);
		printf("The client says %s\n", buf);

		FILE *fp;
		
		if(buf[strlen(buf)-1] =='\n')
			buf[strlen(buf)-1] = 0;
		if(buf[strlen(buf)-1] =='\r')
			buf[strlen(buf)-1] = 0;
		
		int status=0; 
		
		i = 0;
		val = 0;
		char string1[30];
		strcpy(string1, buf);
		if((val=strncmp(string1, "exit", 4))==0)
			{
				printf("closing Socket and exiting server \n");
				strcpy(msg, "exit");
				if ((nbytes = sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&their_addr, addrlen)) == -1)
					{
						perror("listner: sendto");
						exit(1);
					}
					printf("sent %s\n", msg); 
				close(sockfd);
				return 0;
			}
		
		if ((val=strncmp(string1, "ls", 2)) == 0)
		{	
			if((child_pid = fork()) < 0 )
			{
				perror("fork failure");
				exit(1);
			}
			if(child_pid==0)
			{	
			strcat(buf,">file");  // add to redirected file	
			execlp("sh","sh","-c",buf,(char *)0); // executed the ls command and redirected to  file 
			}
			else
			{
				wait(&status);
				fp=fopen("file","r"); //open the file read the content and stored to buffer
				while((c=getc(fp))!=EOF)
					msg[i++]=c;
					msg[i]='\0';
				fclose(fp); 
				if ((nbytes = sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&their_addr, addrlen)) == -1)
				{
					perror("listner: sendto");
					exit(1);
				}
				printf("sent %d bytes\n", nbytes);  
			 }	
		}
		//code for returning the requested file to client
		else if((val=strncmp(string1, "get", 3)) == 0)
		{
			memmove(filename, buf+3, strlen (buf));
			printf("string = %s\n", filename);

			of = open(filename,  O_RDONLY);
			if (of == -1) {
				strcpy(msg, "Can't open input file \n");
				if((nbytes = sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&their_addr, addrlen)) == -1)
				{
						perror("listner: sendto");
						exit(1);			
				}
			}
			printf("sending file to client...\n");
			while (1)
			{
				if((bytes_read = read(of, msg, msglen))==-1)
				{
					perror("File Read Error");
					exit(1);
				}
				printf("%d\n", bytes_read);
				if((nbytes2 = sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&their_addr, addrlen)) == -1)
				{
						perror("listner: sendto");
						exit(1);			
				}
				
				if((nbytes = recvfrom(sockfd, (void *)&size, MAXBUFSIZE-1, 0, 
				(struct sockaddr *)&their_addr, &addr_len)) == -1)
				{
					perror("recvfrom");
					exit(1);
				}
				if (size!=nbytes2)
				{
					printf("%d : %d \n", size, nbytes2);
					if((nbytes2 = sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&their_addr, addrlen)) == -1)
					{
							perror("listner: sendto");
							exit(1);			
					}
				
				}	
				if(bytes_read == 0)
				{
					strcpy(msg, "EOF");

					if((nbytes = sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)&their_addr, addrlen)) == -1)
					{
						perror("listner: sendto");
						exit(1);			
					}
					printf("%s\n", msg);
					close(of);
					break;
				}
			}

		}
		else if((val=strncmp(string1, "put", 3)) == 0)
		{
			memmove(filename, buf+3, strlen (buf));
			printf("filename = %s\n", filename);
			
			pf = open(filename, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			if (pf == -1) {
				fprintf(stderr, "Can't open input file %s\n", filename);
				exit(1);
			}
			printf("receiving file...\n");
			while(1)
			{	
				bzero(buf,sizeof(buf));
				if((nbytes = recvfrom(sockfd, buf, MAXBUFSIZE-1, 0, 
				(struct sockaddr *)&their_addr, &addr_len)) == -1)
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
				
				bytes_written=write(pf, buf, buflen);
				/*if(bytes_written<0);
				{					
					printf("File write Error: %s\n", strerror(errno));
					exit(1);
				}*/
							
			}
		}
			

		/*
		if (!strncmp(str, "ls", 2)) {
		sprintf(sysstr, "%s > /tmp/server.out", str);
		system(sysstr);
		} 
		*/
		
		
		
		
	}
	close(sockfd);
	
	return 0;
}

