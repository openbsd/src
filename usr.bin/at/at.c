/*	$OpenBSD: at.c,v 1.26 2002/05/11 23:16:44 millert Exp $	*/
/*	$NetBSD: at.c,v 1.4 1995/03/25 18:13:31 glass Exp $	*/

/*
 *  at.c : Put file into atrun queue
 *  Copyright (C) 1993, 1994  Thomas Koenig
 *
 *  Atrun & Atq modifications
 *  Copyright (C) 1993  David Parsons
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* System Headers */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#include <locale.h>
#include <err.h>

#if (MAXLOGNAME-1) > UT_NAMESIZE
#define LOGNAMESIZE UT_NAMESIZE
#else
#define LOGNAMESIZE (MAXLOGNAME-1)
#endif

/* Local headers */
#include "at.h"
#include "panic.h"
#include "parsetime.h"
#include "perm.h"
#include "pathnames.h"
#define MAIN
#include "privs.h"

/* Macros */
#define ALARMC 10		/* Number of seconds to wait for timeout */

#define TIMESIZE 50

enum { ATQ, ATRM, AT, BATCH, CAT };	/* what program we want to run */

/* File scope variables */
#ifndef lint
static const char rcsid[] = "$OpenBSD: at.c,v 1.26 2002/05/11 23:16:44 millert Exp $";
#endif

char *no_export[] =
{
	"TERM", "TERMCAP", "DISPLAY", "_"
};
static int send_mail = 0;

/* External variables */

extern char **environ;
int fcreated;
char atfile[FILENAME_MAX];

char *atinput = (char *)0;	/* where to get input from */
char atqueue = 0;		/* which queue to examine for jobs (atq) */
char atverify = 0;		/* verify time instead of queuing job */

/* Function declarations */

static void sigc(int);
static void alarmc(int);
static char *cwdname(void);
static void writefile(time_t, char);
static void list_jobs(void);
static time_t ttime(const char *);

/* Signal catching functions */

static void 
sigc(int signo)
{
	/* If the user presses ^C, remove the spool file and exit. */
	if (fcreated) {
		PRIV_START;
		(void)unlink(atfile);
		PRIV_END;
	}

	_exit(EXIT_FAILURE);
}

static void 
alarmc(int signo)
{
	char buf[1024];

	/* Time out after some seconds. */
	strlcpy(buf, __progname, sizeof(buf));
	strlcat(buf, ": File locking timed out\n", sizeof(buf));
	write(STDERR_FILENO, buf, strlen(buf));
	if (fcreated) {
		PRIV_START;
		unlink(atfile);
		PRIV_END;
	}
	_exit(EXIT_FAILURE);
}

/* Local functions */

static char *
cwdname(void)
{
	/*
	 * Read in the current directory; the name will be overwritten on
	 * subsequent calls.
	 */
	static char path[MAXPATHLEN];

	return (getcwd(path, sizeof(path)));
}

static int
nextjob(void)
{
	int jobno;
	FILE *fid;

	/* We require that the sequence file already exist. */
	if ((fid = fopen(_PATH_SEQFILE, "r+")) == NULL)
		return (EOF);

	if (fscanf(fid, "%5x", &jobno) == 1)
		jobno = (jobno + 1) % 0xfffff;	/* 2^20 jobs enough? */
	else
		jobno = 1;
	(void)rewind(fid);
	(void)fprintf(fid, "%05x\n", jobno);
	(void)fclose(fid);

	return (jobno);
}

static void
writefile(time_t runtimer, char queue)
{
	/*
	 * This does most of the work if at or batch are invoked for
	 * writing a job.
	 */
	int jobno;
	char *ap, *ppos, *mailname;
	struct passwd *pass_entry;
	struct stat statbuf;
	int fdes, lockdes, fd2;
	FILE *fp, *fpin;
	struct sigaction act;
	char **atenv;
	int ch;
	mode_t cmask;
	struct flock lock;

	(void)setlocale(LC_TIME, "");

	/*
	 * Install the signal handler for SIGINT; terminate after removing the
	 * spool file if necessary
	 */
	memset(&act, 0, sizeof act);
	act.sa_handler = sigc;
	sigemptyset(&(act.sa_mask));
	act.sa_flags = 0;

	sigaction(SIGINT, &act, NULL);

	(void)strlcpy(atfile, _PATH_ATJOBS, sizeof atfile);
	ppos = atfile + strlen(atfile);

	/*
	 * Loop over all possible file names for running something at this
	 * particular time, see if a file is there; the first empty slot at
	 * any particular time is used.  Lock the jobs directory first
	 * to make sure we're alone when doing this.
	 */

	PRIV_START;

	/*
	 * Set an alarm so we don't sleep forever waiting on the lock.
	 * If we don't succeed with ALARMC seconds, something is wrong...
	 */
	act.sa_handler = alarmc;
	sigemptyset(&(act.sa_mask));
	act.sa_flags = 0;
	sigaction(SIGALRM, &act, NULL);
	alarm(ALARMC);
	lockdes = open(_PATH_ATJOBS, O_RDONLY|O_EXLOCK, 0);
	alarm(0);

	if (lockdes < 0)
		perr("Cannot lock jobs dir");

	if ((jobno = nextjob()) == EOF)
		perr("Cannot generate job number");

	(void)snprintf(ppos, sizeof(atfile) - (ppos - atfile),
	    "%c%5x%8x", queue, jobno, (unsigned) (runtimer/60));

	for (ap = ppos; *ap != '\0'; ap++)
		if (*ap == ' ')
			*ap = '0';

	if (stat(atfile, &statbuf) != 0)
		if (errno != ENOENT)
			perr2("Cannot access ", _PATH_ATJOBS);

	/*
	 * Create the file. The x bit is only going to be set after it has
	 * been completely written out, to make sure it is not executed in
	 * the meantime.  To make sure they do not get deleted, turn off
	 * their r bit.  Yes, this is a kluge.
	 */
	cmask = umask(S_IRUSR | S_IWUSR | S_IXUSR);
	if ((fdes = open(atfile, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR)) == -1)
		perr("Cannot create atjob file");

	if ((fd2 = dup(fdes)) < 0)
		perr("Error in dup() of job file");

	if (fchown(fd2, real_uid, real_gid) != 0)
		perr("Cannot give away file");

	PRIV_END;

	/*
	 * We've successfully created the file; let's set the flag so it
	 * gets removed in case of an interrupt or error.
	 */
	fcreated = 1;

	/* Now we can release the lock, so other people can access it */
	lock.l_type = F_UNLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	(void)fcntl(lockdes, F_SETLKW, &lock);
	(void)close(lockdes);

	if ((fp = fdopen(fdes, "w")) == NULL)
		panic("Cannot reopen atjob file");

	/*
	 * Get the userid to mail to, first by trying getlogin(), which asks
	 * the kernel, then from $LOGNAME or $USER, finally from getpwuid().
	 */
	mailname = getlogin();
	if (mailname == NULL && (mailname = getenv("LOGNAME")) == NULL)
		mailname = getenv("USER");

	if ((mailname == NULL) || (mailname[0] == '\0') ||
	    (strlen(mailname) > LOGNAMESIZE) || (getpwnam(mailname) == NULL)) {
		pass_entry = getpwuid(real_uid);
		if (pass_entry != NULL)
			mailname = pass_entry->pw_name;
	}

	if (atinput != NULL) {
		fpin = freopen(atinput, "r", stdin);
		if (fpin == NULL)
			perr("Cannot open input file");
	}
	(void)fprintf(fp, "#!/bin/sh\n# atrun uid=%u gid=%u\n# mail %*s %d\n",
	    real_uid, real_gid, LOGNAMESIZE, mailname, send_mail);

	/* Write out the umask at the time of invocation */
	(void)fprintf(fp, "umask %o\n", cmask);

	/*
	 * Write out the environment. Anything that may look like a special
	 * character to the shell is quoted, except for \n, which is done
	 * with a pair of "'s.  Dont't export the no_export list (such as
	 * TERM or DISPLAY) because we don't want these.
	 */
	for (atenv = environ; *atenv != NULL; atenv++) {
		int export = 1;
		char *eqp;

		eqp = strchr(*atenv, '=');
		if (eqp == NULL)
			eqp = *atenv;
		else {
			int i;

			for (i = 0;i < sizeof(no_export) /
			    sizeof(no_export[0]); i++) {
				export = export
				    && (strncmp(*atenv, no_export[i],
					(size_t) (eqp - *atenv)) != 0);
			}
			eqp++;
		}

		if (export) {
			(void)fwrite(*atenv, sizeof(char), eqp - *atenv, fp);
			for (ap = eqp; *ap != '\0'; ap++) {
				if (*ap == '\n')
					(void)fprintf(fp, "\"\n\"");
				else {
					if (!isalnum(*ap)) {
						switch (*ap) {
						case '%': case '/': case '{':
						case '[': case ']': case '=':
						case '}': case '@': case '+':
						case '#': case ',': case '.':
						case ':': case '-': case '_':
							break;
						default:
							(void)fputc('\\', fp);
							break;
						}
					}
					(void)fputc(*ap, fp);
				}
			}
			(void)fputs("; export ", fp);
			(void)fwrite(*atenv, sizeof(char), eqp - *atenv - 1, fp);
			(void)fputc('\n', fp);
		}
	}
	/*
	 * Cd to the directory at the time and write out all the
	 * commands the user supplies from stdin.
	 */
	if ((ap = cwdname()) == NULL)
		perr("Cannot get current working directory");
	(void)fputs("cd ", fp);
	for (; *ap != '\0'; ap++) {
		if (*ap == '\n')
			fprintf(fp, "\"\n\"");
		else {
			if (*ap != '/' && !isalnum(*ap))
				(void)fputc('\\', fp);

			(void)fputc(*ap, fp);
		}
	}
	/*
	 * Test cd's exit status: die if the original directory has been
	 * removed, become unreadable or whatever.
	 */
	(void)fprintf(fp, " || {\n\t echo 'Execution directory inaccessible' >&2\n\t exit 1\n}\n");

	if ((ch = getchar()) == EOF)
		panic("Input error");

	do {
		(void)fputc(ch, fp);
	} while ((ch = getchar()) != EOF);

	(void)fprintf(fp, "\n");
	if (ferror(fp))
		panic("Output error");

	if (ferror(stdin))
		panic("Input error");

	(void)fclose(fp);

	/*
	 * Set the x bit so that we're ready to start executing
	 */
	if (fchmod(fd2, S_IRUSR | S_IWUSR | S_IXUSR) < 0)
		perr("Cannot give away file");

	(void)close(fd2);
	(void)fprintf(stderr, "Job %d will be executed using /bin/sh\n", jobno);
}

static void
list_jobs(void)
{
	/*
	 * List all a user's jobs in the queue, by looping through
	 * _PATH_ATJOBS, or everybody's if we are root
	 */
	struct passwd *pw;
	DIR *spool;
	struct dirent *dirent;
	struct stat buf;
	struct tm runtime;
	unsigned long ctm;
	char queue;
	int jobno;
	time_t runtimer;
	char timestr[TIMESIZE];
	int first = 1;

	PRIV_START;

	if (chdir(_PATH_ATJOBS) != 0)
		perr2("Cannot change to ", _PATH_ATJOBS);

	if ((spool = opendir(".")) == NULL)
		perr2("Cannot open ", _PATH_ATJOBS);

	/* Loop over every file in the directory */
	while ((dirent = readdir(spool)) != NULL) {
		if (stat(dirent->d_name, &buf) != 0)
			perr2("Cannot stat in ", _PATH_ATJOBS);

		/*
		 * See it's a regular file and has its x bit turned on and
		 * is the user's
		 */
		if (!S_ISREG(buf.st_mode)
		    || ((buf.st_uid != real_uid) && !(real_uid == 0))
		    || !(S_IXUSR & buf.st_mode || atverify))
			continue;

		if (sscanf(dirent->d_name, "%c%5x%8lx", &queue, &jobno, &ctm) != 3)
			continue;

		if (atqueue && (queue != atqueue))
			continue;

		runtimer = 60 * (time_t) ctm;
		runtime = *localtime(&runtimer);
		strftime(timestr, TIMESIZE, "%+", &runtime);
		if (first) {
			(void)printf("Date\t\t\t\tOwner\t\tQueue\tJob#\n");
			first = 0;
		}
		pw = getpwuid(buf.st_uid);

		(void)printf("%s\t%-16s%c%s\t%d\n",
		    timestr,
		    pw ? pw->pw_name : "???",
		    queue,
		    (S_IXUSR & buf.st_mode) ? "" : "(done)",
		    jobno);
	}
	PRIV_END;
}

static void
process_jobs(int argc, char **argv, int what)
{
	/* Delete every argument (job - ID) given */
	int i;
	struct stat buf;
	DIR *spool;
	struct dirent *dirent;
	unsigned long ctm;
	char queue;
	int jobno;

	PRIV_START;

	if (chdir(_PATH_ATJOBS) != 0)
		perr2("Cannot change to ", _PATH_ATJOBS);

	if ((spool = opendir(".")) == NULL)
		perr2("Cannot open ", _PATH_ATJOBS);

	PRIV_END;

	/* Loop over every file in the directory */
	while((dirent = readdir(spool)) != NULL) {

		PRIV_START;
		if (stat(dirent->d_name, &buf) != 0)
			perr2("Cannot stat in ", _PATH_ATJOBS);
		PRIV_END;

		if (sscanf(dirent->d_name, "%c%5x%8lx", &queue, &jobno, &ctm) !=3)
			continue;

		for (i = optind; i < argc; i++) {
			if (atoi(argv[i]) == jobno) {
				if ((buf.st_uid != real_uid) && !(real_uid == 0))
					errx(EXIT_FAILURE,
					     "%s: Not owner\n", argv[i]);
				switch (what) {
				case ATRM:
					PRIV_START;

					if (unlink(dirent->d_name) != 0)
						perr(dirent->d_name);

					PRIV_END;

					break;

				case CAT:
					{
						FILE *fp;
						int ch;

						PRIV_START;

						fp = fopen(dirent->d_name, "r");

						PRIV_END;

						if (!fp)
							perr("Cannot open file");

						while((ch = getc(fp)) != EOF)
							putchar(ch);
					}
					break;

				default:
					errx(EXIT_FAILURE,
					    "Internal error, process_jobs = %d",
					    what);
					break;
				}
			}
		}
	}
}				/* delete_jobs */

#define	ATOI2(s)	((s) += 2, ((s)[-2] - '0') * 10 + ((s)[-1] - '0'))

static time_t
ttime(const char *arg)
{
	/*
	 * This is pretty much a copy of stime_arg1() from touch.c.  I changed
	 * the return value and the argument list because it's more convenient
	 * (IMO) to do everything in one place. - Joe Halpin
	 */
	struct timeval tv[2];
	time_t now;
	struct tm *t;
	int yearset;
	char *p;
	
	if (gettimeofday(&tv[0], NULL))
		panic("Cannot get current time");
	
	/* Start with the current time. */
	now = tv[0].tv_sec;
	if ((t = localtime(&now)) == NULL)
		panic("localtime");
	/* [[CC]YY]MMDDhhmm[.SS] */
	if ((p = strchr(arg, '.')) == NULL)
		t->tm_sec = 0;		/* Seconds defaults to 0. */
	else {
		if (strlen(p + 1) != 2)
			goto terr;
		*p++ = '\0';
		t->tm_sec = ATOI2(p);
	}
	
	yearset = 0;
	switch(strlen(arg)) {
	case 12:			/* CCYYMMDDhhmm */
		t->tm_year = ATOI2(arg);
		t->tm_year *= 100;
		yearset = 1;
		/* FALLTHROUGH */
	case 10:			/* YYMMDDhhmm */
		if (yearset) {
			yearset = ATOI2(arg);
			t->tm_year += yearset;
		} else {
			yearset = ATOI2(arg);
			t->tm_year = yearset + 2000;
		}
		t->tm_year -= 1900;	/* Convert to UNIX time. */
		/* FALLTHROUGH */
	case 8:				/* MMDDhhmm */
		t->tm_mon = ATOI2(arg);
		--t->tm_mon;		/* Convert from 01-12 to 00-11 */
		t->tm_mday = ATOI2(arg);
		t->tm_hour = ATOI2(arg);
		t->tm_min = ATOI2(arg);
		break;
	default:
		goto terr;
	}
	
	t->tm_isdst = -1;		/* Figure out DST. */
	tv[0].tv_sec = tv[1].tv_sec = mktime(t);
	if (tv[0].tv_sec != -1)
		return (tv[0].tv_sec);
	else
    terr:
		panic("out of range or illegal time specification: "
		    "[[CC]YY]MMDDhhmm[.SS]");
}

/* Global functions */

int
main(int argc, char **argv)
{
	int c;
	char queue = DEFAULT_AT_QUEUE;
	char queue_set = 0;
	int program = AT;			/* default program mode */
	char *options = "q:f:t:bcdlmrv";	/* default options for at */
	time_t timer;
	int tflag = 0;

	RELINQUISH_PRIVS;

	/* find out what this program is supposed to do */
	if (strcmp(__progname, "atq") == 0) {
		program = ATQ;
		options = "q:v";
	} else if (strcmp(__progname, "atrm") == 0) {
		program = ATRM;
		options = "";
	} else if (strcmp(__progname, "batch") == 0) {
		program = BATCH;
		options = "f:q:mv";
	}

	/* process whatever options we can process */
	opterr = 1;
	while ((c = getopt(argc, argv, options)) != -1)
		switch (c) {
		case 'v':	/* verify time settings */
			atverify = 1;
			break;

		case 'm':	/* send mail when job is complete */
			send_mail = 1;
			break;

		case 'f':
			atinput = optarg;
			break;

		case 'q':	/* specify queue */
			if (strlen(optarg) > 1)
				usage();

			atqueue = queue = *optarg;
			if (!(islower(queue) || isupper(queue)))
				usage();

			queue_set = 1;
			break;

		case 'd':		/* for backwards compatibility */
		case 'r':
			if (program != AT)
				usage();

			program = ATRM;
			options = "";
			break;

		case 't':
			if (program != AT)
				usage();
			tflag++;
			timer = ttime(optarg);
			break;

		case 'l':
			if (program != AT)
				usage();

			program = ATQ;
			options = "q:v";
			break;

		case 'b':
			if (program != AT)
				usage();

			program = BATCH;
			options = "f:q:mv";
			break;

		case 'c':
			program = CAT;
			options = "";
			break;

		default:
			usage();
			break;
		}
	/* end of options eating */

	if (!check_permission())
		errx(EXIT_FAILURE, "You do not have permission to use %s.",
		     __progname);

	/* select our program */
	switch (program) {
	case ATQ:
		if (optind != argc)
			usage();
		list_jobs();
		break;

	case ATRM:
	case CAT:
		if (optind == argc)
			usage();
		process_jobs(argc, argv, program);
		break;

	case AT:
		/* Time may have been specified via the -t flag. */
		if (!tflag)
			timer = parsetime(argc, argv);
		if (atverify) {
			struct tm *tm = localtime(&timer);
			(void)fprintf(stderr, "%s\n", asctime(tm));
		}
		writefile(timer, queue);
		break;

	case BATCH:
		if (queue_set)
			queue = toupper(queue);
		else
			queue = DEFAULT_BATCH_QUEUE;

		if (argc > optind)
			timer = parsetime(argc, argv);
		else
			timer = time(NULL);

		if (atverify) {
			struct tm *tm = localtime(&timer);
			(void)fprintf(stderr, "%s\n", asctime(tm));
		}

		writefile(timer, queue);
		break;

	default:
		panic("Internal error");
		break;
	}
	exit(EXIT_SUCCESS);
}
