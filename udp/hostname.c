#include <unistd.h>
#include <stdio.h>

int main(void)
{
	int hostip;
	char* name;
	size_t len;
	hostip= gethostname(name, len); 
	
}
