#include <cassert>
#include <pthread.h>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

/*
** Compile and run this program, and make sure you get the 'aargh' error
** message. Fix it using a pthread mutex. The one command-line argument is
** the number of times to loop. Here are some suggested initial values, but
** you might have to tune them to your machine.
** Debian 8: 100000000
** Gouda: 10000000
** OS X: 100000
** You will need to compile your program with a "-lpthread" option.
*/

#define NUM_THREADS 2

int i;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *foo (void *bar)
{
	auto *me = new pthread_t(pthread_self());
	//std::unique_ptr<pthread_t> threadPtr(me);

	printf("in a foo thread, ID %ld\n", *me); 
	
	assert(pthread_mutex_lock(&lock) == 0);
	
	for (i = 0; i < *(static_cast<int *>(bar)); i++)
	{
		int tmp = i;

		if (tmp != i)
		{
			printf ("aargh: %d != %d\n", tmp, i);
		}
	}
	
	assert(pthread_mutex_unlock(&lock) == 0);
	pthread_exit (me);
}

int main(int argc, char **argv)
{
	int iterations = strtol(argv[1], NULL, 10);
	pthread_t threads[NUM_THREADS];
	for (int i = 0; i < NUM_THREADS; i++)
	{
		if (pthread_create(&threads[i], NULL, foo, static_cast<void *>(&iterations)) != 0)
		{
			perror ("pthread_create");
			return (1);
		}
	}

	for (int i = 0; i < NUM_THREADS; i++)
	{
		void *status;
		if (pthread_join (threads[i], &status) != 0)
		{
			perror ("pthread_join");
			return (1);
		}
		printf("joined a foo thread, number %ld\n", *(static_cast<pthread_t *>(status)));
		delete(static_cast<pthread_t *>(status));
	}

	return (0);
}
