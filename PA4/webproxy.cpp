/***********************************************************************
 * Andrew Candelaresi
 * 
 * Network Systems Fall 2016
 * Programming Assignment 4 
 * Network proxy server
 * Help from stackoverflow, the opengroup, cplusplus.com
 * ********************************************************************/
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <unordered_map>
#include <netdb.h>
#include <memory.h>
#include <openssl/sha.h>
#include <iostream>
#include <mutex>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <atomic>

typedef std::string String;

typedef std::vector<unsigned char> ucVector;
struct Cash 
{
    ucVector data; //cached payload will be stored here
    String host, uri;
    time_t expireTime;
    std::mutex lock;
};
struct urlinfo
{
	char* proto, dns, port, path;
};
struct Request 
{
    String method, uri, version, data, fileext, error;
    Request *next;
    std::unordered_map<String, String> headers;
   
};
#define BUFFER_SIZE 80000
#define SIZE_MESSAGE_SIZE 17
#define BACKLOG 10
//unorderedassociative container
std::unordered_map<String, Cash *> cache;

int CACHE_TTL = 200;


/**********************************************************************
 * create a socket file descriptor
 * and bind and listen on it
 * *******************************************************************/
int bindSocket(const char *port)
{
	int sockfd;
	struct addrinfo hints, *servinfo, *p;\
	socklen_t sin_size;
	char s[INET6_ADDRSTRLEN];
	int va;
	int yes=1;
	
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;  // IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; //a TCP connection
	hints.ai_flags = AI_PASSIVE; // get my own IP
	
	if ((va = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "gettaddrinfo: %s\n", gai_strerror(va));
		return 1;
	}
	
	    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
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
		perror("listen");
		exit(1);
	}
	
	return sockfd;
}

/***********************************************************************
 * extrapolates the HTTP request and breaks it into method, url,
 * version etc...
 * ********************************************************************/
Request *parseRequest( unsigned char *requestString, unsigned long requestStringLength ) 
{
  Request *request = new Request; // return value
  int i = 0; //index in requestString
  request->error = "";
  request->next = NULL;

  String key;
  String value;
  String fileext;

  // get method
  request->method = "";
  while ( i < requestStringLength && !isspace( requestString[ i ] ) ) 
  {
    request->method.append( 1u, requestString[ i ] );
    i++;
  }

	//std::cout << "method" << request->method<< "\n";
  // Check for supported methods
  if ( request->method.compare( "GET" ) != 0 && request->method.compare( "POST" ) != 0 ) 
  {
     //std::cout << "method1 =" << request->method<< "\n";
    request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Method: ";
    request->error.append( request->method );
    request->error.append( "</body></html>" );
   
    return request;
  }

  // move to URI
  while ( i < requestStringLength && isspace( requestString[ i ] ) ) 
  {
    i++;
  }

  // get uri
  request->uri = "";
  while ( i < requestStringLength && !isspace( requestString[ i ] ) ) 
  {
    request->uri.append( 1u, requestString[ i ] );
    i++;
  }

  //Check for directory request
  request->fileext = "";
  try 
  {
    request->fileext = request->uri.substr( request->uri.rfind( "." ),
                                            request->uri.length() - request->uri.rfind( "." ) );
  }
  catch ( std::out_of_range &exc ) 
  {
    request->fileext = ".html";
  }



  // move to version
  while ( i < requestStringLength && isspace( requestString[ i ] ) ) 
  {
    i++;
  }

  // get version
  request->version = "";
  while ( i < requestStringLength && !isspace( requestString[ i ] ) )
  {
    request->version.append( 1u, requestString[ i ] );
    i++;
  }


  // move to end of first line
  while ( i < requestStringLength && isspace( requestString[ i ] ) ) 
  {
    i++;
  }

  // read one new line, if next character is not a newline, then this request is invalid
  while ( i < requestStringLength && isspace( requestString[ i ]) ) {
    if ( requestString[ i ] == '\n' ) 
    {
      i++;
    }
    else if ( requestString[ i ] == 13 ) 
    {
      i += 2;
    }
    else 
    {
		 //std::cout << "method2 = " << request->method<< "\n";
      request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Malformed Request</body></html>";
      return request;
    }
  }


  if ( request->version.compare( "HTTP/1.0" ) != 0 ) 
  {
  //if ( request->version.compare( "HTTP/1.0" ) != 0 && request->version.compare( "HTTP/1.1" ) != 0 ) {
    request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Version: ";
    request->error.append( request->version );
    request->error.append( "</body></html>" );
    return request;
  }
   request->version.append(1u, '\n');
   request->version.append(1u, '\n');

  // get headers if any
  while ( i < requestStringLength && !isspace( requestString[ i ]) ) 
  {
    // Read header name
    key = "";
    while ( i < requestStringLength && !isspace( requestString[ i ] ) )
    {
      key.append( 1u, requestString[ i ] );
      i++;
    }
	//std::cout << key << "\n";
    // move to value
    while ( i < requestStringLength && isspace( requestString[ i ] ) ) 
    {
      i++;
    }

    // Read header value
    value = "";
    while ( i < requestStringLength && !isspace( requestString[ i ]) ) 
    {
      value.append( 1u, requestString[ i ] );
      i++;
    }

    //check for return character
    request->headers[ key.substr( 0, key.length() - 1 ) ] = value;

    // printf("key:value = {%s:%s}\n", key.substr(0,key.length()-1).c_str(), value.c_str());
	 while ( i < requestStringLength && isspace( requestString[ i ] ) ) {
       i++;
     }

    // check for neline
    if ( i < requestStringLength && isspace( requestString[ i ]) ) 
    {
      if ( requestString[ i ] == '\n' ) 
      {
        i++;
      }
      else if ( requestString[ i ] == 13 ) 
      {
        i += 2;
      }
      else {
		   //std::cout << "method4 = " << request->method<< "\n";
        request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Malformed Request</body></html>";
        return request;
      }
    }
  }

  //pass newline
  if ( i < requestStringLength && isspace( requestString[ i ]) ) 
  {
    if ( requestString[ i ] == '\n' ) {
      i++;
    }
    else if ( requestString[ i ] == 13 ) 
    {
      i += 2;
    }
    else 
    {
		 //std::cout << "method3 = " << request->method<< "\n";
      request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Malformed Request</body></html>";
      return request;
    }
  }

  // check if request is post
  if ( request->method.compare( "POST" ) == 0 ) 
  {
    request->data = "";
    while ( i < requestStringLength ) 
    {
      request->data.append( 1u, requestString[ i ] );
      i++;
    }
  }
    // look for more gets
  else if ( request->method.compare( "GET" ) == 0 && request->version.compare( "HTTP/1.1" ) == 0 
            && strncmp( ( ( char * ) requestString ) + i, "GET", 3 ) == 0 ) 
  {
    request->next = parseRequest( requestString + i, requestStringLength - i );
  }

  return request;
}


void *cleanCache( void *arg ) {
  std::cout << "cleaning the cache\n";
  while ( true ) {
    sleep( CACHE_TTL);
    std::cout << "Clearing cache1\n";

    time_t currentTime;
    time( &currentTime );
    typedef std::unordered_map<String, Cash *>::iterator it_type;
    for ( it_type i = cache.begin(); i != cache.end(); i++ ) {
      if ( i->second->expireTime < currentTime ) {
        std::cout << "Deleting from cache"<< i->first << "\n";
        delete ( cache[ i->first ] );
      }
    }
  }
}


int getSO_ERROR( int fd ) 
{
  int err = 1;
  socklen_t len = sizeof err;
  if ( -1 == getsockopt( fd, SOL_SOCKET, SO_ERROR, ( char * ) &err, &len ) ) 
  {
    perror( "getSO_ERROR" );
  }
  if ( err )
  {
    errno = err;              // set errno to the socket SO_ERROR
  }
  return err;
}

/***********************************************************************
 * reads the request off the socket
 * ********************************************************************/
ucVector getRequest( int sock ) {
  unsigned char clientMessage[BUFFER_SIZE];
  ucVector request = ucVector();

  bzero( clientMessage, BUFFER_SIZE );
  ssize_t readSize = recv( sock, clientMessage, BUFFER_SIZE - 1, 0 );

  std::cout << "client sent: ";
  for ( int i = 0; clientMessage[ i ] != '\n' && i < readSize; i++ ) 
  { 
	  printf( "%c", clientMessage[ i ] ); 
  }
  printf( "\n" );
  if ( readSize == -1 ) 
  {
    char err[6] = "ERROR";
    std::cout << "recv error";
    request.insert( request.begin(), err, err + 5 );
    perror( "recv failed" );
    return request;
  }
  request.insert( request.end(), clientMessage, clientMessage + readSize );

  return request;
}

/***********************************************************************
 * send response to the client
 * ********************************************************************/
void sendResponse( ucVector response, int sock ) 
{
	
  unsigned char *sendBuffer = response.data();
  std::cout << (sendBuffer);
  size_t responseSizeRemaining = response.size();
  while ( responseSizeRemaining > 0 ) {
    ssize_t bytesSent = send( sock, sendBuffer, responseSizeRemaining, 0 );
    if ( bytesSent < 0 ) {
      perror( "Error sending get body" );
    }
    sendBuffer += bytesSent;
    responseSizeRemaining -= bytesSent;
    //printf("inside sendresponse\n");
  }
}

void closeSocket( int fd ) 
{
  if ( fd >= 0 ) 
  {
    getSO_ERROR( fd ); // clear errors make sure socket doesnt hang
    if ( shutdown( fd, SHUT_RDWR ) < 0 ) { //eliminate access
      if ( errno != ENOTCONN && errno != EINVAL )
      {
        perror( "shutdown" );
      }
    }
    if ( close( fd ) < 0 ) 
    { // close socket
      perror( "close" );
    }
  }
}

String getKey( String host, String url ) 
{
  // If url already has the host, just strip the protocol off and return it
  if ( url.find( "http://" ) == 0 || url.find( "https://" ) == 0 ) 
  {
    size_t start = url.find( "//" ) + 2;
    return url.substr( start, url.length() - start );
  }
  
  printf("inside get key\n");
  if ( host.find( "http://" ) == 0 || host.find( "https://" ) == 0 ) 
  {
    size_t start = host.find( "//" ) + 2;
    host = host.substr( start, host.length() - start );
  }

  // strip last '/'
  while ( host.find( "/" ) == host.length() - 1 ) 
  {
    host = host.substr( 0, host.length() - 2 );
  }
  while ( url.find( "/" ) == 0 ) {
    url = url.substr( 1, url.length() - 2 );
  }
  
  // Don't include anchors in key		
  size_t anchorPosition = url.find( "#" );		
  if ( anchorPosition != std::string::npos ) 
  {		
    url = url.substr( anchorPosition, url.length() - anchorPosition );		
  }
  return host + "/" + url;
}
/***********************************************************************
 * if no cached result found send the request on to the server to 
 * fulfill
 * ********************************************************************/
ucVector forwardToServer( String host, String httpVersion, ucVector requestData ) 
{
  ucVector response = ucVector();

    struct sockaddr_in server;
  int newSock = socket( AF_INET, SOCK_STREAM, 0 );
  if ( newSock == -1 ) {
    std::cout << "Could not connect to socket for host" << host;
    return response;
  }

  // Allow client to reuse port/addr if in TIME_WAIT state
  int enable = 1;
  if ( setsockopt( newSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof( int ) ) < 0 ) 
  {
    std::cout << "Could not reuse socket for host "<< host <<"\n";
    return response;
  }

  // Find port, if specified in the host string
  int port = 80;
  String temp = host;
  if ( host.find( "http" ) == 0 ) {
    temp = host.substr( 6, host.length() - 6 );
  }
  size_t delimiterPosition = temp.find( ":" );
  if ( delimiterPosition != String::npos && delimiterPosition < host.length() ) 
  {
    port = std::stoi( host.substr( delimiterPosition + 1, host.length() - delimiterPosition + 1 ) );
    host = host.substr( 0, delimiterPosition );
  }
  if(host.compare("")==0)
  {
    String requestString = (char *)requestData.data();
    size_t beginHost = requestString.find("http");
    if (beginHost != String::npos ) 
    {		
      beginHost = requestString.find( "://", beginHost ) + 3;		
      size_t endHost = std::min( requestString.find( "/", beginHost ), requestString.find(" ", beginHost) );		
      if ( endHost > beginHost && endHost != String::npos ) 
      {		
        host = requestString.substr( beginHost, endHost - beginHost );		
      }		
    }		
  }		
  
    //collect address info
    //int getAddrInfoVal;
    //if((getAddrInfoVal = getaddrinfo(reqInfo->reqUrl->domain, port, &hints, &results)) != 0)
    //{
     //   printf("Error trying to get from %s:%s\n", reqInfo->reqUrl->domain, port);
      //  printf("Error trying to get the address info: %s\n", gai_strerror(getAddrInfoVal));
       // return -1 ;
    //}
  std::cout << "Looking for host " << host << "\n";
  
  struct hostent *newHostent = gethostbyname( host.c_str() );
  if ( newHostent == NULL ) 
  {
    printf( "Could not find host from hostname(%s)\n", host.c_str() );
    return response;
  }

  memcpy( &server.sin_addr, newHostent->h_addr_list[ 0 ], ( size_t ) newHostent->h_length );
  server.sin_family = AF_INET;
  server.sin_port = htons( ( uint16_t ) port );
  
  if ( connect( newSock, ( struct sockaddr * ) &server, sizeof( server ) ) < 0 ) 
  {
    printf( "Connect failed for host(%s). Error: %d\n", host.c_str(), errno );
    return response;
  }

  ssize_t bytesToSend = requestData.size();
  unsigned char *sendBuffer = requestData.data();
  
  while ( bytesToSend > 0 ) {
    ssize_t bytesSent = send( newSock, sendBuffer, requestData.size(), 0 );
    bytesToSend -= bytesSent;
    sendBuffer += bytesSent;

    if ( bytesSent < 0 ) 
    {
      printf( "Error sending get request to host(%s)\n", host.c_str() );
    }
  }
  unsigned char readBuffer[BUFFER_SIZE];
  bzero( readBuffer, BUFFER_SIZE );
  ssize_t bR;
  // If request is HTTP/1.0 read data until the stream closes
  if ( httpVersion.compare( "HTTP/1.0" ) == 0 ) 
  {
    if((bR = recv( newSock, readBuffer, BUFFER_SIZE - 1, 0 )) < 0)
      {
		  perror("error reading response");
		  
	  }
    while ( bR > 0 ) {
		 
      response.insert( response.end(), readBuffer, readBuffer + bR);
      bzero( readBuffer, BUFFER_SIZE );
      bR = recv( newSock, readBuffer, BUFFER_SIZE - 1, 0 );
    }
  }
    //parse header
  else 
  {
    // Read the header a byte at a time
    std::string header = "";
    while ( ( bR = recv( newSock, readBuffer, 1, 0 ) ) > 0 ) 
    {
      bzero( readBuffer, BUFFER_SIZE );
      if ( header.length() > 2 && header.find( "\r\n\r\n" ) != std::string::npos ) 
      {
        break;
      }
    }
	
    // check what type of content format we are getting
    if ( header.find( "Content-Length:" ) != std::string::npos ) 
    {
      std::cout << "Getting body using contentLength\n";
      size_t begin = header.find( ":", header.find( "Content-Length:" ) );
      size_t end = header.find( "\n", begin );
      ssize_t contentLength = std::stoi( header.substr( begin + 1, end - begin ) );
      std::cout << "Found contentLength" << contentLength << "\n";
      while ( response.size() < contentLength ) 
      {
        bzero( readBuffer, BUFFER_SIZE );
        bR = recv( newSock, readBuffer, BUFFER_SIZE - 1, 0 );
        std::cout << "bytesReceived "<< bR<< "\n";
        response.insert( response.end(), readBuffer, readBuffer + bR );
      }
    }
    else if ( header.find( "Encoding for transfer: chunked" ) != std::string::npos ) 
    {
      String readType = "";
      ssize_t chunkSize = 1;

      while ( chunkSize > 0 ) {
        // continure to read chunks until no more chunks
        readType = "";
        bzero( readBuffer, BUFFER_SIZE );
        while ( recv( newSock, readBuffer, 1, 0 ) > 0 ) 
        {
          readType.push_back( readBuffer[ 0 ] );
          response.push_back( readBuffer[ 0 ] );
          bzero( readBuffer, BUFFER_SIZE );
          if ( readType.length() > 2 && readType.find( "\r\n" ) != std::string::npos ) 
          {
            break;
          }
        }
        chunkSize = strtol( readType.c_str(), NULL, 16 );;

        bR = 0;
        while ( bR < chunkSize ) 
        {
          bzero( readBuffer, BUFFER_SIZE );
          bR += recv( newSock, readBuffer, 1, 0 );
          response.push_back( readBuffer[ 0 ] );
        }
      }
    }

    else 
    {
      std::cout << "Content-Length / Transfer-Encoding not specified\n";
      return response;
    }
	
	
    response.insert( response.begin(), header.begin(), header.end() );
  }

  if ( bR == -1 && errno != 11 ) 
  {
    printf( "recv failed for host(%s), errno(%d)\n", host.c_str(), errno );
  }
  return response;
}


/***********************************************************************
 * checks to see if an entry exists for the key, if not creates a new 
 * cache entry
 * takes in a pointer to cached info for a website and the key to it 
 * will be hashed to
 * ********************************************************************/
void CachePage( String lookup, Cash *pageinfo ) {
  if ( cache.find( lookup ) != cache.end() ) {
    std::cout << "Updating info for page " << lookup << " in cache\n";
    std::lock_guard<std::mutex> newLock( cache[ lookup ]->lock );
    cache[ lookup ] = pageinfo;
  }
  else {
   std::cout <<"Adding " <<lookup << " to cache\n";
    cache[ lookup ] = pageinfo;

  }
}


/***********************************************************************
 * function checks for a cached result if none found calls forward to 
 * server function
 * ********************************************************************/
void *threadRequest( void *incomingSocket ) 
{
  int clientSocket = *( int * ) incomingSocket;
	
  ucVector requestData = getRequest( clientSocket );
  Request *request = parseRequest( requestData.data(), requestData.size() );

  if ( request->error.compare( "" ) != 0 ) 
  {
    sendResponse( ucVector( request->error.begin(), request->error.end() ), clientSocket );
	return 0;
  }

  unsigned char *hash = ( unsigned char * ) malloc( SHA256_DIGEST_LENGTH * sizeof( unsigned char ) );
  String key = getKey( request->headers[ "Host" ], request->uri );
  SHA256( ( unsigned char * ) key.c_str(), key.length(), hash );

  time_t currentTime;
  time( &currentTime );

  if ( cache.find( key ) == cache.end() ) 
  {
    std::cout << "Cached response not found\n"; 
  }
  else if ( cache[ key ]->expireTime < currentTime )
  { 
    std::cout << "Cached response expired\n"; 
  }

  if ( cache.find( key ) == cache.end() || cache[ key ]->expireTime < currentTime ) 
  {
    
    // Get the request form the remote server
    ucVector responseData =
      forwardToServer( request->headers[ "Host" ], request->version, requestData);

    // Send it back to the client
    sendResponse( responseData, clientSocket );
    closeSocket( clientSocket );
  /*  for (int i = 0; i < responseData.size(); i++)
	{
       if(responseData[i] != 0){
            std::cout << (responseData[i]);
        }
	}
	printf("end\n");*/


    // Create new cache entry
    Cash *newCacheEntry = new Cash();
    newCacheEntry->data = responseData;
    newCacheEntry->expireTime = currentTime + CACHE_TTL;
    newCacheEntry->host = request->headers[ "Host" ];
    newCacheEntry->uri = request->uri;
    CachePage( key, newCacheEntry );
  }
  else 
  {
    std::cout << "Found cached response for key" << key << "\n";
    sendResponse( cache[ key ]->data, clientSocket );
    closeSocket( clientSocket );
  }

  return 0;
}



/***********************************************************************
 * main function
 * ********************************************************************/
int main( int argc, char *argv[] ) 
{
  
  socklen_t sin_size;
  if ( argc < 2 ||argc > 3 ) 
  {
    std::cout << "USAGE: ./webproxy <port number> <timeout (cache gets cleared in client after timeout) in secs>\n" ;
    exit( 1 );
  }
  int port = atoi( argv[ 1 ] );
  //sets the cache time out from the command line
  if( argc ==3)
  {
	CACHE_TTL = atoi(argv[2]);
	std::cout << "using timeout of " << CACHE_TTL << " seconds for cached responses\n";
  }
  int serverSocket, clientSocket, c, *tempSocket;
  struct sockaddr_in serverSockaddr_in, clientSockaddr_in;

	const char * portd = argv[ 1 ];
	
	int sockfd = bindSocket(portd);
	
	
  std::cout << "web proxy Listening for connection, port "<< port << "\n";
  
  
  while (1) 
  {
	 
	clientSocket = accept( sockfd, ( struct sockaddr * ) &clientSockaddr_in,  &sin_size );
    if ( clientSocket < 0)
    {
      perror( "failed on accept" );
      continue;
    }

    pthread_t requestThread;
    tempSocket = ( int * ) malloc( 4 );
    *tempSocket = clientSocket;
    if ( pthread_create( &requestThread, NULL, threadRequest, ( void * ) tempSocket ) < 0 ) 
    {
      perror( "could not create thread" );
      return 1;
    }
    
   // clientSocket = accept( serverSocket, ( struct sockaddr * ) &clientSockaddr_in, ( socklen_t * ) &c );
  }
  close(sockfd);
  return 0;
}
