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
 * $OpenBSD: uthread_info.c,v 1.5 1999/01/06 05:29:24 d Exp $
 */
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

extern int _thread_sig_statistics[];

struct s_thread_info {
	enum pthread_state state;
	char           *name;
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
	{PS_SELECT_WAIT	, "select_wait"},
	{PS_SLEEP_WAIT	, "sleep_wait"},
	{PS_WAIT_WAIT	, "wait_wait"},
	{PS_SIGSUSPEND	, "sigsuspend"},
	{PS_SIGWAIT	, "sigwait"},
	{PS_JOIN	, "join"},
	{PS_SUSPENDED	, "suspended"},
	{PS_DEAD	, "dead"},
	{PS_STATE_MAX	, "xxx"}
};

const static char info_lead[] = "               -";

/* Determine a filename for display purposes: */
static const char *
truncname(const char *name, int maxlen)
{
	int len;

	if (name == NULL)
		name = "(null)";
	len = strlen(name);
	if (len > maxlen)
		return name + len - maxlen;
	else
		return name;
}

void
_thread_dump_info(void)
{
	char            s[512];
	int             fd;
	int             i;
	int             j;
	pthread_t       pthread;
	const char	*state;

	/* Open either the controlling tty or the dump file */

	fd = _thread_sys_open(_PATH_TTY, O_WRONLY | O_APPEND);
	if (fd < 0)
		fd = _thread_sys_open(INFO_DUMP_FILE, 
			O_WRONLY | O_APPEND | O_CREAT, 0666);
	if (fd < 0)
		return;

	/* Display a list of active threads */

	snprintf(s, sizeof s, " %8s%c%-11s %2s %4s %-8s %5s %5s %s\n", 
	    "id", ' ',  "state", "pr", "flag", "name", "utime", "stime",
	    "location");
	_thread_sys_write(fd, s, strlen(s));

	for (pthread = _thread_link_list; pthread != NULL;
	    pthread = pthread->nxt)
	{
		char location[30];

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
		    " %8p%c%-11s %2d %c%c%c%c %-8s %5.2f %5.2f %s\n",
		    (void *)pthread,
		    (pthread == _thread_run)     ? '*' : ' ',
		    state,
		    pthread->pthread_priority,
		    (pthread->attr.flags & PTHREAD_DETACHED)      ? 'D' : ' ',
		    (pthread->attr.flags & PTHREAD_SCOPE_SYSTEM)  ? 'S' : ' ',
		    (pthread->attr.flags & PTHREAD_INHERIT_SCHED) ? 'I' : ' ',
		    (pthread->attr.flags & PTHREAD_NOFLOAT)       ? 'F' : ' ',
		    (pthread->name == NULL) ? "" : pthread->name,
		    pthread->ru_utime.tv_sec + 
			(double)pthread->ru_utime.tv_usec / 1000000.0,
		    pthread->ru_stime.tv_sec + 
			(double)pthread->ru_stime.tv_usec / 1000000.0,
		    location
		);
		_thread_sys_write(fd, s, strlen(s));

		/* Process according to thread state: */
		switch (pthread->state) {
		/* File descriptor read lock wait: */
		case PS_FDLR_WAIT:
		case PS_FDLW_WAIT:
		case PS_FDR_WAIT:
		case PS_FDW_WAIT:
			/* Write the lock details: */
			snprintf(s, sizeof(s), "%s fd %d [%s:%d]\n",
			    info_lead,
			    pthread->data.fd.fd, 
			    truncname(pthread->data.fd.fname, 32),
			    pthread->data.fd.branch);
			_thread_sys_write(fd, s, strlen(s));
			s[0] = 0;
			snprintf(s, sizeof(s), "%s owner %pr/%pw\n",
			    info_lead,
			    _thread_fd_table[pthread->data.fd.fd]->r_owner,
			    _thread_fd_table[pthread->data.fd.fd]->w_owner);
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
				    (*pthread->data.mutex)->m_owner);
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
		case PS_JOIN:
			if (pthread->queue) {
				struct pthread *t;

				/* Locate the thread through its queue: */
				t = (struct pthread*)((
				    (char *)pthread->queue) -
				    offsetof(struct pthread, join_queue));
				snprintf(s, sizeof(s), 
				    "%s thread %p\n", info_lead, t);
				_thread_sys_write(fd, s, strlen(s));
			}
			break;
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
			/* XXX information in pthread->data.select_data */
			break;
		default:
			/* Nothing to do here. */
			break;
		}
	}

	/* Output a header for file descriptors: */
	snprintf(s, sizeof(s), "file descriptor table, size %d\n", 
	    _thread_dtablesize);
	_thread_sys_write(fd, s, strlen(s));

	snprintf(s, sizeof s,
		" %3s %8s %4s %21s %8s %4s %21s\n",
		"fd", "rdowner", "rcnt", "rdcode",
		"wrowner", "wcnt", "wrcode");
	_thread_sys_write(fd, s, strlen(s));

	/* Enter a loop to report file descriptor lock usage: */
	for (i = 0; i < _thread_dtablesize; i++) {
		/*
		 * Check if memory is allocated for this file
		 * descriptor: 
		 */
		char rcode[22], wcode[22];

		if (_thread_fd_table[i] != NULL) {

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

	/* Show signal counter statistics: */
	snprintf(s, sizeof s, "sig:");
	for (i = 0; i < NSIG; i++) {
		char buf[16] = " xx:xxxxx";
		if (_thread_sig_statistics[i]) {
			snprintf(buf, sizeof buf, " %d:%d", i, 
			    _thread_sig_statistics[i]);
			strlcat(s, buf, sizeof s);
		}
	}
	strlcat(s, "\n", sizeof s);
	_thread_sys_write(fd, s, strlen(s));

	/* Close the dump file: */
	_thread_sys_close(fd);
	return;
}

/* Set the thread name for debug: */
void
pthread_set_name_np(pthread_t thread, char *name)
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
