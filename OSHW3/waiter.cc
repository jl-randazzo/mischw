#include <assert.h>
#include <unistd.h>

int main()
{
	assert( sleep(4) == 0);
	return 0;
}
