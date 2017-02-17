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
#include <memory.h>
#include <openssl/sha.h>
 #include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <unordered_map>
#include <netdb.h>
#include <atomic>
#include <mutex>

#define BUFFER_SIZE 80000
#define SIZE_MESSAGE_SIZE 17

//put in the global namespace to avoid ambiguity with other byte typedefs
typedef unsigned char BYTE; 
typedef std::vector<BYTE> ByteVector;
typedef std::string String;


struct Request {
    String method;
    String uri;
    String version;
    std::unordered_map<String, String> headers;
    String data;
    String fileext;
    String error;
    Request *next;
};

struct CachedResponse {
    ByteVector data; //cached payload will be stored here
    String host;
    String uri;
    time_t expireTime;
    std::mutex lock;
};

//unordered associative container
std::unordered_map<String, CachedResponse *> cache;

void AddtoCache( String key, CachedResponse *cachedResponse );

Request *threadRequest( BYTE *requestString, unsigned long requestStringLength );

String getKey( String host, String url );

void *requestCycle( void *incomingSocket );

void *crawlPage( void *hashArg );

void *cleanCache( void * );

void closeSocket( int fd );

ByteVector forwardToServer( String host, String httpVersion, ByteVector requestData );

ByteVector getRequest( int sock );

void sendResponse( ByteVector response, int sock );

int getSO_ERROR( int fd );

void closeSocket( int fd );

int CACHE_TTL = 200;

/***********************************************************************
 * main function
 * ********************************************************************/
int main( int argc, char *argv[] ) {
  if ( argc < 2 ||argc > 3 ) {
    printf( "USAGE: ./webproxy <port number> <timeout (cache gets cleared in client after timeout) in secs>\n" );
    exit( 1 );
  }
  int port = atoi( argv[ 1 ] );
  //sets the cache time out from the command line
  if( argc ==3)
  {
	CACHE_TTL = atoi(argv[2]);
	printf("using timeout of %d seconds for cached responses\n", CACHE_TTL);
  }
  int serverSocket, clientSocket, c, *tempSocket;
  struct sockaddr_in serverSockaddr_in, clientSockaddr_in;

  // Create socket
  serverSocket = socket( AF_INET, SOCK_STREAM, 0 );
  if ( serverSocket == -1 ) {
    perror( "Could not create socket" );
    exit( 1 );
  }

  // Allow server to reuse port/addr if in TIME_WAIT state
  int enable = 1;
  if ( setsockopt( serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof( int ) ) < 0 ) {
    perror( "setsockopt(SO_REUSEADDR) failed" );
    exit( 1 );
  }

  //Prepare the sockaddr_in structure
  serverSockaddr_in.sin_family = AF_INET;
  serverSockaddr_in.sin_addr.s_addr = INADDR_ANY;
  serverSockaddr_in.sin_port = htons( ( uint16_t ) atoi( argv[ 1 ] ) );


  if ( bind( serverSocket, ( struct sockaddr * ) &serverSockaddr_in, sizeof( serverSockaddr_in ) ) < 0 ) {
    perror( "bind failed. Error" );
    exit( 1 );
  }

  listen( serverSocket, 3 );
  c = sizeof( struct sockaddr_in );

  printf( "web proxy Listening for connection, port %d\n", port );
  clientSocket = accept( serverSocket, ( struct sockaddr * ) &clientSockaddr_in, ( socklen_t * ) &c );

  while ( clientSocket ) {
    if ( clientSocket < 0 ) {
      perror( "failed on accept" );
      continue;
    }

    pthread_t requestThread;
    tempSocket = ( int * ) malloc( 4 );
    *tempSocket = clientSocket;

    if ( pthread_create( &requestThread, NULL, requestCycle, ( void * ) tempSocket ) < 0 ) {
      perror( "could not create thread" );
      return 1;
    }
    clientSocket = accept( serverSocket, ( struct sockaddr * ) &clientSockaddr_in, ( socklen_t * ) &c );
  }
}

/***********************************************************************
 * checks to see if an entry exists for the key, if not creates a new 
 * cache entry
 * takes in a pointer to cached info for a website and the key to it 
 * will be hashed to
 * ********************************************************************/
void AddtoCache( String key, CachedResponse *cachedResponse ) {
  if ( cache.find( key ) != cache.end() ) {
    printf( "Updating key(%s) in cache\n", key.c_str() );
    std::lock_guard<std::mutex> newLock( cache[ key ]->lock );
    cache[ key ] = cachedResponse;
  }
  else {
    printf( "Adding (%s) to cache\n", key.c_str() );
    cache[ key ] = cachedResponse;
  }
}

void *cleanCache( void *arg ) {
  printf("cleaning the cache\n");
  while ( true ) {
    sleep( CACHE_TTL * 2 );
    printf( "Clearing cache1\n" );

    time_t currentTime;
    time( &currentTime );
    typedef std::unordered_map<String, CachedResponse *>::iterator it_type;
    for ( it_type i = cache.begin(); i != cache.end(); i++ ) {
      if ( i->second->expireTime < currentTime ) {
        printf( "Deleting from cache(%s)", i->first.c_str() );
        delete ( cache[ i->first ] );
      }
    }
  }
}

ByteVector getRequest( int sock ) {
  unsigned char clientMessage[BUFFER_SIZE];
  ByteVector request = ByteVector();

  bzero( clientMessage, BUFFER_SIZE );
  ssize_t readSize = recv( sock, clientMessage, BUFFER_SIZE - 1, 0 );

  printf( "Read(%ld): ", readSize );
  for ( int i = 0; clientMessage[ i ] != '\n' && i < readSize; i++ ) 
  { 
	  printf( "%c", clientMessage[ i ] ); 
  }
  printf( "\n" );
  if ( readSize == -1 ) {
    char err[6] = "ERROR";
    printf( "recv error" );
    request.insert( request.begin(), err, err + 5 );
    perror( "recv failed" );
    return request;
  }
  request.insert( request.end(), clientMessage, clientMessage + readSize );

  return request;
}

void sendResponse( ByteVector response, int sock ) {
  BYTE *sendBuffer = response.data();
  size_t responseSizeRemaining = response.size();
  while ( responseSizeRemaining > 0 ) {
    ssize_t bytesSent = send( sock, sendBuffer, responseSizeRemaining, 0 );
    if ( bytesSent < 0 ) {
      perror( "Error sending get body" );
    }
    sendBuffer += bytesSent;
    responseSizeRemaining -= bytesSent;
  }
}

/***********************************************************************
 *
 **********************************************************************/
void getHostAndDataFromCache( String key, String *page, String *host, String *uri ) {
  std::lock_guard<std::mutex> lock( cache[ key ]->lock );
  CachedResponse *cachedResponse = cache[ key ];
  *page = ( char * ) cachedResponse->data.data();
  *host = cachedResponse->host;
  *uri = cachedResponse->uri;
}


/***********************************************************************
 *threaded function for prefetching links in a page
 **********************************************************************/
void *crawlPage( void *hashArg ) {
  String key = ( char * ) hashArg;

  // Get the cached entry
  String page;
  String host;
  String originalHost;
  String originaluri;
  getHostAndDataFromCache( key, &page, &originalHost, &originaluri );

  size_t minIndex = 0;

  while ( minIndex < page.length() ) {
    // Look for the next html tag, enclosed by <>
    // Stop processing if no more tags found
    size_t nextLAngle = page.find( "<", minIndex );
    if ( nextLAngle == String::npos ) { break; }
    size_t nextRAngle = page.find( ">", nextLAngle );
    if ( nextRAngle == String::npos ) { break; }
    String tag = page.substr( nextLAngle, nextRAngle - nextLAngle + 1 );
    printf( "Crawler found a tag: %s\n", tag.c_str() );

    // Update the loop variable
    minIndex = nextRAngle + 1;

    // Get the content of href or src attributes, if present
    size_t nextItem = std::min( tag.find( "href=\"" ), tag.find( "src=\"" ) );
    size_t hrefBegin = tag.find( "\"", nextItem );
    size_t hrefEnd = tag.find( "\"", hrefBegin + 1 );

    // If the item found is within the current tag, process it
    if ( hrefEnd >= hrefBegin
         && hrefBegin != String::npos
         && hrefEnd != String::npos ) 
    {
			 
      String url = tag.substr( hrefBegin + 1, hrefEnd - hrefBegin - 1 );
      
      // Don't prefetch same page anchors		
      if ( url.find( "#" ) == 0 ) {		
        continue;		
      }		
        // If we found a relative path, prepend the current location (using the original url as the best guess)		
      else if ( url[ 0 ] != '/' && url.find( "http://" ) != 0 && url.find( "https://" ) != 0 ) {		
        originaluri = originaluri.substr( 0, originaluri.rfind( "/" ) + 1 );		
        url = originaluri + url;	
      }
      if ( url.find( "#" ) == 0 ) { continue; } // Don't prefetch same page anchors
      String requestString = "GET " + url + " HTTP/1.0\n\n";
      printf( "Crawler created request: %s\n", requestString.c_str() );
      ByteVector requestData = ByteVector( requestString.begin(), requestString.end() );

      // If the uri is external to the page, find the host, otherwise assume the link is relative
      if ( url.find( "http://" ) == 0 || url.find( "https://" ) == 0 ) 
      {
        size_t hostStart = url.find( "//" ) + 2;
        size_t hostEnd = url.find( "/", hostStart );
        hostEnd = ( hostEnd == std::string::npos ) ? url.length() - 1 : hostEnd;
        host = url.substr( hostStart, hostEnd - hostStart );
      }
      else 
      {
        host = originalHost;
      }

      // Create a new key value
      BYTE *newHash = ( BYTE * ) malloc( SHA256_DIGEST_LENGTH * sizeof( BYTE ) );
      String newKey = getKey( host, url );
      SHA256( ( BYTE * ) key.c_str(), key.length(), newHash );

      // If the uri is not cached or the cache is expired, add to cache
      time_t currentTime;
      time(&currentTime);
      if ( cache.find( newKey ) == cache.end() || cache[ newKey ]->expireTime < currentTime ) 
      {
        // Get the response
        ByteVector response = forwardToServer( host, "HTTP/1.0", requestData );

        // If we got a response cache it
        if ( response.size() > 0 ) 
        {
          // Create new cached entry
          CachedResponse *newResponse = new CachedResponse();
          newResponse->data = response;
          newResponse->expireTime = currentTime + CACHE_TTL;
          newResponse->host = host;
          newResponse->uri = url;

          AddtoCache( newKey, newResponse );
        }
      }
    }
  }
  return 0;
}

ByteVector forwardToServer( String host, String httpVersion, ByteVector requestData ) {
  ByteVector response = ByteVector();

  struct addrinfo hints, *results;
  bzero(&hints, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  
  struct sockaddr_in server;
  int newSock = socket( AF_INET, SOCK_STREAM, 0 );
  if ( newSock == -1 ) {
    printf( "Could not connect to socket for host(%s)", host.c_str() );
    return response;
  }

  // Allow client to reuse port/addr if in TIME_WAIT state
  int enable = 1;
  if ( setsockopt( newSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof( int ) ) < 0 ) {
    printf( "Could not reuse socket for host(%s)\n", host.c_str() );
    return response;
  }

  // Find port, if specified in the host string
  int port = 80;
  std::string temp = host;
  if ( host.find( "http" ) == 0 ) {
    temp = host.substr( 6, host.length() - 6 );
  }
  size_t delimiterPosition = temp.find( ":" );
  if ( delimiterPosition != String::npos && delimiterPosition < host.length() ) {
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
  printf("Looking for host(%s)\n", host.c_str());

  struct hostent *newHostent = gethostbyname( host.c_str() );
  if ( newHostent == NULL ) {
    printf( "Could not find host from hostname(%s)\n", host.c_str() );
    return response;
  }

  memcpy( &server.sin_addr, newHostent->h_addr_list[ 0 ], ( size_t ) newHostent->h_length );
  server.sin_family = AF_INET;
  server.sin_port = htons( ( uint16_t ) port );

  if ( connect( newSock, ( struct sockaddr * ) &server, sizeof( server ) ) < 0 ) {
    printf( "Connect failed for host(%s). Error: %d\n", host.c_str(), errno );
    return response;
  }

  ssize_t bytesToSend = requestData.size();
  BYTE *sendBuffer = requestData.data();

  while ( bytesToSend > 0 ) {
    ssize_t bytesSent = send( newSock, sendBuffer, requestData.size(), 0 );
    bytesToSend -= bytesSent;
    sendBuffer += bytesSent;

    if ( bytesSent < 0 ) {
      printf( "Error sending get request to host(%s)\n", host.c_str() );
    }
  }

  unsigned char readBuffer[BUFFER_SIZE];
  bzero( readBuffer, BUFFER_SIZE );
  ssize_t bytesReceived;

  // If request is HTTP/1.0 read data until the stream closes
  if ( httpVersion.compare( "HTTP/1.0" ) == 0 ) {
    bytesReceived = recv( newSock, readBuffer, BUFFER_SIZE - 1, 0 );
    while ( bytesReceived > 0 ) {
      response.insert( response.end(), readBuffer, readBuffer + bytesReceived );
      bzero( readBuffer, BUFFER_SIZE );
      bytesReceived = recv( newSock, readBuffer, BUFFER_SIZE - 1, 0 );
    }
  }
    // If request is HTTP/1.1 pares the headers
  else {
    // Read the header a byte at a time
    std::string header = "";
    while ( ( bytesReceived = recv( newSock, readBuffer, 1, 0 ) ) > 0 ) {
      header.push_back( readBuffer[ 0 ] );
      bzero( readBuffer, BUFFER_SIZE );
      if ( header.length() > 2 && header.find( "\r\n\r\n" ) != std::string::npos ) {
        break;
      }
    }

    // If response uses Content-Length, read that many bytes
    if ( header.find( "Content-Length:" ) != std::string::npos ) {
      printf( "Getting body using contentLength\n" );
      size_t begin = header.find( ":", header.find( "Content-Length:" ) );
      size_t end = header.find( "\n", begin );
      ssize_t contentLength = std::stoi( header.substr( begin + 1, end - begin ) );
      printf( "Found contentLength(%ld), ", contentLength );
      while ( response.size() < contentLength ) {
        bzero( readBuffer, BUFFER_SIZE );
        bytesReceived = recv( newSock, readBuffer, BUFFER_SIZE - 1, 0 );
        printf( "bytesReceived(%ld)\n", bytesReceived );
        response.insert( response.end(), readBuffer, readBuffer + bytesReceived );
      }
    }

      //If response is using chunked encoding, read bytes until we receive a 0 chunk
    else if ( header.find( "Transfer-Encoding: chunked" ) != std::string::npos ) {
      std::string chunkDescription = "";
      ssize_t chunkSize = 1;

      while ( chunkSize > 0 ) {
        // Read the chunk string
        chunkDescription = "";
        bzero( readBuffer, BUFFER_SIZE );
        while ( recv( newSock, readBuffer, 1, 0 ) > 0 ) {
          chunkDescription.push_back( readBuffer[ 0 ] );
          response.push_back( readBuffer[ 0 ] );
          bzero( readBuffer, BUFFER_SIZE );
          if ( chunkDescription.length() > 2 && chunkDescription.find( "\r\n" ) != std::string::npos ) {
            break;
          }
        }

        // Get the decimal value for the chunk size
        chunkSize = strtol( chunkDescription.c_str(), NULL, 16 );;

        // Read chunkSize bytes from the sockeet
        bytesReceived = 0;
        while ( bytesReceived < chunkSize ) {
          bzero( readBuffer, BUFFER_SIZE );
          bytesReceived += recv( newSock, readBuffer, 1, 0 );
          response.push_back( readBuffer[ 0 ] );
        }
      }
    }

    else 
    {
      printf( "No Content-Length or Transfer-Encoding header received.\n" );
      return response;
    }
	
    // replace the headers
    response.insert( response.begin(), header.begin(), header.end() );
  }

  if ( bytesReceived == -1 && errno != 11 ) {
    printf( "recv failed for host(%s), errno(%d)\n", host.c_str(), errno );
  }

  return response;
}

void *requestCycle( void *incomingSocket ) {
  //Get the socket descriptor
  int clientSocket = *( int * ) incomingSocket;

  ByteVector requestData = getRequest( clientSocket );
  Request *request = threadRequest( requestData.data(), requestData.size() );

  if ( request->error.compare( "" ) != 0 ) 
  {
    sendResponse( ByteVector( request->error.begin(), request->error.end() ), clientSocket );
	return 0;
  }

  BYTE *hash = ( BYTE * ) malloc( SHA256_DIGEST_LENGTH * sizeof( BYTE ) );
  String key = getKey( request->headers[ "Host" ], request->uri );
  SHA256( ( BYTE * ) key.c_str(), key.length(), hash );

  time_t currentTime;
  time( &currentTime );

  if ( cache.find( key ) == cache.end() ) 
  {
    printf( "Cached response not found\n" ); 
  }
  else if ( cache[ key ]->expireTime < currentTime )
  { 
    printf( "Cached response expired\n" ); 
  }

  if ( cache.find( key ) == cache.end() || cache[ key ]->expireTime < currentTime ) {
    // Get the request form the remote server
    ByteVector responseData =
      forwardToServer( request->headers[ "Host" ], request->version, requestData );
	
	
    // Send it back to the client
    sendResponse( responseData, clientSocket );
    closeSocket( clientSocket );

    // Create new cache entry
    CachedResponse *newCacheEntry = new CachedResponse();
    newCacheEntry->data = responseData;
    newCacheEntry->expireTime = currentTime + CACHE_TTL;
    newCacheEntry->host = request->headers[ "Host" ];
    newCacheEntry->uri = request->uri;

    AddtoCache( key, newCacheEntry );

    // Prefetch links from this page
    // If the response wasn't an html page, there probably aren't any crawlable links
    if ( request->fileext.compare( ".html" ) == 0 ) {
      pthread_t crawlerThread;
      if ( pthread_create( &crawlerThread, NULL, crawlPage, ( void * ) key.c_str() ) < 0 ) {
        perror( "could not create thread" );
        exit( 1 );
      }
    }
  }
  else {
    printf( "Found cached response for key(%s)\n", key.c_str() );
    sendResponse( cache[ key ]->data, clientSocket );
    closeSocket( clientSocket );
  }

  return 0;
}



String getKey( String host, String url ) {
  // If url already has the host, just strip the protocol off and return it
  if ( url.find( "http://" ) == 0 || url.find( "https://" ) == 0 ) {
    size_t start = url.find( "//" ) + 2;
    return url.substr( start, url.length() - start );
  }
  
  printf("inside get key\n");

  // If the host string has a protocol, strip it off
  if ( host.find( "http://" ) == 0 || host.find( "https://" ) == 0 ) {
    size_t start = host.find( "//" ) + 2;
    host = host.substr( start, host.length() - start );
  }

  // Strip any trailing or leading '/' chars
  while ( host.find( "/" ) == host.length() - 1 ) {
    host = host.substr( 0, host.length() - 2 );
  }
  while ( url.find( "/" ) == 0 ) {
    url = url.substr( 1, url.length() - 2 );
  }
  
  // Don't include anchors in key		
  size_t anchorPosition = url.find( "#" );		
  if ( anchorPosition != std::string::npos ) {		
    url = url.substr( anchorPosition, url.length() - anchorPosition );		
  }
  return host + "/" + url;
}


Request *threadRequest( BYTE *requestString, unsigned long requestStringLength ) {
  Request *request = new Request; // return value
  int i = 0; //index in requestString
  request->error = "";
  request->next = NULL;

  String key;
  String value;
  String fileext;

  // get method
  request->method = "";
  while ( i < requestStringLength && !isspace( requestString[ i ] ) ) {
    request->method.append( 1u, requestString[ i ] );
    i++;
  }

  // Check for supported methods
  if ( request->method.compare( "GET" ) != 0 && request->method.compare( "POST" ) != 0 ) {
    request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Method: ";
    request->error.append( request->method );
    request->error.append( "</body></html>" );
    return request;
  }

  // move to URI
  while ( i < requestStringLength && isspace( requestString[ i ] ) ) {
    i++;
  }

  // get uri
  request->uri = "";
  while ( i < requestStringLength && !isspace( requestString[ i ] ) ) {
    request->uri.append( 1u, requestString[ i ] );
    i++;
  }

  //Check for directory request
  request->fileext = "";
  try {
    request->fileext = request->uri.substr( request->uri.rfind( "." ),
                                            request->uri.length() - request->uri.rfind( "." ) );
  }
  catch ( std::out_of_range &exc ) {
    request->fileext = ".html";
  }



  // move to version
  while ( i < requestStringLength && isspace( requestString[ i ] ) ) {
    i++;
  }

  // get version
  request->version = "";
  while ( i < requestStringLength && !isspace( requestString[ i ] ) ){
    request->version.append( 1u, requestString[ i ] );
    i++;
  }

  // move to end of first line
  while ( i < requestStringLength && isspace( requestString[ i ] ) ) {
    i++;
  }

  // read one new line, if next character is not a newline, then this request is invalid
  if ( i < requestStringLength && isspace( requestString[ i ]) ) {
    if ( requestString[ i ] == '\n' ) {
      i++;
    }
    else if ( requestString[ i ] == 13 ) {
      i += 2;
    }
    else {
      request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Malformed Request</body></html>";
      return request;
    }
  }

  //printf( "request.version: {%s}\n", request->version.c_str() );
	
  if ( request->version.compare( "HTTP/1.0" ) != 0 ) {
//  if ( request->version.compare( "HTTP/1.0" ) != 0 && request->version.compare( "HTTP/1.1" ) != 0 ) {
    request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Invalid Version: ";
    request->error.append( request->version );
    request->error.append( "</body></html>" );
    return request;
  }

  // get headers if any
  while ( i < requestStringLength && !isspace( requestString[ i ]) ) {
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

    // Add key:value to headers map
    request->headers[ key.substr( 0, key.length() - 1 ) ] = value;

    // printf("key:value = {%s:%s}\n", key.substr(0,key.length()-1).c_str(), value.c_str());

    // Read the CRLF
    if ( i < requestStringLength && isspace( requestString[ i ]) ) {
      if ( requestString[ i ] == '\n' ) {
        i++;
      }
      else if ( requestString[ i ] == 13 ) {
        i += 2;
      }
      else {
        request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Malformed Request</body></html>";
        return request;
      }
    }
  }

  // Read the second CRLF
  if ( i < requestStringLength && isspace( requestString[ i ]) ) {
    if ( requestString[ i ] == '\n' ) {
      i++;
    }
    else if ( requestString[ i ] == 13 ) {
      i += 2;
    }
    else {
      request->error = "HTTP/1.0 400 Bad Request\n\n<html><body>400 Bad Request Reason: Malformed Request</body></html>";
      return request;
    }
  }

  // if request is post
  if ( request->method.compare( "POST" ) == 0 ) {
    request->data = "";
    while ( i < requestStringLength ) {
      request->data.append( 1u, requestString[ i ] );
      i++;
    }
  }
    // look for pipelined request
  else if ( request->method.compare( "GET" ) == 0 && request->version.compare( "HTTP/1.1" ) == 0 
            && strncmp( ( ( char * ) requestString ) + i, "GET", 3 ) == 0 ) {
    request->next = threadRequest( requestString + i, requestStringLength - i );
  }

  return request;
}


int getSO_ERROR( int fd ) {
  int err = 1;
  socklen_t len = sizeof err;
  if ( -1 == getsockopt( fd, SOL_SOCKET, SO_ERROR, ( char * ) &err, &len ) ) {
    perror( "getSO_ERROR" );
  }
  if ( err )
    errno = err;              // set errno to the socket SO_ERROR
  return err;
}


void closeSocket( int fd ) {
  if ( fd >= 0 ) {
    getSO_ERROR( fd ); // first clear any errors, which can cause close to fail
    if ( shutdown( fd, SHUT_RDWR ) < 0 ) { // secondly, terminate the 'reliable' delivery
      if ( errno != ENOTCONN && errno != EINVAL ) { // SGI causes EINVAL
        perror( "shutdown" );
      }
    }
    if ( close( fd ) < 0 ) { // finally call close()
      perror( "close" );
    }
  }
}
