/*********************************************************************
 * Andrew Candelaresi
 * CSCI Fall 2016 5273
 * assistance from Taylor J Andrews, opengroup, stack over flow
 * *******************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <openssl/md5.h>

#define BUFFER_SIZE 1024 // Max size of allowed buffer

typedef struct conf {
	char username[BUFFER_SIZE];
	char password[BUFFER_SIZE];
} conf_t;

int numUsers(void* fp) {
	FILE* cfile = fp;
	char buf[BUFFER_SIZE];

	int line = 0;

	while(fgets(buf, sizeof(buf), cfile) != NULL) {
		// get the number of users from the file
		line++;
	}

	line--; // newlines are not users
	return line;
}

conf_t parseConfig(void* fp, int usernum) {
	FILE* cfile = fp;
	char buf[BUFFER_SIZE];
	conf_t config;
	
	int line = 0;

	while (fgets(buf, sizeof(buf), cfile) != NULL) {
		if (buf[0] != '#'){
			if (line == usernum) {
				sscanf(buf, "%s %s", config.username, config.password);
			}
		}
		line++;
	}	

	return config;
}

void handleget(int sock, char* folder, char* username, char* filename) {
 	char userdir[BUFFER_SIZE], fpath[BUFFER_SIZE];
 	FILE *fp;
 	int i;
 	int validfile = 0;
 	char ch;

 	strncpy(userdir, ".", sizeof("."));
 	strcat(userdir, folder);
 	strncat(userdir, "/", sizeof("/"));
 	strcat(userdir, username);
 	strncat(userdir, "/", sizeof("/"));

 	strncpy(fpath, userdir, sizeof(userdir));

 	for (i = 0; i < 4; i++) {
 		char hiddenpath[BUFFER_SIZE];
 		char num[2];
 		strcpy(hiddenpath, fpath);
 		strcat(hiddenpath, ".");
 		sprintf(num, "%d", i+1);
 		strcat(hiddenpath, num);
 		strcat(hiddenpath, ".");
 		strcat(hiddenpath, filename);

	 	fp = fopen(hiddenpath, "rb");
	 	if (fp) {
			while((ch = fgetc(fp)) != EOF) {
				write(sock, &ch, 1);
		    }

		    //WRITE THE FILE
			char delim = '\x3';
		    write(sock, &delim, 1);

		    if (fclose(fp)) {
		        fprintf(stderr, "Error closing file\n");
		    }
		   

		    validfile = 1;
	    }

	    memset(&hiddenpath[0], 0, sizeof(hiddenpath));
 	}

 	if(!validfile) {
 		fclose(fp);
		// if the file has failed send !!!
		write(sock, "Invalid file", strlen("Invalid file"));
		char delim = '\x3';
    	write(sock, &delim, 1);
		write(sock, "name \n", strlen("name \n"));
    	write(sock, &delim, 1);

		return;
 	}
}

void handlelist(int sock, char* folder, char* username) {
	DIR *dp;
 	struct dirent *ep;
 	FILE* fp;
 	char userdir[BUFFER_SIZE], fname[BUFFER_SIZE];

 	strncpy(userdir, ".", sizeof("."));
 	strcat(userdir, folder);
 	strncat(userdir, "/", sizeof("/"));
 	strcat(userdir, username);
 	strncat(userdir, "/", sizeof("/"));

  	dp = opendir(userdir);
  	if (dp != NULL)
    {
      	while ((ep = readdir(dp))) {
      		strcpy(fname, userdir);
      		strcat(fname, ep->d_name);
      		fp = fopen(fname, "rb");
      		fseek(fp, 0, SEEK_END);
      		if (ftell(fp) != 0) {
	        	write(sock, ep->d_name, strlen(ep->d_name));
	    		write(sock, "\n", strlen("\n"));
    		}
    		fclose(fp);
        }
      	closedir(dp);
    }

	char delim = '\x3';
    write(sock, &delim, 1);
}

void handleput(int sock,  char* folder, char* username, char* filename, int p1, int p2) {
 	char userdir[BUFFER_SIZE], fpath[BUFFER_SIZE];
	char hiddenpath1[BUFFER_SIZE], hiddenpath2[BUFFER_SIZE];
	char num1[2], num2[2];
	FILE *f1, *f2;
 	char r;
 	int rc;
 	//printf("%s", filename);

 	strncpy(userdir, ".", sizeof("."));
 	strcat(userdir, folder);
 	strncat(userdir, "/", sizeof("/"));
 	strcat(userdir, username);
 	strncat(userdir, "/", sizeof("/"));

 	strncpy(fpath, userdir, sizeof(userdir));
 	
	strcpy(hiddenpath1, fpath);
	strcat(hiddenpath1, ".");
	sprintf(num1, "%d", p1);
	strcat(hiddenpath1, num1);
	strcat(hiddenpath1, ".");
	strcat(hiddenpath1, filename);

	strcpy(hiddenpath2, fpath);
	strcat(hiddenpath2, ".");
	sprintf(num2, "%d", p2);
	strcat(hiddenpath2, num2);
	strcat(hiddenpath2, ".");
	strcat(hiddenpath2, filename);

	f1 = fopen(hiddenpath1, "wb");
    if (!f1) {
        fprintf(stderr, "Error opening file\n");
		return;
	}
    f2 = fopen(hiddenpath2, "wb");
    if (!f2) {
        fprintf(stderr, "Error opening file\n");
		return;
    }

 	while((rc = read(sock, &r, sizeof(r))) > 0) {
		if (r == '\x3')
			break;
		fputc(r, f1);
	}

	while((rc = read(sock, &r, sizeof(r))) > 0) {
		if (r == '\x3')
			break;
		fputc(r, f2);
	}

	if (fclose(f1)) {
	    fprintf(stderr, "Error closing file\n");
	}
	if (fclose(f2)) {
	    fprintf(stderr, "Error closing file\n");
	}
}

int main(int argc, char *argv[]) {
	FILE* fp = NULL;
	char folder[BUFFER_SIZE];
	struct sockaddr_in addr, caddr;
	socklen_t len;
	int ssock, csock; //socket for the server and client
	int num_users; // Number of users
	int i; // Iterator
	int port; // Port number of DFS server

	/* User Sanitation */
	if (argc < 3) {
		printf("Please specify a location and port\n");
		return EXIT_FAILURE;
	}

    /* Read in Args */
	strcpy(folder, argv[1]);
	port = atoi(argv[2]);

	/* Open Config File */
    fp = fopen("./dfs.conf", "rb");
    if (!fp) {
        fprintf(stderr, "Please create a config file\n");
		return EXIT_FAILURE;
    }

    /* Fill the Config Array */
    num_users = numUsers(fp);
    conf_t config[num_users];

    for (i = 0; i < num_users; i++) {
    	fseek(fp, 0, 0); // Go back to the beginning of the file for reading
		config[i] = parseConfig(fp, i); // Parse the config
	}

	/* Close Config File */
    if (fclose(fp)) {
        fprintf(stderr, "Error closing file\n");
    }

    /* Create Server Directory */
    char path[BUFFER_SIZE];
    strncpy(path, ".", sizeof("."));
    strncat(path, folder, sizeof(folder));

   	mkdir(path, S_IRWXU | S_IRWXG | S_IROTH); // Make directory for the server with rwxrwxr-- permissions

   	/* Create Socket */
	if((ssock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Error creating socket\n");
		exit(EXIT_FAILURE);
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	/* Bind Socket */	
	if (bind(ssock, (struct sockaddr *)&addr, sizeof(addr)) < 0){
		fprintf(stderr, "Error binding socket\n");
		close(ssock);
		exit(EXIT_FAILURE);
	}

	/* Tell Socket to Listen */
	if (listen(ssock, 10) < 0){
		fprintf(stderr, "Error listening\n");
		close(ssock);
		exit(EXIT_FAILURE);
	}

	while(1) {
		/* Initialize Client */
		len = sizeof(caddr);
		csock = accept(ssock, (struct sockaddr *)&caddr, &len);
		
		if(csock < 0) {
			close(csock);
			return EXIT_FAILURE;
		}
		
		printf("server port %d\n", port);
		// Use forking to handle multiple requests
		if (!fork()) { // We are in the child
			//write(csock, "Connected", strlen("Connected"));
            close(ssock);

            char buf[BUFSIZ];
            char field1[BUFFER_SIZE], field2[BUFFER_SIZE], p1[BUFFER_SIZE], p2[BUFFER_SIZE];

            fd_set s;
            FD_ZERO(&s);
            FD_SET(csock, &s);

            int authenticated = 0, made_dir = 0, user;
            int len;

            while((len=recv(csock, &buf, BUFSIZ, 0)) > 0) {
	            /* Process Server Requests */
	            memset(&field1[0], 0, sizeof(field1));
	            memset(&field2[0], 0, sizeof(field2));
	            memset(&p1[0], 0, sizeof(p1));
	            memset(&p2[0], 0, sizeof(p2));

            	sscanf(buf, "%s %s %s %s %*s", field1, field2, p1, p2);

	            if (!authenticated) {
	            	// Give the user a chance to authenticate
	            	for (i=0; i < num_users; i++) {
	            		if((strcmp(config[i].username,field1) == 0) && (strcmp(config[i].password, field2) == 0)) {
	            			authenticated = 1; // Authenticated is now the value of our user in the array
	            			user = i;
	            			break;
	            		}
	            	}	
	            	if (!authenticated) {
		            	// If they failed, tell them
		            	char response[] = "Invalid Username/Password. Please try again.\n";
		            	//send(csock, "1", strlen("1"), 0);
		            	write(csock, response, strlen(response));
		            	char delim = '\x3';
    					write(csock, &delim, 1);
	            	}
	            	else {
	            		// If they succeeded, tell them
		            	char welcome[] = "Authentication successful\n";
		            	send(csock, welcome, strlen(welcome), 0);
		            	char delim = '\x3';
    					write(csock, &delim, 1);

    	        		/* Create User Directory */
    	        		if (!made_dir) {
						    strncat(path, "/", sizeof("/"));
						    strncat(path, config[user].username, sizeof(config[user].username));

						   	mkdir(path, S_IRWXU | S_IRWXG | S_IROTH); // Make directory for the server with rwxrwxr-- permissions
						   	made_dir = 1;
					    }
	            	}
	            }
	            else {
				    if ((strcmp(field1, "PUT") == 0)) {
		            	handleput(csock, folder, config[user].username, field2, atoi(p1), atoi(p2));
						
						write(csock, "Waiting for input\n", strlen("Waiting for input\n"));
						char delim = '\x3';
					    write(csock, &delim, 1);
		            }
		            if ((strcmp(field1, "GET") == 0)) {
		            	handleget(csock, folder, config[user].username, field2);

		            	write(csock, "Waiting for input\n", strlen("Waiting for input\n"));
						char delim = '\x3';
					    write(csock, &delim, 1);
		            }
		            if ((strcmp(field1, "LIST") == 0)) {
						handlelist(csock, folder, config[user].username);
						
						write(csock, "Waiting for input", strlen("Waiting for input"));
						char delim = '\x3';
					    write(csock, &delim, 1);
		            }
		            authenticated = 0; // Force the user to reauthenticate for a new command
	            }
            	memset(&buf[0], 0, sizeof(buf)); // Reset the buffer for next input
        	}	
        }
        close(csock);
	}
	close(ssock);

    return 0;
}
