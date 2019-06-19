/* $OpenBSD: display.c,v 1.58 2018/11/28 22:00:30 kn Exp $	 */

/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS EMPLOYER BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  This file contains the routines that display information on the screen.
 *  Each section of the screen has two routines:  one for initially writing
 *  all constant and dynamic text, and one for only updating the text that
 *  changes.  The prefix "i_" is used on all the "initial" routines and the
 *  prefix "u_" is used for all the "updating" routines.
 *
 *  ASSUMPTIONS:
 *        None of the "i_" routines use any of the termcap capabilities.
 *        In this way, those routines can be safely used on terminals that
 *        have minimal (or nonexistent) terminal capabilities.
 *
 *        The routines are called in this order:  *_loadave, i_timeofday,
 *        *_procstates, *_cpustates, *_memory, *_message, *_header,
 *        *_process, u_endscreen.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/sched.h>
#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <err.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "screen.h"		/* interface to screen package */
#include "layout.h"		/* defines for screen position layout */
#include "display.h"
#include "top.h"
#include "boolean.h"
#include "machine.h"		/* we should eliminate this!!! */
#include "utils.h"

#ifdef DEBUG
FILE           *debug;
#endif

static int      display_width = MAX_COLS;

static char    *cpustates_tag(int);
static int      string_count(char **);
static void     summary_format(char *, size_t, int *, char **);
static int	readlinedumb(char *, int);

#define lineindex(l) ((l)*display_width)

/* things initialized by display_init and used throughout */

/* buffer of proc information lines for display updating */
char           *screenbuf = NULL;

static char   **procstate_names;
static char   **cpustate_names;
static char   **memory_names;

static int      num_cpustates;

static int     *cpustate_columns;
static int      cpustate_total_length;

/* display ips */
int y_mem;
int y_message;
int y_header;
int y_idlecursor;
int y_procs;
extern int ncpu;
extern int ncpuonline;
extern int combine_cpus;
extern struct process_select ps;

int header_status = Yes;

static int
empty(void)
{
	return OK;
}

static int
myfputs(const char *s)
{
	return fputs(s, stdout);
}

static int (*addstrp)(const char *);
static int (*printwp)(const char *, ...);
static int (*standoutp)(void);
static int (*standendp)(void);

int
display_resize(void)
{
	int cpu_lines, display_lines;

	ncpuonline = getncpuonline();
	cpu_lines = (combine_cpus ? 1 : ncpuonline);
	y_mem = 2 + cpu_lines;
	y_header = 4 + cpu_lines;
	y_procs = 5 + cpu_lines;

	/* calculate the current dimensions */
	/* if operating in "dumb" mode, we only need one line */
	display_lines = smart_terminal ? screen_length - y_procs : 1;

	y_idlecursor = y_message = 3 + (combine_cpus ? 1 : ncpuonline);
	if (screen_length <= y_message)
		y_idlecursor = y_message = screen_length - 1;

	/*
	 * we don't want more than MAX_COLS columns, since the
	 * machine-dependent modules make static allocations based on
	 * MAX_COLS and we don't want to run off the end of their buffers
	 */
	display_width = screen_width;
	if (display_width >= MAX_COLS)
		display_width = MAX_COLS - 1;

	if (display_lines < 0)
		display_lines = 0;

	/* return number of lines available */
	/* for dumb terminals, pretend like we can show any amount */
	return (smart_terminal ? display_lines : Largest);
}

int
display_init(struct statics * statics)
{
	int display_lines, *ip, i;
	char **pp;

	if (smart_terminal) {
		addstrp = addstr;
		printwp = printw;
		standoutp = standout;
		standendp = standend;
	} else {
		addstrp = myfputs;
		printwp = printf;
		standoutp = empty;
		standendp = empty;
	}

	/* call resize to do the dirty work */
	display_lines = display_resize();

	/* only do the rest if we need to */
	/* save pointers and allocate space for names */
	procstate_names = statics->procstate_names;

	cpustate_names = statics->cpustate_names;
	num_cpustates = string_count(cpustate_names);
	
	cpustate_columns = calloc(num_cpustates, sizeof(int));
	if (cpustate_columns == NULL)
		err(1, NULL);

	memory_names = statics->memory_names;

	/* calculate starting columns where needed */
	cpustate_total_length = 0;
	pp = cpustate_names;
	ip = cpustate_columns;
	while (*pp != NULL) {
		if ((i = strlen(*pp++)) > 0) {
			*ip++ = cpustate_total_length;
			cpustate_total_length += i + 8;
		}
	}

	/* return number of lines available */
	return (display_lines);
}

static void
format_uptime(char *buf, size_t buflen)
{
	time_t uptime;
	int days, hrs, mins;
	struct timespec boottime;

	/*
	 * Print how long system has been up.
	 */
	if (clock_gettime(CLOCK_BOOTTIME, &boottime) != -1) {
		uptime = boottime.tv_sec;
		uptime += 30;
		days = uptime / (3600 * 24);
		uptime %= (3600 * 24);
		hrs = uptime / 3600;
		uptime %= 3600;
		mins = uptime / 60;
		if (days > 0)
			snprintf(buf, buflen, "up %d day%s, %2d:%02d",
			    days, days > 1 ? "s" : "", hrs, mins);
		else
			snprintf(buf, buflen, "up %2d:%02d",
			    hrs, mins);
	}
}


void
i_loadave(pid_t mpid, double *avenrun)
{
	if (screen_length > 1 || !smart_terminal) {
		int i;

		move(0, 0);
		clrtoeol();

		addstrp("load averages");
		/* mpid == -1 implies this system doesn't have an _mpid */
		if (mpid != -1)
			printwp("last pid: %5ld;  ", (long) mpid);

		for (i = 0; i < 3; i++)
			printwp("%c %5.2f", i == 0 ? ':' : ',', avenrun[i]);
	}

}

/*
 *  Display the current time.
 *  "ctime" always returns a string that looks like this:
 *
 *	Sun Sep 16 01:03:52 1973
 *      012345678901234567890123
 *	          1         2
 *
 *  We want indices 11 thru 18 (length 8).
 */

void
i_timeofday(time_t * tod)
{
	static char buf[30];

	if (buf[0] == '\0')
		gethostname(buf, sizeof(buf));

	if (screen_length > 1 || !smart_terminal) {
		if (smart_terminal) {
			move(0, screen_width - 8 - strlen(buf) - 1);
		} else {
			if (fputs("    ", stdout) == EOF)
				exit(1);
		}
#ifdef DEBUG
		{
			char *foo;
			foo = ctime(tod);
			addstrp(foo);
		}
#endif
		printwp("%s %-8.8s", buf, &(ctime(tod)[11]));
		putn();
	}
}

/*
 *  *_procstates(total, states, threads) - print the process/thread summary line
 *
 *  Assumptions:  cursor is at the beginning of the line on entry
 */
void
i_procstates(int total, int *states, int threads)
{
	if (screen_length > 2 || !smart_terminal) {
		char procstates_buffer[MAX_COLS];
		char uptime[40];

		move(1, 0);
		clrtoeol();
		/* write current number of procs and remember the value */
		if (threads == Yes)
			printwp("%d threads: ", total);
		else
			printwp("%d processes: ", total);

		/* format and print the process state summary */
		summary_format(procstates_buffer, sizeof(procstates_buffer),
		    states, procstate_names);

		addstrp(procstates_buffer);

		format_uptime(uptime, sizeof(uptime));
		if (smart_terminal)
			move(1, screen_width - strlen(uptime));
		else
			printwp("  ");
		printwp("%s", uptime);
		putn();
	}
}

/*
 *  *_cpustates(states) - print the cpu state percentages
 *
 *  Assumptions:  cursor is on the PREVIOUS line
 */

/* cpustates_tag() calculates the correct tag to use to label the line */

static char *
cpustates_tag(int cpu)
{
	if (screen_length > 3 || !smart_terminal) {
		static char *tag;
		static int cpulen, old_width;
		int i;

		if (cpulen == 0 && ncpu > 1) {
			/* compute length of the cpu string */
			for (i = ncpu; i > 0; cpulen++, i /= 10)
				continue;
		}

		if (old_width == screen_width) {
			if (ncpu > 1) {
				/* just store the cpu number in the tag */
				i = tag[3 + cpulen];
				snprintf(tag + 3, cpulen + 1, "%.*d", cpulen, cpu);
				tag[3 + cpulen] = i;
			}
		} else {
			/*
			 * use a long tag if it will fit, otherwise use short one.
			 */
			free(tag);
			if (cpustate_total_length + 10 + cpulen >= screen_width)
				i = asprintf(&tag, "CPU%.*d: ", cpulen, cpu);
			else
				i = asprintf(&tag, "CPU%.*d states: ", cpulen, cpu);
			if (i == -1)
				tag = NULL;
			else
				old_width = screen_width;
		}
		return (tag);
	} else
		return ("\0");
}

void
i_cpustates(int64_t *ostates, int *online)
{
	int i, first, cpu, cpu_line;
	double value;
	int64_t *states;
	char **names, *thisname;

	if (combine_cpus) {
		static double *values;
		if (!values) {
			values = calloc(num_cpustates, sizeof(*values));
			if (!values)
				err(1, NULL);
		}
		memset(values, 0, num_cpustates * sizeof(*values));
		for (cpu = 0; cpu < ncpu; cpu++) {
			if (!online[cpu])
				continue;
			names = cpustate_names;
			states = ostates + (CPUSTATES * cpu);
			i = 0;
			while ((thisname = *names++) != NULL) {
				if (*thisname != '\0') {
					/* retrieve the value and remember it */
					values[i++] += *states++;
				}
			}
		}
		if (screen_length > 2 || !smart_terminal) {
			names = cpustate_names;
			i = 0;
			first = 0;
			move(2, 0);
			clrtoeol();
			printwp("%-3d CPUs: ", ncpuonline);

			while ((thisname = *names++) != NULL) {
				if (*thisname != '\0') {
					value = values[i++] / ncpuonline;
					/* if percentage is >= 1000, print it as 100% */
					printwp((value >= 1000 ? "%s%4.0f%% %s" :
					    "%s%4.1f%% %s"), first++ == 0 ? "" : ", ",
					    value / 10., thisname);
				}
			}
			putn();
		}
		return;
	}
	for (cpu = cpu_line = 0; cpu < ncpu; cpu++) {
		/* skip if offline */
		if (!online[cpu])
			continue;

		/* now walk thru the names and print the line */
		names = cpustate_names;
		first = 0;
		states = ostates + (CPUSTATES * cpu);

		if (screen_length > 2 + cpu_line || !smart_terminal) {
			move(2 + cpu_line, 0);
			clrtoeol();
			addstrp(cpustates_tag(cpu));

			while ((thisname = *names++) != NULL) {
				if (*thisname != '\0') {
					/* retrieve the value and remember it */
					value = *states++;

					/* if percentage is >= 1000, print it as 100% */
					printwp((value >= 1000 ? "%s%4.0f%% %s" :
					    "%s%4.1f%% %s"), first++ == 0 ? "" : ", ",
					    value / 10., thisname);
				}
			}
			putn();
			cpu_line++;
		}
	}
}

/*
 *  *_memory(stats) - print "Memory: " followed by the memory summary string
 */
void
i_memory(int *stats)
{
	if (screen_length > y_mem || !smart_terminal) {
		char memory_buffer[MAX_COLS];

		move(y_mem, 0);
		clrtoeol();
		addstrp("Memory: ");

		/* format and print the memory summary */
		summary_format(memory_buffer, sizeof(memory_buffer), stats,
		    memory_names);
		addstrp(memory_buffer);
		putn();
	}
}

/*
 *  *_message() - print the next pending message line, or erase the one
 *                that is there.
 */

/*
 *  i_message is funny because it gets its message asynchronously (with
 *	respect to screen updates).
 */

static char     next_msg[MAX_COLS + 5];
static int      msgon = 0;

void
i_message(void)
{
	move(y_message, 0);
	if (next_msg[0] != '\0') {
		standoutp();
		addstrp(next_msg);
		standendp();
		clrtoeol();
		msgon = TRUE;
		next_msg[0] = '\0';
	} else if (msgon) {
		clrtoeol();
		msgon = FALSE;
	}
}

/*
 *  *_header(text) - print the header for the process area
 */

void
i_header(char *text)
{
	if (header_status == Yes && (screen_length > y_header
              || !smart_terminal)) {
		if (!smart_terminal) {
			putn();
			if (fputs(text, stdout) == EOF)
				exit(1);
			putn();
		} else {
			move(y_header, 0);
			clrtoeol();
			addstrp(text);
		}
	}
}

/*
 *  *_process(line, thisline) - print one process line
 */

void
i_process(int line, char *thisline, int hl)
{
	/* make sure we are on the correct line */
	move(y_procs + line, 0);

	/* truncate the line to conform to our current screen width */
	thisline[display_width] = '\0';

	/* write the line out */
	if (hl && smart_terminal)
		standoutp();
	addstrp(thisline);
	if (hl && smart_terminal)
		standendp();
	putn();
	clrtoeol();
}

void
u_endscreen(void)
{
	if (smart_terminal) {
		clrtobot();
		/* move the cursor to a pleasant place */
		move(y_idlecursor, x_idlecursor);
	} else {
		/*
		 * separate this display from the next with some vertical
		 * room
		 */
		if (fputs("\n\n", stdout) == EOF)
			exit(1);
	}
}

void
display_header(int status)
{
	header_status = status;
}

void
new_message(int type, const char *msgfmt,...)
{
	va_list ap;

	va_start(ap, msgfmt);
	/* first, format the message */
	vsnprintf(next_msg, sizeof(next_msg), msgfmt, ap);
	va_end(ap);

	if (next_msg[0] != '\0') {
		/* message there already -- can we clear it? */
		/* yes -- write it and clear to end */
		if ((type & MT_delayed) == 0) {
			move(y_message, 0);
			if (type & MT_standout)
				standoutp();
			addstrp(next_msg);
			if (type & MT_standout)
				standendp();
			clrtoeol();
			msgon = TRUE;
			next_msg[0] = '\0';
			if (smart_terminal)
				refresh();
		}
	}
}

void
clear_message(void)
{
	move(y_message, 0);
	clrtoeol();
}


static int
readlinedumb(char *buffer, int size)
{
	char *ptr = buffer, ch, cnt = 0, maxcnt = 0;
	extern volatile sig_atomic_t leaveflag;
	ssize_t len;

	/* allow room for null terminator */
	size -= 1;

	/* read loop */
	while ((fflush(stdout), (len = read(STDIN_FILENO, ptr, 1)) > 0)) {

		if (len == 0 || leaveflag) {
			end_screen();
			exit(0);
		}

		/* newline means we are done */
		if ((ch = *ptr) == '\n')
			break;

		/* handle special editing characters */
		if (ch == ch_kill) {
			/* return null string */
			*buffer = '\0';
			putr();
			return (-1);
		} else if (ch == ch_erase) {
			/* erase previous character */
			if (cnt <= 0) {
				/* none to erase! */
				if (putchar('\7') == EOF)
					exit(1);
			} else {
				if (fputs("\b \b", stdout) == EOF)
					exit(1);
				ptr--;
				cnt--;
			}
		}
		/* check for character validity and buffer overflow */
		else if (cnt == size || !isprint((unsigned char)ch)) {
			/* not legal */
			if (putchar('\7') == EOF)
				exit(1);
		} else {
			/* echo it and store it in the buffer */
			if (putchar(ch) == EOF)
				exit(1);
			ptr++;
			cnt++;
			if (cnt > maxcnt)
				maxcnt = cnt;
		}
	}

	/* all done -- null terminate the string */
	*ptr = '\0';

	/* return either inputted number or string length */
	putr();
	return (cnt == 0 ? -1 : cnt);
}

int
readline(char *buffer, int size)
{
	size_t cnt;

	/* allow room for null terminator */
	size -= 1;

	if (smart_terminal) {
		int y, x;
		getyx(stdscr, y, x);
		while (getnstr(buffer, size) == KEY_RESIZE)
			move(y, x);
	} else
		return readlinedumb(buffer, size);

	cnt = strlen(buffer);
	if (cnt > 0 && buffer[cnt - 1] == '\n')
		buffer[cnt - 1] = '\0';
	return (cnt == 0 ? -1 : cnt);
}

/* internal support routines */
static int
string_count(char **pp)
{
	int cnt;

	cnt = 0;
	while (*pp++ != NULL)
		cnt++;
	return (cnt);
}

#define	COPYLEFT(to, from)				\
	do {						\
		len = strlcpy((to), (from), left);	\
		if (len >= left)			\
			return;				\
		p += len;				\
		left -= len;				\
	} while (0)

static void
summary_format(char *buf, size_t left, int *numbers, char **names)
{
	char *p, *thisname;
	size_t len;
	int num;

	/* format each number followed by its string */
	p = buf;
	while ((thisname = *names++) != NULL) {
		/* get the number to format */
		num = *numbers++;

		if (num >= 0) {
			/* is this number in kilobytes? */
			if (thisname[0] == 'K') {
				/* yes: format it as a memory value */
				COPYLEFT(p, format_k(num));

				/*
				 * skip over the K, since it was included by
				 * format_k
				 */
				COPYLEFT(p, thisname + 1);
			} else if (num > 0) {
				len = snprintf(p, left, "%d%s", num, thisname);
				if (len == (size_t)-1 || len >= left)
					return;
				p += len;
				left -= len;
			}
		} else {
			/*
			 * Ignore negative numbers, but display corresponding
			 * string.
			 */
			COPYLEFT(p, thisname);
		}
	}

	/* if the last two characters in the string are ", ", delete them */
	p -= 2;
	if (p >= buf && p[0] == ',' && p[1] == ' ')
		*p = '\0';
}

/*
 *  printable(str) - make the string pointed to by "str" into one that is
 *	printable (i.e.: all ascii), by converting all non-printable
 *	characters into '?'.  Replacements are done in place and a pointer
 *	to the original buffer is returned.
 */
char *
printable(char *str)
{
	char *ptr, ch;

	ptr = str;
	while ((ch = *ptr) != '\0') {
		if (!isprint((unsigned char)ch))
			*ptr = '?';
		ptr++;
	}
	return (str);
}


/*
 *  show_help() - display the help screen; invoked in response to
 *		either 'h' or '?'.
 */
void
show_help(void)
{
	if (smart_terminal) {
		clear();
		nl();
	}
	printwp("These single-character commands are available:\n"
	    "\n"
	    "^L           - redraw screen\n"
	    "<space>      - update screen\n"
	    "+            - reset any g, p, or u filters\n"
	    "1            - display CPU statistics on a single line\n"
	    "C            - toggle the display of command line arguments\n"
	    "d count      - show `count' displays, then exit\n"
	    "e            - list errors generated by last \"kill\" or \"renice\" command\n"
	    "g string     - filter on command name (g+ selects all commands)\n"
	    "h | ?        - help; show this text\n"
	    "H            - toggle the display of threads\n"
	    "I | i        - toggle the display of idle processes\n"
	    "k [-sig] pid - send signal `-sig' to process `pid'\n"
	    "n|# count    - show `count' processes\n"
	    "o [-]field   - specify sort order (size, res, cpu, time, pri, pid, command)\n"
	    "               (o -field sorts in reverse)\n"
	    "P pid        - highlight process `pid' (P+ switches highlighting off)\n"
	    "p pid        - display process by `pid' (p+ selects all processes)\n"
	    "q            - quit\n"
	    "r count pid  - renice process `pid' to nice value `count'\n"
	    "S            - toggle the display of system processes\n"
	    "s time       - change delay between displays to `time' seconds\n"
	    "u [-]user    - show processes for `user' (u+ shows all, u -user hides user)\n"
	    "\n");

	if (smart_terminal) {
		nonl();
		refresh();
	}
}

/*
 *  show_errors() - display on stdout the current log of errors.
 */
void
show_errors(void)
{
	struct errs *errp = errs;
	int cnt = 0;

	if (smart_terminal) {
		clear();
		nl();
	}
	printwp("%d error%s:\n\n", errcnt, errcnt == 1 ? "" : "s");
	while (cnt++ < errcnt) {
		printwp("%5s: %s\n", errp->arg,
		    errp->err == 0 ? "Not a number" : strerror(errp->err));
		errp++;
	}
	printwp("\n");
	if (smart_terminal) {
		nonl();
		refresh();
	}
}

void
anykey(void)
{
	int ch;
	ssize_t len;

	standoutp();
	addstrp("Hit any key to continue: ");
	standendp();
	if (smart_terminal)
		refresh();
	else
		fflush(stdout);
	while (1) {
		len = read(STDIN_FILENO, &ch, 1);
		if (len == -1 && errno == EINTR)
			continue;
		if (len == 0)
			exit(1);
		break;
	}
}
