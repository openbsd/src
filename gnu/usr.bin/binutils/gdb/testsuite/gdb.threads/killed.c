#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>

int pid;

void *
child_func (void *dummy)
{
  kill (pid, SIGKILL);
  exit (1);
}

int
main ()
{
  pthread_t child;

  pid = getpid ();
  pthread_create (&child, 0, child_func, 0);
  for (;;)
    sleep (10000);
}
