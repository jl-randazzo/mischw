#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#define READ 0
#define WRITE 1
int child2parent;
int parent2child;

void waiter();

int main(int argc, char* argv[])
{

	child2parent = 4;
	parent2child = 3;

	//just a waiting mechanism
	waiter();

	char message [1024];
	message[0] = 4;
	char* words = "#4 print more words\n";
	int j = 1;
	for(char* i = words; *i != 0; i++)
	{
		message[j] = *i;
		message[++j] = 0;
	}
	assert(write(child2parent, message, 1024) >= 0); 
	assert(kill(getppid(), SIGTRAP) == 0);

	waiter();


	char buffer[1024];
	
	waiter();
	

	message[0] = 1;
	message[1] = 0;

	assert(write(child2parent, message, 1024) >= 0); 
	assert(kill(getppid(), SIGTRAP) == 0);
	//read is implemented to be blocking
	int readLength = read(parent2child, buffer, sizeof(buffer));
	assert(readLength >= 0);
	printf("%s\n", buffer);
	return 0;
}

void waiter()
{

	long sum = 1;
	for(long i = 499999999; i > 0; i--)
	{
		sum = sum + 1;
	}
}
