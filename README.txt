Simple HTTP server.

Andrew Candelaresi
Colorado University Boulder
Network Systems Fall 2016

written in C

Compile server: 
in terminal window type:

gcc -pthread -o <desired executable name> server.c

run server:
in terminal window type:

./<desired executable name>

Simple HTTP server handles HTTP request from a client/web browser, processes the request and 
returns the proper HTTP response. 

The server handles multiple connections and request using pthreading.
