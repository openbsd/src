#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

void *thread_function(void *arg); /* Pointer to function executed by each thread */

#define NUM 5

unsigned int args[NUM+1];

int main() {
    int res;
    pthread_t threads[NUM];
    void *thread_result;
    long i;

    for (i = 0; i < NUM; i++)
      {
	args[i] = 1;
	res = pthread_create(&threads[i],
		             NULL,
			     thread_function,
			     (void *) i);
      }

    /* schedlock.exp: last thread start.  */
    args[i] = 1;
    thread_function ((void *) i);

    exit(EXIT_SUCCESS);
}

void *thread_function(void *arg) {
    int my_number =  (long) arg;
    int *myp = &args[my_number];

    /* Don't run forever.  Run just short of it :)  */
    while (*myp > 0)
      {
	/* schedlock.exp: main loop.  */
	(*myp) ++;
      }

    pthread_exit(NULL);
}

