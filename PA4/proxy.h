#ifndef PROXY_H
#define PROXY_H

#include <cstdlib>
#include <iostream>
#include <string>
#include <string.h>
#include <sstream>
#include <vector>
#include <iterator>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <netdb.h>

#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define BUFFER_SIZE 8192

int checkError(std::vector<std::string> request); // Ensures the HTTP 1.0 request is error free
void sendErr(int err, int socket, std::vector<std::string> request); // If there is an error write it to the socket
void proxyConnect(int sock, std::string uri); // Use the proxy

#endif
