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
#define	LINELEN 128 // Length of line

typedef struct conf {
	const char* names[4];

	char username[BUFFER_SIZE];
	char password[BUFFER_SIZE];

	int ports[4];
} config_t;

typedef struct pieces {
	int p1;
	int p2;
} pieces_t;

typedef struct node {
	char name[BUFFER_SIZE];
	int hash;
	struct node* next;
} node_t;

config_t parseConfig(void* fp) {
	FILE* cfile = fp;
	char buf[BUFFER_SIZE];
	char temp[6];	
	char DFS1name[5];
	char DFS2name[5];
	char DFS3name[5];
	char DFS4name[5];
	config_t info;
	
	int line = 0;

	while (fgets(buf, sizeof(buf), cfile) != NULL) {
		// Parse the input by line
		if (buf[0] != '#'){
			switch (line){
				case 0:
					strncpy(DFS1name, buf + 7, 4);
					strncpy(temp, buf + 22, 5);
					info.ports[0] = atoi(temp);
					break;
				case 1:
					strncpy(DFS2name, buf + 7, 4);
					strncpy(temp, buf + 22, 5);
					info.ports[1] = atoi(temp);
					break;
				case 2:
					strncpy(DFS3name, buf + 7, 4);
					strncpy(temp, buf + 22, 5);
					info.ports[2] = atoi(temp);
					break;
				case 3:
					strncat(DFS4name, buf + 7, 4);
					strncpy(temp, buf + 22, 5);
					info.ports[3] = atoi(temp);
					break;
				case 4:
					memset(&info.username[0], 0, sizeof(info.username)); // Reset the buffer for next input
					strncat(info.username, buf + 10, strlen(buf)-11);
					break;
				case 5:
					memset(&info.password[0], 0, sizeof(info.password)); // Reset the buffer for next input
					strncat(info.password, buf + 10, strlen(buf)-11);
					break;
				default:
					break;
			}
			line++;
		}
	}

	info.names[0] = DFS1name;
	info.names[1] = DFS2name;
	info.names[2] = DFS3name;
	info.names[3] = DFS4name;

	return info;
}

void putHash(int hash, char* filename, node_t* headptr) {
	node_t* cur = headptr;
	if(headptr->hash == -1) {
		cur->hash = hash;
		strncpy(cur->name, filename, strlen(filename));
		cur->next = NULL;
	}
	else {
		while(cur->next != NULL) {
			cur = cur->next;
		}
		cur->next = malloc(sizeof(node_t));
		cur = cur->next;
		cur->hash = hash;
		strncpy(cur->name, filename, strlen(filename));
		cur->next = NULL;
	}
}

int retrieveHash(char* filename, node_t* headptr) {
	node_t* cur = headptr;
	while(cur != NULL) {
		if(strncmp(filename, cur->name, 3) == 0) {
			return cur->hash;
		}
		cur = cur->next;
	}
	return -100;
}

int MD5Hash(void *fp) {
	FILE* put_file = fp; // Cast the void to a FILE
	unsigned char hash[MD5_DIGEST_LENGTH], buf[BUFFER_SIZE]; // Store the final hash and the intermediate buffer
    MD5_CTX mdContext; // Use the MD5 library
    int bytes, i;
  	unsigned long mod_hash = 0, mod = 4;

    MD5_Init(&mdContext);

    while ((bytes = fread (buf, 1, 1024, put_file)) != 0) {
        MD5_Update(&mdContext, buf, bytes);
    }

    MD5_Final(hash,&mdContext);

    for(i = 0; i < MD5_DIGEST_LENGTH + 1; i++) {
    	int cur_val = (hash[i] > '9')?(hash[i] &~0x20)-'A'+10:(hash[i] - '0'); // Convert value to an int
    	mod_hash = mod_hash*16 + cur_val;

    	if (mod_hash >= mod) {
    		mod_hash %= mod;
    	}
    }

    return (int)mod_hash;
}

void Authenticate(int* sock, config_t info) {
	int i; // Iterator
	int rc;
	char auth[BUFFER_SIZE];
	char buf;
	strncpy(auth, info.username, sizeof(info.username));
	strncat(auth, " ", sizeof(" "));
	strncat(auth, info.password, sizeof(info.password));

	for (i = 0; i < 4; i++) {
		if (sock[i] != -1) {
			while((rc = read(sock[i], &buf, sizeof(buf))) > 0) {
				if (buf == '\x3')
					break;
			}
			if(rc > 0) {
				write(sock[i], auth, strlen(auth));
			}
			else {
				sock[i] = -1;
			}
		}
	}
}

void list(int* sock, config_t info) {
	int i, rc;
	int invalid = 0;
	char buf;
	int resCount = 0;
	char r;
	char response[BUFFER_SIZE];
	char incomp[BUFFER_SIZE];
	
	Authenticate(sock, info); // Send authorization to servers
	for (i = 0; i < 4; i++) {
		while((rc = read(sock[i], &buf, sizeof(buf))) > 0) {
			if (buf == 'I')
				invalid = 1;
			if (buf == '\x3')
				break;
		}
		if (invalid) {
			write(sock[i], "LIST", strlen("LIST"));
			printf("Invalid Username/Password. Please try again.\n");
			return;
		}
		if(rc > 0) {
			write(sock[i], "LIST", strlen("LIST"));

			while((rc = read(sock[i], &r, sizeof(r))) > 0) {
				if (r == '\n') 
					resCount++;
				if (r == '\x3')
					break;
				strncat(response, &r, 1);
			}
		}
		else {
			sock[i] = -1;
		}
	}

	int offsock = 0;
	for(i = 0; i < 4; i++) {
		if (sock[i] == -1)
			offsock++;
	}
	
	int missing_count = 0;
	if(((sock[0] == -1) && (sock[1] == -1)) || ((sock[1] == -1) && (sock[2] == -1))
		 || ((sock[2] == -1) && (sock[3] == -1)) || ((sock[3] == -1) && (sock[0] == -1))) {
		missing_count = 2;
	}

	if (missing_count > 1) {
		strcpy(incomp, "[incomplete]");
	}
	else {
		strcpy(incomp, "");
	}

	char strip_leading_dot[BUFFER_SIZE];
	i = 1;
	int j = 0;
	while(response[i] != '\0') {
		if(!((response[i-1] == '\n') && (response[i] == '.'))) {
			strip_leading_dot[j] = response[i];
			j++;
		} 
		i++;
	}
	char strip_num[BUFFER_SIZE];
	i = 1;
	j = 0;
	while(strip_leading_dot[i+1] != '\0') {
		if(!((strip_leading_dot[i-1] == '\n') && (strip_leading_dot[i+1] == '.'))) {
			strip_num[j] = strip_leading_dot[i];
			j++;
		} 
		i++;
	}
	char strip_second_dot[BUFFER_SIZE];
	i = 1;
	j = 0;
	while(strip_num[i] != '\0') {
		if(!((strip_num[i-1] == '\n') && (strip_num[i] == '.'))) {
			strip_second_dot[j] = strip_num[i];
			j++;
		} 
		i++;
	}
	char strip_extra_newlines[BUFFER_SIZE];
	i = 1;
	j = 0;
	while(strip_second_dot[i] != '\0') {
		if(!((strip_second_dot[i-1] == '\n') && (strip_second_dot[i] == '\n'))) {
			strip_extra_newlines[j] = strip_second_dot[i];
			j++;
		} 
		i++;
	}
	const char* current = strip_extra_newlines;
	int line_num = 1;
	int okay_print = 0;
	int count = 0;
	resCount /= (4-offsock); // Divide by the number of working sockets
	resCount /= 2; // Divide by 2 for 2 res from each working socket
	resCount -= 1; // Subtract 1 for returning ..
	okay_print = resCount * 2; // Number of lines we care about
    while(current)
    {
    	if(count == okay_print) 
    		break;
        char* next = strchr(current, '\n');
        if (next != NULL) {
        	*next = '\0';
        }
        if(strcmp(current, "") != 0 && (current[0] != '/')) {
        	if(count%2 == 0) {
        		printf("%d. %s %s\n", line_num, current, incomp);
        		line_num++;
        	}
        }
        if (next != NULL) {
        	current = next + 1;
        }  
        else {
        	current = NULL;
        }
        count ++;
    }

    memset(&strip_leading_dot[0], 0, sizeof(strip_leading_dot));
    memset(&strip_num[0], 0, sizeof(strip_num));
    memset(&strip_second_dot[0], 0, sizeof(strip_second_dot));
    memset(&strip_extra_newlines[0], 0, sizeof(strip_extra_newlines));
    //memset(&current[0], 0, sizeof(current));
}

void get(int* sock, char* filename, config_t info, node_t* headptr) {
	int i, rc, hash;
	int invalid = 0;
	char buffer;
	char r;
	FILE** f1 = malloc(sizeof(FILE*) * 4);
	FILE** f2 = malloc(sizeof(FILE*) * 4);
	FILE* fp = fopen(filename, "wb");

	for (i = 0; i < 4; i++) {
		f1[i] = tmpfile();
		f2[i] = tmpfile();
	}
	
	Authenticate(sock, info); // Send authorization to servers
	for (i = 0; i < 4; i++) {
		while((rc = read(sock[i], &buffer, sizeof(buffer))) > 0) {
			if (buffer == 'I')
				invalid = 1;
			if (buffer == '\x3')
				break;
		}
		if (invalid) {
			write(sock[i], "GET ", strlen("GET "));
			printf("Invalid Username/Password. Please try again.\n");
			return;
		}
		if(rc > 0) {
			char request[BUFFER_SIZE];
			strncpy(request, "GET ", sizeof("GET "));
			strcat(request, filename);
			write(sock[i], request, strlen(request));

			while((rc = read(sock[i], &r, sizeof(r))) > 0) {
				if (r == '\x3')
					break;
				fputc(r, f1[i]);
			}

			while((rc = read(sock[i], &r, sizeof(r))) > 0) {
				if (r == '\x3')
					break;
				fputc(r, f2[i]);
			}
		}
		else {
			fclose(f1[i]);
			f1[i] = NULL;
			fclose(f2[i]);
			f2[i] = NULL;
			sock[i] = -1;
		}
	}

	hash = retrieveHash(filename, headptr);

	for(i = 0; i < 4; i++) {
		if(f1[i] != NULL) {
			fseek(f1[i], 0, SEEK_SET);
			fseek(f2[i], 0, SEEK_SET);
		}
	}

	int missing_count = 0;
	if(((sock[0] == -1) && (sock[1] == -1)) || ((sock[1] == -1) && (sock[2] == -1))
		 || ((sock[2] == -1) && (sock[3] == -1)) || ((sock[3] == -1) && (sock[0] == -1))) {
		missing_count = 2;
	}

	if (missing_count > 1) {
		printf("File is incomplete.\n");
	}
	else {
		int sock_num; // Number of the socket to get the file from
		char c; // Reads the character

		switch (hash) {
			case 0:
				/* File piece 1 */
				sock_num = 0;
				if (sock[0] == -1)
					sock_num = 3;
				if(sock_num == 0) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 3) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}

				/* File piece 2 */
				sock_num = 0;
				if (sock[0] == -1) {
					sock_num = 1;
				}
				if(sock_num == 0) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 1) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}

				/* File piece 3 */
				sock_num = 1;
				if (sock[1] == -1) {
					sock_num = 2;
				}
				if(sock_num == 1) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 2) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}

				/* File piece 4 */
				sock_num = 2;
				if (sock[0] == -1) {
					sock_num = 3;
				}
				if(sock_num == 2) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 3) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				break;
			case 1:
				/* File piece 1 */
				sock_num = 0;
				if (sock[0] == -1)
					sock_num = 1;
				if(sock_num == 0) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 1) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}

				/* File piece 2 */
				sock_num = 1;
				if (sock[0] == -1) {
					sock_num = 2;
				}
				if(sock_num == 1) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 2) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}

				/* File piece 3 */
				sock_num = 2;
				if (sock[1] == -1) {
					sock_num = 3;
				}
				if(sock_num == 2) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 3) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}

				/* File piece 4 */
				sock_num = 3;
				if (sock[0] == -1) {
					sock_num = 0;
				}
				if(sock_num == 3) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 0) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				break;
			case 2:
				/* File piece 1 */
				sock_num = 1;
				if (sock[0] == -1)
					sock_num = 2;
				if(sock_num == 1) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 2) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}

				/* File piece 2 */
				sock_num = 2;
				if (sock[0] == -1) {
					sock_num = 3;
				}
				if(sock_num == 2) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 3) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}

				/* File piece 3 */
				sock_num = 3;
				if (sock[1] == -1) {
					sock_num = 0;
				}
				if(sock_num == 3) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 0) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}

				/* File piece 4 */
				sock_num = 0;
				if (sock[0] == -1) {
					sock_num = 1;
				}
				if(sock_num == 0) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 1) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				break;
			case 3:
				/* File piece 1 */
				sock_num = 2;
				if (sock[0] == -1)
					sock_num = 3;
				if(sock_num == 2) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 3) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}

				/* File piece 2 */
				sock_num = 3;
				if (sock[0] == -1) {
					sock_num = 0;
				}
				if(sock_num == 3) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 0) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}

				/* File piece 3 */
				sock_num = 0;
				if (sock[1] == -1) {
					sock_num = 1;
				}
				if(sock_num == 0) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 1) {
					while((c = fgetc(f1[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}

				/* File piece 4 */
				sock_num = 1;
				if (sock[0] == -1) {
					sock_num = 2;
				}
				if(sock_num == 1) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				else if(sock_num == 2) {
					while((c = fgetc(f2[sock_num])) != EOF) {
						fputc(c, fp);
					}
				}
				break;
			default:
				printf("File not found.\n");
				break;
		}
	}

	for (i = 0; i < 4; i++) {
		if (f1[i] != NULL) {
			if (fclose(f1[i])) {
		        fprintf(stderr, "Error closing file\n");
		    }
			if (fclose(f2[i])) {
		        fprintf(stderr, "Error closing file\n");
		    }
		}
	}

	free(f1);
	free(f2);

	if (fclose(fp)) {
        fprintf(stderr, "Error closing file\n");
    }

 	for (i = 0; i < 4; i++) {
	 	char userdir[BUFFER_SIZE], fpath[BUFFER_SIZE];
 		char hiddenpath[BUFFER_SIZE];
 		char num[2];
 		int j;

	 	strncpy(userdir, "./DFS", sizeof("./DFS"));
 		sprintf(num, "%d", i+1);
 		strcat(userdir, num);
	 	strncat(userdir, "/", sizeof("/"));
	 	strcat(userdir, info.username);
	 	strncat(userdir, "/", sizeof("/"));

	 	for(j = 0; j < 4; j++) {
	 		strncpy(fpath, userdir, sizeof(userdir));
	 		strcpy(hiddenpath, fpath);
	 		strcat(hiddenpath, ".");
	 		sprintf(num, "%d", j+1);
	 		strcat(hiddenpath, num);
	 		strcat(hiddenpath, ".");
	 		strcat(hiddenpath, filename);

	 		remove(hiddenpath);
	 		//unlink(hiddenpath);
	 	}
	}
}	

void put(int* sock, char* filename, config_t config, node_t* headptr) {
	FILE* fp = NULL, *f1 = NULL, *f2 = NULL, *f3 = NULL, *f4 = NULL;
	int i, rc;
	int invalid = 0;
	int hash;
	int size, f1size, f2size, f3size;//, f4size;
	pieces_t dfs[4]; // Denotes which piece we are putting in which file
	char buf;

    fp = fopen(filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error opening file\n");
		return;
    }

    hash = MD5Hash(fp);
    putHash(hash, filename, headptr);

   	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	f1size = size/4;
	f2size = size/4;
	f3size = size/4;
	//f4size = size/4 + size%4; // Give the extra bytes to the last file chunk

	/* Divide the file */
	f1 = tmpfile();
	f2 = tmpfile();
	f3 = tmpfile();
	f4 = tmpfile();

	char c;
	int k = 0;
	while((c = fgetc(fp)) != EOF) {
		if (k < f1size){
			fputc(c, f1);
		} else if (k < f1size+f2size) {
			fputc(c, f2);
		} else if (k < f1size+f2size+f3size) {
			fputc(c, f3);
		} else {
			fputc(c, f4);
		}
		k++;
	}

    /* Set up the hasing values */
    switch (hash) {
    	case 0:
    		dfs[0].p1 = 1; dfs[0].p2 = 2;
    		dfs[1].p1 = 2; dfs[1].p2 = 3;
    		dfs[2].p1 = 3; dfs[2].p2 = 4;
    		dfs[3].p1 = 4; dfs[3].p2 = 1;
    		break;
    	case 1:
    		dfs[0].p1 = 4; dfs[0].p2 = 1;
    		dfs[1].p1 = 1; dfs[1].p2 = 2;
    		dfs[2].p1 = 2; dfs[2].p2 = 3;
    		dfs[3].p1 = 3; dfs[3].p2 = 4;
    		break;
    	case 2:
    		dfs[0].p1 = 3; dfs[0].p2 = 4;
    		dfs[1].p1 = 4; dfs[1].p2 = 1;
    		dfs[2].p1 = 1; dfs[2].p2 = 2;
    		dfs[3].p1 = 2; dfs[3].p2 = 3;
    		break;
    	case 3:
    		dfs[0].p1 = 2; dfs[0].p2 = 3;
    		dfs[1].p1 = 3; dfs[1].p2 = 4;
    		dfs[2].p1 = 4; dfs[2].p2 = 1;
    		dfs[3].p1 = 1; dfs[3].p2 = 2;
    		break;
    	default:
    		printf("Oh no!");
    		return;
    }

	Authenticate(sock, config); // Send authorization to servers
	for (i = 0; i < 4; i++) {
		while((rc = read(sock[i], &buf, sizeof(buf))) > 0) {
			if (buf == 'I')
				invalid = 1;
			if (buf == '\x3')
				break;
		}
		if (invalid) {
			write(sock[i], "PUT", strlen("PUT"));
			printf("Invalid Username/Password. Please try again.\n");
			return;
		}
		if(rc > 0) {
			char request[BUFFER_SIZE], piece[BUFFER_SIZE], ch;
			strncpy(request, "PUT ", sizeof("PUT "));
			strcat(request, filename);
			strcat(request, " ");
			sprintf(piece, "%d %d", dfs[i].p1, dfs[i].p2);
			strcat(request, piece);
			write(sock[i], request, strlen(request));

			fseek(f1, 0, SEEK_SET);
			fseek(f2, 0, SEEK_SET);
			fseek(f3, 0, SEEK_SET);
			fseek(f4, 0, SEEK_SET);

			if(dfs[i].p1 == 1) {
				while((ch = fgetc(f1)) != EOF) 
					write(sock[i], &ch, 1);
				char delim = '\x3';
			    write(sock[i], &delim, 1);
			} else if (dfs[i].p1 == 2) {
				while((ch = fgetc(f2)) != EOF) 
					write(sock[i], &ch, 1);
				char delim = '\x3';
			    write(sock[i], &delim, 1);
			} else if (dfs[i].p1 == 3) {
				while((ch = fgetc(f3)) != EOF) 
					write(sock[i], &ch, 1);
				char delim = '\x3';
			    write(sock[i], &delim, 1);
			} else if (dfs[i].p1 == 4) {
				while((ch = fgetc(f4)) != EOF) 
					write(sock[i], &ch, 1);
				char delim = '\x3';
			    write(sock[i], &delim, 1);
			}

			fseek(f1, 0, SEEK_SET);
			fseek(f2, 0, SEEK_SET);
			fseek(f3, 0, SEEK_SET);
			fseek(f4, 0, SEEK_SET);

			if(dfs[i].p2 == 1) {
				while((ch = fgetc(f1)) != EOF) 
					write(sock[i], &ch, 1);
				char delim = '\x3';
			    write(sock[i], &delim, 1);
			} else if (dfs[i].p2 == 2) {
				while((ch = fgetc(f2)) != EOF) 
					write(sock[i], &ch, 1);
				char delim = '\x3';
			    write(sock[i], &delim, 1);
			} else if (dfs[i].p2 == 3) {
				while((ch = fgetc(f3)) != EOF) 
					write(sock[i], &ch, 1);
				char delim = '\x3';
			    write(sock[i], &delim, 1);
			} else if (dfs[i].p2 == 4) {
				while((ch = fgetc(f4)) != EOF) 
					write(sock[i], &ch, 1);
				char delim = '\x3';
			    write(sock[i], &delim, 1);
			}

			// while((rc = read(sock[i], &r, sizeof(r))) > 0) {
			// 	if (r == '\x3')
			// 		break;
			// 	printf("%c", r);
			// }
		}
		else {
			sock[i] = -1;
		}
	}

	if (fclose(fp)) {
        fprintf(stderr, "Error closing file\n");
    }
    if (fclose(f1)) {
        fprintf(stderr, "Error closing file\n");
    }
    if (fclose(f2)) {
        fprintf(stderr, "Error closing file\n");
    }
    if (fclose(f3)) {
        fprintf(stderr, "Error closing file\n");
    }
    if (fclose(f4)) {
        fprintf(stderr, "Error closing file\n");
    }
    unlink(filename);
}

int main(int argc, char *argv[]) {
	config_t info; // Struct to hold data from the config file
	FILE* fp = NULL;
	struct sockaddr_in addr;
	char buf[BUFFER_SIZE], command[BUFFER_SIZE], filename[BUFFER_SIZE];
	int sock[4];
	int i; // Iterator


	/* User Sanitation */
	if (argc < 2) {
		printf("Please specify a config file\n");
		return EXIT_FAILURE;
	}

	/* Open Config File */
    fp = fopen(argv[(argc-1)], "rb");
    if (!fp) {
        fprintf(stderr, "Error opening file\n");
		return EXIT_FAILURE;
    }

	info = parseConfig(fp); // Parse the config

	/* Close Config File */
    if (fclose(fp)) {
        fprintf(stderr, "Error closing file\n");
    }

    for (i = 0; i < 4; i++) {
		/* Create Socket */
		if((sock[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			fprintf(stderr, "Error creating socket\n");
			return EXIT_FAILURE;
		}

		addr.sin_family = AF_INET;
		addr.sin_port = htons(info.ports[i]);
		addr.sin_addr.s_addr = INADDR_ANY;

		/* Connect Socket */
		if (connect(sock[i], (struct sockaddr *)&addr, sizeof(addr)) < 0){
			close(sock[i]);
			sock[i] = -1;
		}
		write(sock[i], "Client Connected", strlen("Client Connected"));
    }

	node_t* headptr = malloc(sizeof(node_t));
	headptr->hash = -1;
	headptr->next = NULL;

    while (fgets(buf, sizeof(buf), stdin)) {
    	/* Open Config File */
	    fp = fopen(argv[(argc-1)], "rb");
	    if (!fp) {
	        fprintf(stderr, "Error opening file\n");
			return EXIT_FAILURE;
	    }

		info = parseConfig(fp); // Parse the config

		/* Close Config File */
	    if (fclose(fp)) {
	        fprintf(stderr, "Error closing file\n");
	    }

		if (strstr(buf, "GET") != NULL) {
            memset(&command[0], 0, sizeof(command));
            memset(&filename[0], 0, sizeof(filename));

        	sscanf(buf, "%s %s %*s", command, filename);

        	get(sock, filename, info, headptr);
		}
		if (strstr(buf, "PUT") != NULL) {
            memset(&command[0], 0, sizeof(command));
            memset(&filename[0], 0, sizeof(filename));

        	sscanf(buf, "%s %s %*s", command, filename);

        	put(sock, filename, info, headptr);
		}
		if (strstr(buf, "LIST") != NULL) {
			list(sock, info);
		}
		if (strstr(buf, "quit") != NULL) {
			break;
		}

	    memset(&buf[0], 0, sizeof(buf)); // Reset the buffer for next input
	    usleep(1000000);// Wait 1 sec to see if server timed out
	}

	for(i = 0; i < 4; i++) {
		if (sock[i] != -1) {
			close(sock[i]);
		}	
	}

	node_t* cur = headptr;
	node_t* tmp = cur;
	while(cur->next != NULL) {
		tmp = cur;
		cur = cur->next;
		free(tmp);
	}
	free(cur);
    return 0;
}
