/*	$OpenBSD: uthread_info_openbsd.c,v 1.16 2011/09/13 23:56:00 fgsch Exp $	*/

/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <paths.h>
#include <sys/poll.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int _thread_dump_verbose = 0;

struct s_thread_info {
	enum pthread_state state;
	const char         *name;
};

/* Static variables: */
static const struct s_thread_info thread_info[] = {
	{PS_RUNNING	, "running"},
	{PS_SIGTHREAD	, "sigthread"},
	{PS_MUTEX_WAIT	, "mutex_wait"},
	{PS_COND_WAIT	, "cond_wait"},
	{PS_FDLR_WAIT	, "fdlr_wait"},
	{PS_FDLW_WAIT	, "fdlw_wait"},
	{PS_FDR_WAIT	, "fdr_wait"},
	{PS_FDW_WAIT	, "fdw_wait"},
	{PS_FILE_WAIT	, "file_wait"},
	{PS_KEVENT_WAIT	, "kevent_wait"},
	{PS_POLL_WAIT	, "poll_wait"},
	{PS_SELECT_WAIT	, "select_wait"},
	{PS_SLEEP_WAIT	, "sleep_wait"},
	{PS_WAIT_WAIT	, "wait_wait"},
	{PS_SIGSUSPEND	, "sigsuspend"},
	{PS_SIGWAIT	, "sigwait"},
	{PS_SPINBLOCK	, "spinblock"},
	{PS_JOIN	, "join"},
	{PS_SUSPENDED	, "suspended"},
	{PS_DEAD	, "dead"},
	{PS_DEADLOCK	, "deadlock"},
	{(enum pthread_state)0, "<invalid>"},
};

#define writestring(fd, s)	_thread_sys_write(fd, s, (sizeof s) - 1)

const static char info_lead[] = "               -";

/* Determine a filename for display purposes: */
static const char *
truncname(const char *name, int maxlen)
{
	int len;

	if (name == NULL)
		name = "(null)";
	len = (int)strlen(name);
	if (len > maxlen)
		return name + len - maxlen;
	else
		return name;
}

static void
_thread_dump_entry(pthread_t pthread, int fd, int verbose)
{
	const char	*state;
	char		s[512];
	char		location[30];
	unsigned int	j;

	/* Find last known file:line checkpoint: */
	if (pthread->fname && pthread->state != PS_RUNNING)
		snprintf(location, sizeof location, "%s:%d",
		    truncname(pthread->fname, 21), pthread->lineno);
	else
		location[0] = '\0';

	/* Find the state: */
	for (j = 0; j < (sizeof(thread_info) /
	    sizeof(struct s_thread_info)) - 1; j++)
		if (thread_info[j].state == pthread->state)
			break;
	state = thread_info[j].name;

	/* Output a record for the current thread: */
	s[0] = 0;
	snprintf(s, sizeof(s), 
	    " %8p%c%-11s %2d %c%c%c%c%c%c%c%c%c%c %04x %-8.8s %s\n",
	    (void *)pthread,
	    (pthread == _thread_run)     ? '*' : ' ',
	    state,
	    pthread->active_priority,
	    (pthread->flags & PTHREAD_FLAGS_PRIVATE)	  ? 'p' : '-',
	    (pthread->flags & PTHREAD_EXITING)		  ? 'E' :
	    (pthread->cancelflags & PTHREAD_AT_CANCEL_POINT) ? 'c' : '-',
	    (pthread->flags & PTHREAD_FLAGS_TRACE)	  ? 't' : '-',
	    (pthread->flags & PTHREAD_FLAGS_IN_CONDQ)     ? 'C' : '-',
	    (pthread->flags & PTHREAD_FLAGS_IN_WORKQ)     ? 'R' : '-',
	    (pthread->flags & PTHREAD_FLAGS_IN_WAITQ)     ? 'W' : '-',
	    (pthread->flags & PTHREAD_FLAGS_IN_PRIOQ)     ? 'P' : '-',
	    (pthread->attr.flags & PTHREAD_DETACHED)      ? 'd' : '-',
	    (pthread->attr.flags & PTHREAD_INHERIT_SCHED) ? 'i' : '-',
	    (pthread->attr.flags & PTHREAD_NOFLOAT)       ? '-' : 'f',
	    ((unsigned int)pthread->sigmask & 0xffff),
	    (pthread->name == NULL) ? "" : pthread->name,
	    (verbose) ? location : ""
	);
	_thread_sys_write(fd, s, strlen(s));

	if (!verbose)
		return;

	/* Show the scheduler wake and time slice properties */
	snprintf(s, sizeof(s), "%s sched wake ", info_lead);
	_thread_sys_write(fd, s, strlen(s));
	if (pthread->wakeup_time.tv_sec == -1)
		writestring(fd, "- slice ");
	else {
		struct timeval now;
		struct timespec nows, delta;

		gettimeofday(&now, NULL);
		TIMEVAL_TO_TIMESPEC(&now, &nows);
		timespecsub(&pthread->wakeup_time, &nows, &delta);
		snprintf(s, sizeof s, "%d.%09ld slice ",
			delta.tv_sec, delta.tv_nsec);
		_thread_sys_write(fd, s, strlen(s));
	}
	if (pthread->slice_usec == -1)
		writestring(fd, "-\n");
	else {
		snprintf(s, sizeof s, "%ld.%06ld\n",
			pthread->slice_usec / 1000000,
			pthread->slice_usec % 1000000);
		_thread_sys_write(fd, s, strlen(s));
	}

	/* Process according to thread state: */
	switch (pthread->state) {
	/* File descriptor read lock wait: */
	case PS_FDLR_WAIT:
	case PS_FDLW_WAIT:
	case PS_FDR_WAIT:
	case PS_FDW_WAIT:
	case PS_KEVENT_WAIT:
		/* Write the lock details: */
		snprintf(s, sizeof(s), "%s fd %d [%s:%d]\n",
		    info_lead,
		    pthread->data.fd.fd, 
		    truncname(pthread->data.fd.fname, 32),
		    pthread->data.fd.branch);
		_thread_sys_write(fd, s, strlen(s));
		s[0] = 0;
		if (_thread_fd_table[pthread->data.fd.fd] &&
		    _thread_fd_table[pthread->data.fd.fd]->state != FD_ENTRY_CLOSED)
		    snprintf(s, sizeof(s), "%s owner %pr/%pw\n",
			     info_lead,
			     _thread_fd_table[pthread->data.fd.fd]->r_owner,
			     _thread_fd_table[pthread->data.fd.fd]->w_owner);
		else
		    snprintf(s, sizeof(s), "%s owner [unknown]\n", info_lead);
		_thread_sys_write(fd, s, strlen(s));
		break;
	case PS_SIGWAIT:
		snprintf(s, sizeof(s), "%s sigmask 0x%08lx\n",
		    info_lead,
		    (unsigned long)pthread->sigmask);
		_thread_sys_write(fd, s, strlen(s));
		break;
	case PS_MUTEX_WAIT:
		snprintf(s, sizeof(s), 
		    "%s mutex %p\n",
		    info_lead,
		    pthread->data.mutex);
		_thread_sys_write(fd, s, strlen(s));
		if (pthread->data.mutex) {
			snprintf(s, sizeof(s), 
			    "%s owner %p\n",
			    info_lead,
			    pthread->data.mutex->m_owner);
			_thread_sys_write(fd, s, strlen(s));
		}
		break;
	case PS_COND_WAIT:
		snprintf(s, sizeof(s), 
		    "%s cond %p\n",
		    info_lead,
		    pthread->data.cond);
		_thread_sys_write(fd, s, strlen(s));
		break;
#ifdef notyet
	case PS_JOIN:
		{
			struct pthread *t, * volatile *last;
			pthread_entry_t *e;

			/* Find the end of the list: */
			for (e = &pthread->qe; e->tqe_next != NULL;
			    e = &e->tqe_next->qe)
				;
			last = &e->tqe_next;
			/* Walk backwards to the head: */
			for (e = &pthread->qe; 
			    ((_thread_list_t *)e)->tqh_last != last; 
			    e = (pthread_entry_t *)e->tqe_prev)
				;
			/* Convert the head address into a thread address: */
			t = (pthread_t)((caddr_t)e - 
			    offsetof(struct pthread, join_queue));
			snprintf(s, sizeof(s), 
			    "%s thread %p\n", info_lead, t);
			_thread_sys_write(fd, s, strlen(s));
		}
		break;
#endif
	case PS_SLEEP_WAIT:
		{
			struct timeval tv;
			struct timespec current_time;
			struct timespec remaining_time;
			double remain;

			gettimeofday(&tv, NULL);
			TIMEVAL_TO_TIMESPEC(&tv, &current_time);
			timespecsub(&pthread->wakeup_time, 
			    &current_time, &remaining_time);
			remain = remaining_time.tv_sec
				+ (double)remaining_time.tv_nsec / 1e9;
			snprintf(s, sizeof(s), 
			    "%s wake in %f sec\n", 
			    info_lead, remain);
			_thread_sys_write(fd, s, strlen(s));
		}
		break;
	case PS_SELECT_WAIT:
	case PS_POLL_WAIT:
		{
			nfds_t i;

			for (i = 0; i < pthread->data.poll_data->nfds; i++) 
				snprintf(s, sizeof(s), "%s%d:%s%s",
				    i ? " " : "",
				    pthread->data.poll_data->fds[i].fd,
				    pthread->data.poll_data->fds[i].events &
					POLLIN ? "r" : "",
				    pthread->data.poll_data->fds[i].events &
					POLLOUT ? "w" : ""
				);
			snprintf(s, sizeof(s), "\n");
		}
		break;
	default:
	/* Nothing to do here. */
		break;
	}
}

void
_thread_dump_info(void)
{
	char            s[512];
	int             fd;
	int             i;
	pthread_t       pthread;
	pq_list_t *	pq_list;
	int		verbose;

	verbose = _thread_dump_verbose;
	if (getenv("PTHREAD_DEBUG") != NULL)
		verbose = 1;

	/* Open the controlling tty: */
	fd = _thread_sys_open(_PATH_TTY, O_WRONLY | O_APPEND);
	if (fd < 0)
		return;

	if (!verbose)  {
		/* Display only a very brief list of threads */
		TAILQ_FOREACH(pthread, &_thread_list, tle)
			if ((pthread->flags & PTHREAD_FLAGS_PRIVATE) == 0)
				_thread_dump_entry(pthread, fd, 0);
		return;
	}

	/* Display a list of active threads: */
	snprintf(s, sizeof s, " %8s%c%-11s %2s %-10s %4s %-8s %s\n", 
	    "id", ' ',  "state", "pr", "flags", "mask", "name", "location");
	_thread_sys_write(fd, s, strlen(s));

	writestring(fd, "active:\n");

	TAILQ_FOREACH(pthread, &_thread_list, tle)
		_thread_dump_entry(pthread, fd, 1);

	writestring(fd, "ready:\n");
	TAILQ_FOREACH (pq_list, &_readyq.pq_queue, pl_link)
		TAILQ_FOREACH(pthread, &pq_list->pl_head, pqe)
			_thread_dump_entry(pthread, fd, 0);

	writestring(fd, "waiting:\n");
	TAILQ_FOREACH (pthread, &_waitingq, pqe) 
		_thread_dump_entry(pthread, fd, 0);

	writestring(fd, "workq:\n");
	TAILQ_FOREACH (pthread, &_workq, qe)
		_thread_dump_entry(pthread, fd, 0);

	writestring(fd, "dead:\n");
	TAILQ_FOREACH(pthread, &_dead_list, dle)
		_thread_dump_entry(pthread, fd, 1);

	/* Output a header for file descriptors: */
	snprintf(s, sizeof(s), "file descriptor table, size %d:\n", 
	    _thread_max_fdtsize);
	_thread_sys_write(fd, s, strlen(s));

	snprintf(s, sizeof s,
		" %3s %8s %4s %21s %8s %4s %21s\n",
		"fd", "rdowner", "rcnt", "rdcode",
		"wrowner", "wcnt", "wrcode");
	_thread_sys_write(fd, s, strlen(s));

	/* Enter a loop to report file descriptor lock usage: */
	for (i = 0; i < _thread_max_fdtsize; i++) {
		/*
		 * Check if memory is allocated for this file
		 * descriptor: 
		 */
		char rcode[22], wcode[22];

		if (_thread_fd_table[i] != NULL &&
		    _thread_fd_table[i]->state != FD_ENTRY_CLOSED) {

			/* Find the reader's last file:line: */
			if (_thread_fd_table[i]->r_owner)
				snprintf(rcode, sizeof rcode, "%s:%d", 
				    truncname(_thread_fd_table[i]->r_fname, 16),
				    _thread_fd_table[i]->r_lineno);
			else
				rcode[0] = '\0';

			/* Find the writer's last file:line: */
			if (_thread_fd_table[i]->w_owner)
				snprintf(wcode, sizeof wcode, "%s:%d", 
				    truncname(_thread_fd_table[i]->w_fname, 16),
				    _thread_fd_table[i]->w_lineno);
			else
				wcode[0] = '\0';

			/* Report the file descriptor lock status: */
			snprintf(s, sizeof(s),
			    " %3d %8p %4d %21s %8p %4d %21s\n",
			    i, 
			    _thread_fd_table[i]->r_owner,
			    _thread_fd_table[i]->r_lockcount,
			    rcode,
			    _thread_fd_table[i]->w_owner,
			    _thread_fd_table[i]->w_lockcount,
			    wcode
			);
			_thread_sys_write(fd, s, strlen(s));
		}
	}

	/* Close the dump file: */
	_thread_sys_close(fd);
	return;
}

/*
 * Generic "dump some data to /dev/tty in hex format" function
 * output format is:
 *      0                     22                        48
 *	0x0123456789abcdef:   00 11 22 33 44 55 66 77   01234567
 */
#define DUMP_BUFLEN 84
#define DUMP_HEX_OFF 22
#define DUMP_ASCII_OFF 48

void
_thread_dump_data(const void *addr, int len)
{
	int fd = -1;
	unsigned char data[DUMP_BUFLEN];
	const unsigned char hexdigits[] = "0123456789abcdef";

	if (getenv("PTHREAD_DEBUG") != NULL)
		fd = _thread_sys_open(_PATH_TTY, O_WRONLY | O_APPEND);
	if (fd != -1) {
		memset(data, ' ', DUMP_BUFLEN);
		while (len) {
			const unsigned char *d;
			unsigned char *h;
			unsigned char *a;
			int count;

			d = addr;
			h = &data[DUMP_HEX_OFF];
			a = &data[DUMP_ASCII_OFF];

			if (len > 8) {
				count = 8;
				len -= 8;
			} else {
				count = len;
				len = 0;
				memset(data, ' ', DUMP_BUFLEN);
			}
			addr = (char *)addr + 8;

			snprintf((char *)data, DUMP_BUFLEN, "%18p:   ", d);
			while (count--) {
				if (isprint(*d))
					*a++ = *d;
				else
					*a++ = '.';
				*h++ = hexdigits[(*d >> 4) & 0xf];
				*h++ = hexdigits[*d++ & 0xf];
				*h++ = ' ';
			}
			*a++ = '\n';
			*a = 0;
			_thread_sys_write(fd, data, (size_t)(a - data));
		}
		writestring(fd, "\n");
		_thread_sys_close(fd);
	}
}

/* Set the thread name for debug: */
void
pthread_set_name_np(pthread_t thread, const char *name)
{
	/* Check if the caller has specified a valid thread: */
	if (thread != NULL && thread->magic == PTHREAD_MAGIC) {
		if (thread->name != NULL)
			free(thread->name);
		thread->name = strdup(name);
	}
	return;
}
#endif
