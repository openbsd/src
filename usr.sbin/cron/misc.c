/*	$OpenBSD: misc.c,v 1.11 2001/12/12 19:02:50 millert Exp $	*/
/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * All rights reserved
 */

/*
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if !defined(lint) && !defined(LINT)
static char rcsid[] = "$OpenBSD: misc.c,v 1.11 2001/12/12 19:02:50 millert Exp $";
#endif

/* vix 26jan87 [RCS has the rest of the log]
 * vix 30dec86 [written]
 */


#include "cron.h"
#if SYS_TIME_H
# include <sys/time.h>
#else
# include <time.h>
#endif
#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#if defined(SYSLOG)
# include <syslog.h>
#endif


#if defined(LOG_CRON) && defined(LOG_FILE)
# undef LOG_FILE
#endif

#if defined(LOG_DAEMON) && !defined(LOG_CRON)
# define LOG_CRON LOG_DAEMON
#endif


static int		LogFD = ERR;

/*
 * glue_strings is the overflow-safe equivalent of
 *		sprintf(buffer, "%s%c%s", a, separator, b);
 *
 * returns 1 on success, 0 on failure.  'buffer' MUST NOT be used if
 * glue_strings fails.
 */
int
glue_strings(buffer, buffer_size, a, b, separator)
	char	*buffer;
	int	buffer_size;	
	char	*a;
	char	*b;
	int	separator;
{
	char *buf;
	char *buf_end;

	if (buffer_size <= 0)
		return (0);
	buf_end = buffer + buffer_size;
	buf = buffer;

	for ( /* nothing */; buf < buf_end && *a != '\0'; buf++, a++ )
		*buf = *a;
	if (buf == buf_end)
		return (0);
	if (separator != '/' || buf == buffer || buf[-1] != '/')
		*buf++ = separator;
	if (buf == buf_end)
		return (0);
	for ( /* nothing */; buf < buf_end && *b != '\0'; buf++, b++ )
		*buf = *b;
	if (buf == buf_end)
		return (0);
	*buf = '\0';
	return (1);
}

int
strcmp_until(const char *left, const char *right, int until) {
	while (*left && *left != until && *left == *right) {
		left++;
		right++;
	}

	if ((*left=='\0' || *left == until) &&
	    (*right=='\0' || *right == until)) {
		return (0);
	}
	return (*left - *right);
}


/* strdtb(s) - delete trailing blanks in string 's' and return new length
 */
int
strdtb(s)
	char	*s;
{
	char	*x = s;

	/* scan forward to the null
	 */
	while (*x)
		x++;

	/* scan backward to either the first character before the string,
	 * or the last non-blank in the string, whichever comes first.
	 */
	do	{x--;}
	while (x >= s && isspace(*x));

	/* one character beyond where we stopped above is where the null
	 * goes.
	 */
	*++x = '\0';

	/* the difference between the position of the null character and
	 * the position of the first character of the string is the length.
	 */
	return (x - s);
}


int
set_debug_flags(flags)
	char	*flags;
{
	/* debug flags are of the form    flag[,flag ...]
	 *
	 * if an error occurs, print a message to stdout and return FALSE.
	 * otherwise return TRUE after setting ERROR_FLAGS.
	 */

#if !DEBUGGING

	printf("this program was compiled without debugging enabled\n");
	return (FALSE);

#else /* DEBUGGING */

	char	*pc = flags;

	DebugFlags = 0;

	while (*pc) {
		const char	**test;
		int		mask;

		/* try to find debug flag name in our list.
		 */
		for (	test = DebugFlagNames, mask = 1;
			*test != NULL && strcmp_until(*test, pc, ',');
			test++, mask <<= 1
		    )
			;

		if (!*test) {
			fprintf(stderr,
				"unrecognized debug flag <%s> <%s>\n",
				flags, pc);
			return (FALSE);
		}

		DebugFlags |= mask;

		/* skip to the next flag
		 */
		while (*pc && *pc != ',')
			pc++;
		if (*pc == ',')
			pc++;
	}

	if (DebugFlags) {
		int	flag;

		fprintf(stderr, "debug flags enabled:");

		for (flag = 0;  DebugFlagNames[flag];  flag++)
			if (DebugFlags & (1 << flag))
				fprintf(stderr, " %s", DebugFlagNames[flag]);
		fprintf(stderr, "\n");
	}

	return (TRUE);

#endif /* DEBUGGING */
}


void
set_cron_uid()
{
#if defined(BSD) || defined(POSIX)
	if (seteuid(ROOT_UID) < OK) {
		perror("seteuid");
		exit(ERROR_EXIT);
	}
#else
	if (setuid(ROOT_UID) < OK) {
		perror("setuid");
		exit(ERROR_EXIT);
	}
#endif
}


void
set_cron_cwd()
{
	struct stat	sb;

	/* first check for CRONDIR ("/var/cron" or some such)
	 */
	if (stat(CRONDIR, &sb) < OK && errno == ENOENT) {
		perror(CRONDIR);
		if (OK == mkdir(CRONDIR, 0700)) {
			fprintf(stderr, "%s: created\n", CRONDIR);
			stat(CRONDIR, &sb);
		} else {
			fprintf(stderr, "%s: ", CRONDIR);
			perror("mkdir");
			exit(ERROR_EXIT);
		}
	}
	if (!(sb.st_mode & S_IFDIR)) {
		fprintf(stderr, "'%s' is not a directory, bailing out.\n",
			CRONDIR);
		exit(ERROR_EXIT);
	}
	if (chdir(CRONDIR) < OK) {
		fprintf(stderr, "cannot chdir(%s), bailing out.\n", CRONDIR);
		perror(CRONDIR);
		exit(ERROR_EXIT);
	}

	/* CRONDIR okay (now==CWD), now look at SPOOL_DIR ("tabs" or some such)
	 */
	if (stat(SPOOL_DIR, &sb) < OK && errno == ENOENT) {
		perror(SPOOL_DIR);
		if (OK == mkdir(SPOOL_DIR, 0700)) {
			fprintf(stderr, "%s: created\n", SPOOL_DIR);
			stat(SPOOL_DIR, &sb);
		} else {
			fprintf(stderr, "%s: ", SPOOL_DIR);
			perror("mkdir");
			exit(ERROR_EXIT);
		}
	}
	if (!(sb.st_mode & S_IFDIR)) {
		fprintf(stderr, "'%s' is not a directory, bailing out.\n",
			SPOOL_DIR);
		exit(ERROR_EXIT);
	}
}


/* acquire_daemonlock() - write our PID into /etc/cron.pid, unless
 *	another daemon is already running, which we detect here.
 *
 * note: main() calls us twice; once before forking, once after.
 *	we maintain static storage of the file pointer so that we
 *	can rewrite our PID into the PIDFILE after the fork.
 *
 * it would be great if fflush() disassociated the file buffer.
 */
void
acquire_daemonlock(closeflag)
	int closeflag;
{
	static	FILE	*fp = NULL;
	char		buf[3*MAX_FNAME];
	char		pidfile[MAX_FNAME];
	int		fd;
	PID_T		otherpid;

	if (closeflag && fp) {
		fclose(fp);
		fp = NULL;
		return;
	}

	if (!fp) {
		if (!glue_strings(pidfile, sizeof pidfile, PIDDIR,
		    PIDFILE, '/')) {
			fprintf(stderr, "%s%s: path too long\n",
				PIDDIR, PIDFILE);
			log_it("CRON", getpid(), "DEATH", "path too long");
			exit(ERROR_EXIT);
		}
		if ((-1 == (fd = open(pidfile, O_RDWR|O_CREAT, 0644))) ||
		    (NULL == (fp = fdopen(fd, "r+")))) {
			snprintf(buf, sizeof buf, "can't open or create %s: %s",
				pidfile, strerror(errno));
			fprintf(stderr, "%s: %s\n", ProgramName, buf);
			log_it("CRON", getpid(), "DEATH", buf);
			exit(ERROR_EXIT);
		}

		if (flock(fd, LOCK_EX|LOCK_NB) < OK) {
			int save_errno = errno;

			if (fscanf(fp, "%d", &otherpid) == 1)
				snprintf(buf, sizeof buf,
				    "can't lock %s, otherpid may be %d: %s",
				    pidfile, otherpid, strerror(save_errno));
			else
				snprintf(buf, sizeof buf,
				    "can't lock %s, otherpid unknown: %s",
				    pidfile, strerror(save_errno));
			fprintf(stderr, "%s: %s\n", ProgramName, buf);
			log_it("CRON", getpid(), "DEATH", buf);
			exit(ERROR_EXIT);
		}

		(void) fcntl(fd, F_SETFD, 1);
	}

	rewind(fp);
	fprintf(fp, "%ld\n", (long)getpid());
	fflush(fp);
	(void) ftruncate(fileno(fp), ftell(fp));

	/* abandon fd and fp even though the file is open. we need to
	 * keep it open and locked, but we don't need the handles elsewhere.
	 */
}

/* get_char(file) : like getc() but increment LineNumber on newlines
 */
int
get_char(file)
	FILE	*file;
{
	int	ch;

	ch = getc(file);
	if (ch == '\n')
		Set_LineNum(LineNumber + 1)
	return (ch);
}


/* unget_char(ch, file) : like ungetc but do LineNumber processing
 */
void
unget_char(ch, file)
	int	ch;
	FILE	*file;
{
	ungetc(ch, file);
	if (ch == '\n')
		Set_LineNum(LineNumber - 1)
}


/* get_string(str, max, file, termstr) : like fgets() but
 *		(1) has terminator string which should include \n
 *		(2) will always leave room for the null
 *		(3) uses get_char() so LineNumber will be accurate
 *		(4) returns EOF or terminating character, whichever
 */
int
get_string(string, size, file, terms)
	char	*string;
	int	size;
	FILE	*file;
	char	*terms;
{
	int	ch;

	while (EOF != (ch = get_char(file)) && !strchr(terms, ch)) {
		if (size > 1) {
			*string++ = (char) ch;
			size--;
		}
	}

	if (size > 0)
		*string = '\0';

	return (ch);
}


/* skip_comments(file) : read past comment (if any)
 */
void
skip_comments(file)
	FILE	*file;
{
	int	ch;

	while (EOF != (ch = get_char(file))) {
		/* ch is now the first character of a line.
		 */

		while (ch == ' ' || ch == '\t')
			ch = get_char(file);

		if (ch == EOF)
			break;

		/* ch is now the first non-blank character of a line.
		 */

		if (ch != '\n' && ch != '#')
			break;

		/* ch must be a newline or comment as first non-blank
		 * character on a line.
		 */

		while (ch != '\n' && ch != EOF)
			ch = get_char(file);

		/* ch is now the newline of a line which we're going to
		 * ignore.
		 */
	}
	if (ch != EOF)
		unget_char(ch, file);
}


/* int in_file(char *string, FILE *file)
 *	return TRUE if one of the lines in file matches string exactly,
 *	FALSE otherwise.
 */
static int
in_file(string, file)
	char *string;
	FILE *file;
{
	char line[MAX_TEMPSTR];

	rewind(file);
	while (fgets(line, MAX_TEMPSTR, file)) {
		if (line[0] != '\0')
			line[strlen(line)-1] = '\0';
		if (0 == strcmp(line, string))
			return (TRUE);
	}
	return (FALSE);
}


/* int allowed(char *username)
 *	returns TRUE if (ALLOW_FILE exists and user is listed)
 *	or (DENY_FILE exists and user is NOT listed)
 *	or (neither file exists but user=="root" so it's okay)
 */
int
allowed(username)
	char *username;
{
	static int	init = FALSE;
	static FILE	*allow, *deny;

	if (!init) {
		init = TRUE;
#if defined(ALLOW_FILE) && defined(DENY_FILE)
		allow = fopen(ALLOW_FILE, "r");
		deny = fopen(DENY_FILE, "r");
		Debug(DMISC, ("allow/deny enabled, %d/%d\n", !!allow, !!deny))
#else
		allow = NULL;
		deny = NULL;
#endif
	}

	if (allow)
		return (in_file(username, allow));
	if (deny)
		return (!in_file(username, deny));

#if defined(ALLOW_ONLY_ROOT)
	return (strcmp(username, ROOT_USER) == 0);
#else
	return (TRUE);
#endif
}


void
log_it(username, xpid, event, detail)
	const char *username;
	int	xpid;
	const char *event;
	const char *detail;
{
#if defined(LOG_FILE) || DEBUGGING
	PID_T		pid = xpid;
#endif
#if defined(LOG_FILE)
	char		*msg;
	size_t		msglen;
	TIME_T		now = time((TIME_T) 0);
	struct tm	*t = localtime(&now);
#endif /*LOG_FILE*/

#if defined(SYSLOG)
	static int	syslog_open = 0;
#endif

#if defined(LOG_FILE)
	/* we assume that MAX_TEMPSTR will hold the date, time, &punctuation.
	 */
	msglen = strlen(username) + strlen(event) + strlen(detail) +
	    MAX_TEMPSTR;
	if ((msg = malloc(msglen)) == NULL)
		return;

	if (LogFD < OK) {
		LogFD = open(LOG_FILE, O_WRONLY|O_APPEND|O_CREAT, 0600);
		if (LogFD < OK) {
			fprintf(stderr, "%s: can't open log file\n",
				ProgramName);
			perror(LOG_FILE);
		} else {
			(void) fcntl(LogFD, F_SETFD, 1);
		}
	}

	/* we have to snprintf() it because fprintf() doesn't always write
	 * everything out in one chunk and this has to be atomically appended
	 * to the log file.
	 */
	snprintf(msg, msglen, "%s (%02d/%02d-%02d:%02d:%02d-%d) %s (%s)\n",
		username,
		t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, pid,
		event, detail);

	/* we have to run strlen() because sprintf() returns (char*) on old BSD
	 */
	if (LogFD < OK || write(LogFD, msg, strlen(msg)) < OK) {
		if (LogFD >= OK)
			perror(LOG_FILE);
		fprintf(stderr, "%s: can't write to log file\n", ProgramName);
		write(STDERR, msg, strlen(msg));
	}

	free(msg);
#endif /*LOG_FILE*/

#if defined(SYSLOG)
	if (!syslog_open) {
		/* we don't use LOG_PID since the pid passed to us by
		 * our client may not be our own.  therefore we want to
		 * print the pid ourselves.
		 */
# ifdef LOG_DAEMON
		openlog(ProgramName, LOG_PID, LOG_CRON);
# else
		openlog(ProgramName, LOG_PID);
# endif
		syslog_open = TRUE;		/* assume openlog success */
	}

	syslog(LOG_INFO, "(%s) %s (%s)", username, event, detail);

#endif /*SYSLOG*/

#if DEBUGGING
	if (DebugFlags) {
		fprintf(stderr, "log_it: (%s %ld) %s (%s)\n",
			username, (long)pid, event, detail);
	}
#endif
}


void
log_close() {
	if (LogFD != ERR) {
		close(LogFD);
		LogFD = ERR;
	}
}


/* two warnings:
 *	(1) this routine is fairly slow
 *	(2) it returns a pointer to static storage
 */
char *
first_word(s, t)
	char *s;	/* string we want the first word of */
	char *t;	/* terminators, implicitly including \0 */
{
	static char retbuf[2][MAX_TEMPSTR + 1];	/* sure wish C had GC */
	static int retsel = 0;
	char *rb, *rp;

	/* select a return buffer */
	retsel = 1-retsel;
	rb = &retbuf[retsel][0];
	rp = rb;

	/* skip any leading terminators */
	while (*s && (NULL != strchr(t, *s))) {
		s++;
	}

	/* copy until next terminator or full buffer */
	while (*s && (NULL == strchr(t, *s)) && (rp < &rb[MAX_TEMPSTR])) {
		*rp++ = *s++;
	}

	/* finish the return-string and return it */
	*rp = '\0';
	return (rb);
}


/* warning:
 *	heavily ascii-dependent.
 */
void
mkprint(dst, src, len)
	char *dst;
	unsigned char *src;
	int len;
{
	/*
	 * XXX
	 * We know this routine can't overflow the dst buffer because mkprints()
	 * allocated enough space for the worst case.
	 */
	while (len-- > 0)
	{
		unsigned char ch = *src++;

		if (ch < ' ') {			/* control character */
			*dst++ = '^';
			*dst++ = ch + '@';
		} else if (ch < 0177) {		/* printable */
			*dst++ = ch;
		} else if (ch == 0177) {	/* delete/rubout */
			*dst++ = '^';
			*dst++ = '?';
		} else {			/* parity character */
			snprintf(dst, 5, "\\%03o", ch);
			dst += 4;
		}
	}
	*dst = '\0';
}


/* warning:
 *	returns a pointer to malloc'd storage, you must call free yourself.
 */
char *
mkprints(src, len)
	unsigned char *src;
	unsigned int len;
{
	char *dst = malloc(len*4 + 1);

	if (dst)
		mkprint(dst, src, len);

	return (dst);
}


#ifdef MAIL_DATE
/* Sat, 27 Feb 1993 11:44:51 -0800 (CST)
 * 1234567890123456789012345678901234567
 */
char *
arpadate(clock)
	time_t *clock;
{
	time_t t = clock ? *clock : time(NULL);
	struct tm *tm = localtime(&t);
	static char ret[64];	/* zone name might be >3 chars */
	char *qmark;
	size_t len;
	long gmtoff = get_gmtoff(&t, tm);
	int hours = gmtoff / 3600;
	int minutes = (gmtoff - (hours * 3600)) / 60;

	if (minutes < 0)
		minutes = -minutes;

	/* Defensive coding (almost) never hurts... */
	len = strftime(ret, sizeof(ret), "%a, %e %b %Y %T ????? (%Z)", tm);
	if (len == 0) {
		ret[0] = '?';
		ret[1] = '\0';
		return (ret);
	}
	qmark = strchr(ret, '?');
	if (qmark && len - (qmark - ret) >= 6) {
		snprintf(qmark, 6, "% .2d%.2d", hours, minutes);
		qmark[5] = ' ';
	}
	return (ret);
}
#endif /*MAIL_DATE*/

#ifdef HAVE_SAVED_UIDS
static uid_t save_euid;
int swap_uids() { save_euid = geteuid(); return (seteuid(getuid())); }
int swap_uids_back() { return (seteuid(save_euid)); }
#else /*HAVE_SAVED_UIDS*/
int swap_uids() { return (setreuid(geteuid(), getuid())); }
int swap_uids_back() { return (swap_uids()); }
#endif /*HAVE_SAVED_UIDS*/

/* Return the offset from GMT in seconds (algorithm taken from sendmail). */
#ifndef HAVE_TM_GMTOFF
long get_gmtoff(time_t *clock, struct tm *local)
{
	struct tm gmt;
	long offset;

	gmt = *gmtime(clock);
	if (local == NULL)
		local = localtime(clock);

	offset = (local->tm_sec - gmt.tm_sec) +
	    ((local->tm_min - gmt.tm_min) * 60) +
	    ((local->tm_hour - gmt.tm_hour) * 3600);

	/* Timezone may cause year rollover to happen on a different day. */
	if (local->tm_year < gmt.tm_year)
		offset -= 24 * 3600;
	else if (local->tm_year > gmt.tm_year)
		offset -= 24 * 3600;
	else if (local->tm_yday < gmt.tm_yday)
		offset -= 24 * 3600;
	else if (local->tm_yday > gmt.tm_yday)
		offset += 24 * 3600;

	return (offset);
}
#endif /* HAVE_TM_GMTOFF */
