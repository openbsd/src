#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include "test.h"
#define NLOOPS 1000

int ntouts = 0;

void *
bg_routine(arg)
	void *arg;
{
  char dot = '.';
  pthread_set_name_np(pthread_self(), "bg");
  write(STDOUT_FILENO,"bg routine running\n",19);
  /*pthread_dump_state();*/
  while (1) {
    int n;
    pthread_yield();
    write(STDOUT_FILENO, &dot, sizeof dot);
    pthread_yield();
    n = NLOOPS;
    while (n-- > 0)
      pthread_yield();
  }
}

void *
fg_routine(arg)
	void *arg;
{
  int flags;
  /* static struct timeval tout = { 0, 500000 }; */
  int n;
  fd_set r;

  pthread_set_name_np(pthread_self(), "fg");

  flags = fcntl(STDIN_FILENO, F_GETFL);
  CHECKr(fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK));

  while (1) {
    int maxfd;

    FD_ZERO(&r);
    FD_SET(STDIN_FILENO,&r);
    maxfd = STDIN_FILENO;

    errno = 0;
    printf("select>");
    CHECKe(n = select(maxfd+1, &r, (fd_set*)0, (fd_set*)0,
	(struct timeval *)0));

    if (n > 0) {
      int nb;
      char buf[128];

      printf("=> select returned: %d\n", n);
      while ((nb = read(STDIN_FILENO, buf, sizeof(buf)-1)) >= 0) {
	buf[nb] = '\0';
	printf("read %d: |%s|\n", nb, buf);
      }
      printf("=> out of read loop: len = %d / %s\n", nb, strerror(errno));
      if (nb < 0)
	ASSERTe(errno, == EWOULDBLOCK || errno == EAGAIN);
    } else
      ntouts++;
  }
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	pthread_t bg_thread, fg_thread;
	int junk;

	setbuf(stdout,NULL);
	setbuf(stderr,NULL);

	CHECKr(pthread_create(&bg_thread, NULL, bg_routine, 0));
	CHECKr(pthread_create(&fg_thread, NULL, fg_routine, 0));

	printf("threads forked: bg=%p fg=%p\n", bg_thread, fg_thread);
	/*pthread_dump_state();*/
	/*
	printf("initial thread %p joining fg...\n", pthread_self());
	CHECKr(pthread_join(fg_thread, (void **)&junk));
	*/
	sleep(20);
	SUCCEED;
}
