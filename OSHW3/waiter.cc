#include<iostream>
#include <assert.h>
#include <unistd.h>

int main()
{
	long sum = 1;
	for(long i = 999999999; i > 0; i--)
	{
		sum = sum + 1;
	}
	std::cout << sum << std::endl;
	return 0;
}
