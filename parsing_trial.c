
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


#define BUF_SIZE 1024 // Max size of allowed buffer

typedef struct conf {
	char username[BUF_SIZE];
	char password[BUF_SIZE];
} conf_t;

int numUsers(void* fp) {
	FILE* cfile = fp;
	char buf[BUF_SIZE];

	int line = 0;

	while(fgets(buf, sizeof(buf), cfile) != NULL) {
		/*one user per line*/
		line++;
	}

	line--; /* number of user without extra carriage return*/
	return line;
}

conf_t parseConfig(void* fp, int usernum) {
	FILE* cfile = fp;
	char buf[BUF_SIZE];
	conf_t config;
	
	int line = 0;

	while (fgets(buf, sizeof(buf), cfile) != NULL) {
		// Parse the input by line
		if (buf[0] != '#'){
			 
			if (line == usernum) {
				sscanf(buf, "%s %s", config.username, config.password);
				printf("%s -  %s\n", config.username, config.password);
			}
		}
		line++;
	}	

	return config;
}

int main(int argc, char *argv[]) {
	FILE* fp = NULL;
	char folder[BUF_SIZE];
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
    fp = fopen("dfs.conf", "rb");
    if (!fp) {
        fprintf(stderr, "Please create a config file\n");
		return EXIT_FAILURE;
    }

    /* Fill the Config Array */
    num_users = numUsers(fp);
    printf("%d\n", num_users);
    conf_t config[num_users];
    char shapes;
    

    for (i = 0; i < num_users; i++) {
    	fseek(fp, 0, 0); // Go back to the beginning of the file for reading
		config[i] = parseConfig(fp, i); // Parse the config
	}
}	
