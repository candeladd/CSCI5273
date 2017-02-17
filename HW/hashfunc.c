#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

int	H(int key)
{
	int x;
	x =	(key + 5) * (key - 3);
	x =	( x / 7 ) + key;
	x =	x % 13;
	return	x;
}

int main(int argc, char *argv[]) {

	int key = atoi(argv[1]);;
	int result = H(key);
	printf("%d\n", result);

}
