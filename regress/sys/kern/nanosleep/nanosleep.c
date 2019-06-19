/*	$OpenBSD: nanosleep.c,v 1.7 2018/05/22 18:33:41 cheloha Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <err.h>
#include <signal.h>

int invalid_time(void);
int trivial(void);
int with_signal(void);
int time_elapsed(void);
int time_elapsed_with_signal(void);

int short_time(void);

void sighandler(int);

int
main(int argc, char **argv)
{
	int ch, ret;

	ret = 0;

	while ((ch = getopt(argc, argv, "itseES")) != -1) {
		switch (ch) {
		case 'i':
			ret |= invalid_time();
			break;
		case 't':
			ret |= trivial();
			break;
		case 's':
			ret |= with_signal();
			break;
		case 'e':
			ret |= time_elapsed();
			break;
		case 'E':
			ret |= time_elapsed_with_signal();
			break;
		case 'S':
			ret |= short_time();
		default:
			fprintf(stderr, "Usage: nanosleep [-itseSE]\n");
			exit(1);
		}
	}

	return (ret);
}

void
sighandler(int signum)
{
}

int
trivial(void)
{
	struct timespec ts, rts;

	ts.tv_sec = 0;
	ts.tv_nsec = 30000000;
	rts.tv_sec = 4711;	/* Just add to the confusion */
	rts.tv_nsec = 4711;
	if (nanosleep(&ts, &rts) < 0) {
		warn("trivial: nanosleep");
		return 1;
	}

	/*
	 * Just check that we don't get any leftover time if we sleep the
	 * amount of time we want to sleep.
	 * If we receive any signal, something is wrong anyway.
	 */
	if (rts.tv_sec != 0 || rts.tv_nsec != 0) {
		warnx("trivial: non-zero time? %lld/%ld", (long long)rts.tv_sec,
		    rts.tv_nsec);
		return 1;
	}

	return 0;
}

int
with_signal(void)
{
	struct timespec ts, rts;
	pid_t pid;
	int status;

	signal(SIGUSR1, sighandler);

	pid = getpid();

	switch(fork()) {
	case -1:
		err(1, "fork");
	default:
		ts.tv_sec = 1;
		ts.tv_nsec = 0;
		nanosleep(&ts, NULL);
		kill(pid, SIGUSR1);
		exit(0);
	}

	ts.tv_sec = 10;
	ts.tv_nsec = 0;
	rts.tv_sec = 0;
	rts.tv_nsec = 0;
	if (nanosleep(&ts, &rts) == 0) {
		warnx("with-signal: nanosleep");
		return 1;
	}
	if (rts.tv_sec == 0 && rts.tv_nsec == 0) {
		warnx("with-signal: zero time");
		return 1;
	}

	if (wait(&status) < 0)
		err(1, "wait");

	return 0;
}

int
time_elapsed(void)
{
	struct timespec ts;
	struct timeval stv, etv;

	ts.tv_sec = 0;
	ts.tv_nsec = 500000000;

	if (gettimeofday(&stv, NULL) < 0) {
		warn("gettimeofday");
		return 1;
	}

	if (nanosleep(&ts, NULL) < 0) {
		warn("nanosleep");
		return 1;
	}

	if (gettimeofday(&etv, NULL) < 0) {
		warn("gettimeofday");
		return 1;
	}

	timersub(&etv, &stv, &stv);

	if (stv.tv_sec == 0 && stv.tv_usec < 500000) {
		warnx("slept less than 0.5 sec");
		return 1;
	}

	return 0;
}

int
time_elapsed_with_signal(void)
{
	struct timespec ts, rts;
	struct timeval stv, etv;
	pid_t pid;
	int status;

	signal(SIGUSR1, sighandler);

	pid = getpid();

	switch(fork()) {
	case -1:
		err(1, "fork");
	default:
		ts.tv_sec = 1;
		ts.tv_nsec = 0;
		nanosleep(&ts, NULL);
		kill(pid, SIGUSR1);
		exit(0);
	}

	ts.tv_sec = 10;
	ts.tv_nsec = 0;
	rts.tv_sec = 0;
	rts.tv_nsec = 0;

	if (gettimeofday(&stv, NULL) < 0) {
		warn("gettimeofday");
		return 1;
	}

	if (nanosleep(&ts, &rts) == 0) {
		warnx("nanosleep");
		return 1;
	}

	if (gettimeofday(&etv, NULL) < 0) {
		warn("gettimeofday");
		return 1;
	}

	timersub(&etv, &stv, &stv);

	etv.tv_sec = rts.tv_sec;
	etv.tv_usec = rts.tv_nsec / 1000 + 1; /* the '+ 1' is a "roundup" */

	timeradd(&etv, &stv, &stv);

	if (stv.tv_sec < 10) {
		warnx("slept time + leftover time < 10 sec");
		return 1;
	}


	if (wait(&status) < 0)
		err(1, "wait");

	return 0;
}

int
short_time(void)
{
	struct timespec ts, rts;
	pid_t pid;
	int status;

	signal(SIGUSR1, sighandler);

	pid = getpid();

	switch(fork()) {
	case -1:
		err(1, "fork");
	default:
		/* Sleep two seconds, then shoot parent. */
		ts.tv_sec = 2;
		ts.tv_nsec = 0;
		nanosleep(&ts, NULL);
		kill(pid, SIGUSR1);
		exit(0);
	}

	ts.tv_sec = 0;
	ts.tv_nsec = 1;
	if (nanosleep(&ts, NULL) <= 0) {
		warn("short_time: nanosleep");
		return 1;
	}

	if (wait(&status) < 0)
		err(1, "wait");

	return 0;
}

int
invalid_time(void)
{
	struct timespec ts[3] = { {-1, 0}, {0, -1}, {0, 1000000000L} };
	int i, status;

	for (i = 0; i < 3; i++) {
		status = nanosleep(&ts[i], NULL);
		if (status != -1 || errno != EINVAL) {
			warnx("invalid-time: nanosleep %lld %ld",
			    (long long)ts[i].tv_sec, ts[i].tv_nsec);
			return 1;
		}
	}
	return 0;
}
