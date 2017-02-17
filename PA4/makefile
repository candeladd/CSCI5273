COMPILER = g++
COMPFLAGS = -std=c++11 -g -O0 
LFLAGS = -pthread -lcrypto

webproxy: webproxy.cpp
	$(COMPILER) $(COMPFLAGS) -o $@ $^ $(LFLAGS)

clean:
	rm -rf webproxy cache *.o *.a *.dSYM