/* Network Systems PA3
 * Created By: Taylor Andrews
 *
 * This is the file to create the webproxy. 
 * The proxy should parse the requested url, then send its own
 * edited request to the same url.
 * Finally, the proxy will return the requested HTTP 
 * to the client.
 */

#include "proxy.h"

using namespace std;

int main(int argc, char *argv[]) {
	int port; // Webproxy port number
	int server_socket, client_socket; // Sockets
	struct sockaddr_in addr, caddr; 
	socklen_t len;

	/* User Sanitation */
	if (argc != 2) {
		cout << "Usage: ./webproxy <port#>" << endl;
		exit(EXIT_FAILURE);
	}

	/* Read in Port */
	if (!(stringstream(argv[1]) >> port)) {
		cout << "Usage: ./webproxy <port#>" << endl;
		exit(EXIT_FAILURE);
	}

	/* Create the server socket */
	if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		//cout << "Error creating socket" << endl;
		exit(EXIT_FAILURE);
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	/* Bind the server socket */	
	if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0){
		//cout << "Error binding socket" << endl;
		close(server_socket);
		exit(EXIT_FAILURE);
	}

	/* Tell the server socket to listen */
	if (listen(server_socket, 10) < 0){
		//cout << "Error listening on socket" << endl;
		close(server_socket);
		exit(EXIT_FAILURE);
	}

	while (1) {
		/* Initialize the client */
		len = sizeof(caddr);
		client_socket = accept(server_socket, (struct sockaddr *) &caddr, &len);

		if(client_socket < 0) {
			//fprintf(stderr, "Error accepting socket");
			close(client_socket);
			exit(EXIT_FAILURE);
		}

		/* Use forking to handle multiple requests */
		if (!fork()) { // We are in the child
            close(server_socket);

            char buf[BUFSIZ];
            string input;
            //int keep_alive = 1; // Should we keep the connection alive?
            //int endreq = 0;
            fd_set s;
            struct timeval t;

            FD_ZERO(&s);
            FD_SET(client_socket, &s);

            t.tv_sec = 100;

            while((select(client_socket + 1, &s, NULL, NULL, &t)) > 0) {
	            /* Process server requests */
	            int buf_len;
	            if ((buf_len = recv(client_socket, &buf, BUFSIZ, 0)) > 0) { 
		            // if (!endreq) {
		            // 	if (buf[strlen(buf) - 2] == '\r' && buf[strlen(buf) - 3] == '\r') {
		            // 		endreq = 1;
		            // 	}
		            // }
	            	//if (endreq) { // Check for trailing newline in request
	            		string input(buf);
						vector<string> request; // Convert request to vector
						int improper_format = 0; // Is the request properly formatted?

						//getline(cin, input);
						stringstream ss(input);

						/* Convert input string to vector */
						string element;
						string line;
						while (getline(ss, line)) {
							stringstream line_ss(line);
							while (getline(line_ss, element, ' ')) {
					        	request.push_back(element);
					    	}
					    }

					    /* Check that the request is correctly formatted */
						improper_format = checkError(request);

						if (improper_format) {
							writeError(improper_format, client_socket, request);
						}
						else {
			            	/* Follow the http request */
			            	// if(strstr(buf, "Connection: keep-alive")) {
			            	// 	t.tv_sec = 10;
			            	// 	keep_alive = 1;
			            	// }
			            	// else{
			            	// 	t.tv_sec = 0;
			            	// 	keep_alive = 0;
			            	// }
			            	if (request[0] == "GET") {
			            		string uri = request[1];
			            		proxyConnect(client_socket, uri);
			            	}
			            	else {
			            		string response = "Sorry, only GET requests are supported at this time.\n";
								send(client_socket, response.c_str(), strlen(response.c_str()), 0);
			            	}

						}
						memset(&buf[0], 0, sizeof(buf));
						//endreq = 0;
					//}
					//}
					//else {
					//	string convert(buf); // Input request by client
					//	input = convert;
					//}

					// Carriage Return
	            	//if (buf[0] == '\r') {
					//	endreq = 1 - endreq;
	            	//}
	            	memset(&buf[0], 0, sizeof(buf)); // Reset the buffer for next input
	            }
        	}

        }
        close(client_socket);
	}
	close(server_socket);

	return 0;
}


void proxyConnect(int sock, string uri) {
	string element, url, host, location;
	char buf[BUF_SIZE];
	int port, which_element = 0, got_port = 0;
	int httpsocket;
	int buf_len;
    struct hostent *phe;
    struct sockaddr_in sin;

	stringstream ss(uri);

	while (getline(ss, element, ':')) {
		if(which_element == 1) {
			url = element;
		}
		else if (which_element == 2) {
			if (!(stringstream(element) >> port)) {
				cout << "Invalid port" << endl;
			}
			else {
				got_port = 1;
			}
		}
		which_element++;
	}

	if (!got_port)
		port = 80;

	host = url.substr(2);

	int pos = host.find('/');
	location = host.substr(pos);

	host.pop_back();

	url.insert(0, "http:");

	string req = "GET " + location + " HTTP/1.0\r\nHost: " + host + "\r\nConnection: keep-alive\r\n\r\n";

    /* Following code is adapted from echoClient.c given on Moodle */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    if ((sin.sin_port = htons((unsigned short)port)) == 0) {
        cout << "Can't allocate port" << endl;
        return;
    }

    if ((phe = gethostbyname(host.c_str()))) {
        memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
    }
    else if ((sin.sin_addr.s_addr = inet_addr(url.c_str())) == INADDR_NONE) {
    	return;
    }

    httpsocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (httpsocket < 0) {
    	cout << "Can't create socet" << endl;
    	return;
    }

    if (connect(httpsocket, (struct sockaddr *)&sin, sizeof(sin)) < 0)  {
    	cout << "Can't connect to " << url << endl;
    	return;
    }
    // End echo adapted code

	if (send(httpsocket, req.c_str(), strlen(req.c_str()), 0) < 0) {
	    cout << "Site not found" << endl;
	    return;
	}    
	if (send(httpsocket, "\r\n", strlen("\r\n"), 0) < 0) {
	    cout << "Site not found" << endl;
	    return;
	}

	while((buf_len = recv(httpsocket, &buf, BUFSIZ, 0)) > 0) {
		buf[buf_len] = '\0';
		if (send(sock, &buf, buf_len, 0) < 0) {
	      cout << "Failed sending" << endl;
	      return ;
	    }

	}
	close(httpsocket);
}

int checkError(vector<string> request) {
	string method, uri, version;

	/* Check number of inputs */
	if (request.size() < 3) {
		return -1;
	}
	
	method = request.front();
	request.erase(request.begin(), request.begin()+1);

	uri = request.front();
	request.erase(request.begin(), request.begin()+1);

	version = request.front();
	request.erase(request.begin(), request.begin()+1);

	/* Check that the method is valid */
	if(!(method == "GET" || method == "HEAD" || method == "POST" || method == "PUT" || 
		 method == "DELETE" || method == "CONNECT" || method == "OPTIONS" || method == "TRACE")) {

		return -2;
	}	

	/* Check that the uri exists */

	/* Check that the version is valid */
	if (!((version.compare(0, 8, "HTTP/1.0") == 0 ) || (version.compare(0, 8, "HTTP/1.1") == 0 ) || (version.compare(0, 6, "HTTP/2") == 0 ))) {
		return -4;
	}
	if (version.compare(0, 8, "HTTP/1.1") == 0)
		return -5;
	if (version.compare(0, 6, "HTTP/2") == 0)
		return -6;

	return 0;
}

/* Write the error to the socket using the following error codes:
 * -1 for invalid number of inputs
 * -2 for invalid method
 * -3 for invalid uri
 * -4 for invalid version
 * -5 for unsupported version HTTP 1.1
 * -6 for unsupported version HTTP 2
 */
void writeError(int proxy_error, int socket, vector<string> request) {
	string response;
	switch(proxy_error) {
		case -1:
			response = "Request Format: <method> <uri> <version>\n";
			send(socket, response.c_str(), strlen(response.c_str()), 0);
			break;
		case -2:
			response = "HTTP/1.0 400 Bad Request: Invalid Method: " + request[0] + "\n";
			send(socket, response.c_str(), strlen(response.c_str()), 0);
			break;
		case -3:
			response = "HTTP/1.0 400 Bad Request: Invalid uri: " + request[1] + "\n";
			send(socket, response.c_str(), strlen(response.c_str()), 0);
			break;
		case -4:
			response = "HTTP/1.0 400 Bad Request: Invalid Version: " + request[2] + "\n";
			send(socket, response.c_str(), strlen(response.c_str()), 0);
			break;
		case -5:
			response = "HTTP/1.0 400 Bad Request: Invalid Version: " + request[2] + "\n"; // Should this be the same as -6?
			send(socket, response.c_str(), strlen(response.c_str()), 0);
			break;
		case -6:
			response = "HTTP/1.0 500 Internal Server Error: cannot allocate memory\n";
			send(socket, response.c_str(), strlen(response.c_str()), 0);
			break;
		default:
			response = "Unexpect error while processing request\n";
			send(socket, response.c_str(), strlen(response.c_str()), 0);
			break;
	}
}
