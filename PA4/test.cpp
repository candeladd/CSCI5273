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

typedef unsigned char BYTE;
typedef std::string String;
using std::cout;
struct Request {
    String method;
    String uri;
    String version;
    String data;
    String fileext;
    String error;
    Request *next;
    int val;
};

int main()
{
	Request *request = new Request; 
	String uri = "fuck	porkhandlamblip stick";
	int i = 0;
	while(!isspace(uri[i]))
	{
		i++;
	}
	i++;
	while (!isspace(uri[i]) ) 
	{
     request->method.append( 1u, uri[ i ] );
     i++;
    }
    request->method.append(1u, '\n');
    cout << request->method;
    
    int foo = 5;
    request->val = foo;
    cout << request->val<<"\n";


}
