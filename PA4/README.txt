Andrew Candelaresi
Fall 2016
Network Systems
PA4 Proxy server
Not implemented:
Prefetch
Keep-alive

pages_from_server - Pass <returned in .47 seconds shorter than 5 but still longer than the instantaneous return from cached results>
pages_from_proxy  - Pass
pages_from_server_after_cache_timeout - Pass < returned in .394 seconds again shorter than 5 but still longer than the instantaneous return from cached results>
prefetching       - FAIL
multithreading  - PASS
connection keepalive    -  FAIL

I also posted a link in my video to my program running with firefox

I could not get the test.py file to execute properly.  So I used apache benchmark to test my program. 
Also my VM does not have an eth0 or any eth at all only enp0s3 
and lo which when I tried editing with wondershaper cause me problems.  So I did not test with wondershaper.

 

video link of running program - https://youtu.be/Wvj4CKBDwco
my vm would not record audio so the video has no sound

Compile:
use command: make

Remove compiled executable:
use command: make clean

Run proxy:
./webproxy <port> <timeout (in seconds)>

My proxy uses threading to when a connection is accepted it spins off a thread to handle the request.  It first parses the pieces of the request then 
checks the cahe using the requested url.  If nothing is found it forwards the request to the server to fulfill.  Then send the server response to the client and adds
an entry to the cache for the url as well as a expiration time for the cached entry specified by the command line argument.
The web proxy servers as an middle man between a client and a server.  It can help improve performance on pages that are frequently visited by cacheing them.  
