/* $OpenBSD: rthread_debug.c,v 1.1 2005/12/31 21:40:43 marc Exp $ */
/* $snafu: rthread_debug.c,v 1.2 2004/12/09 18:41:44 marc Exp $ */

/* PUBLIC DOMAIN: No Rights Reserved. Marco S Hyman <marc@snafu.org> */

#define _POSIX_THREADS

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <machine/spinlock.h>

#include <pthread.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "rthread.h"

int _rthread_debug_level;

/*
 * Note: messages truncated at 255 characters.   Could use vasprintf,
 * but don't want to use malloc here so the function can be used
 * in signal handlers.
 */
#define MAX_MSG_LEN 256
#define RTHREAD_ENV_DEBUG	"RTHREAD_DEBUG"

/*
 * format and send output to stderr if the given "level" is less than or
 * equal to the current debug level.   Messages with a level <= 0 will
 * always be printed.
 */
void
_rthread_debug(int level, const char *fmt, ...)
{
	char msg[MAX_MSG_LEN];
	char *p;
	int cnt;
	ssize_t c;

	if (_rthread_debug_level >= level) {
		va_list ap;
		va_start(ap, fmt);
		cnt = vsnprintf(msg, MAX_MSG_LEN, fmt, ap);
		va_end(ap);
		if (cnt > MAX_MSG_LEN - 1)
			cnt = MAX_MSG_LEN - 1;
		p = msg;
		do {
			c = write(STDERR_FILENO, p, cnt);
			if (c == -1)
				break;
			if (c != cnt)
				sched_yield();
			p += c;
			cnt -= c;
		} while (cnt > 0);
	}
}

/*
 * set the debug level from an environment string.  Bogus values are
 * silently ignored.
 */
void
_rthread_debug_init(void)
{
	char *envp;
	char *rem;

	envp = getenv(RTHREAD_ENV_DEBUG);
	if (envp) {
		_rthread_debug_level = (int) strtol(envp, &rem, 0);
		if (*rem || _rthread_debug_level < 0)
			_rthread_debug_level = 0;
	}
}
