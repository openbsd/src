/*	$OpenBSD: at.c,v 1.32 2003/01/02 15:48:40 mpech Exp $	*/
/*	$NetBSD: at.c,v 1.4 1995/03/25 18:13:31 glass Exp $	*/

/*
 *  at.c : Put file into atrun queue
 *  Copyright (C) 1993, 1994  Thomas Koenig
 *
 *  Atrun & Atq modifications
 *  Copyright (C) 1993  David Parsons
 *
 *  Traditional BSD behavior and other significant modifications
 *  Copyright (C) 2002  Todd C. Miller
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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <utmp.h>

#if (MAXLOGNAME-1) > UT_NAMESIZE
#define LOGNAMESIZE UT_NAMESIZE
#else
#define LOGNAMESIZE (MAXLOGNAME-1)
#endif

#include "at.h"
#include "panic.h"
#include "parsetime.h"
#include "perm.h"
#include "pathnames.h"
#define MAIN
#include "privs.h"

#define ALARMC 10		/* Number of seconds to wait for timeout */
#define TIMESIZE 50		/* Size of buffer passed to strftime() */

#ifndef lint
static const char rcsid[] = "$OpenBSD: at.c,v 1.32 2003/01/02 15:48:40 mpech Exp $";
#endif

/* Variables to remove from the job's environment. */
char *no_export[] =
{
	"TERM", "TERMCAP", "DISPLAY", "_", "SHELLOPTS", "BASH_VERSINFO",
	"EUID", "GROUPS", "PPID", "UID", "SSH_AUTH_SOCK", "SSH_AGENT_PID",
};

int program = AT;		/* default program mode */
char atfile[PATH_MAX];		/* path to the at spool file */
int fcreated;			/* whether or not we created the file yet */
char *atinput = NULL;		/* where to get input from */
char atqueue = 0;		/* which queue to examine for jobs (atq) */
char vflag = 0;			/* show completed but unremoved jobs (atq) */
char force = 0;			/* suppress errors (atrm) */
char interactive = 0;		/* interactive mode (atrm) */
static int send_mail = 0;	/* whether we are sending mail */

static void sigc(int);
static void alarmc(int);
static void writefile(time_t, char);
static void list_jobs(int, char **, int, int);
static void poke_daemon(void);
static time_t ttime(const char *);

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

static int
newjob(time_t runtimer, int queue)
{
	int fd, i;

	/*
	 * If we have a collision, try shifting the time by up to
	 * two minutes.  Perhaps it would be better to try different
	 * queues instead...
	 */
	for (i = 0; i < 120; i++) {
		snprintf(atfile, sizeof(atfile), "%s/%ld.%c",
		    _PATH_ATJOBS, (long)runtimer, queue);
		fd = open(atfile, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR);
		if (fd >= 0)
			return (fd);
		runtimer++;
	}
	return (-1);
}

/*
 * This does most of the work if at or batch are invoked for
 * writing a job.
 */
static void
writefile(time_t runtimer, char queue)
{
	char *ap, *mailname, *shell;
	char timestr[TIMESIZE];
	char path[PATH_MAX];
	struct passwd *pass_entry;
	struct tm runtime;
	int fdes, lockdes, fd2;
	FILE *fp, *fpin;
	struct sigaction act;
	char **atenv;
	int ch;
	mode_t cmask;
	extern char **environ;

	(void)setlocale(LC_TIME, "");

	/*
	 * Install the signal handler for SIGINT; terminate after removing the
	 * spool file if necessary
	 */
	memset(&act, 0, sizeof act);
	act.sa_handler = sigc;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);

	PRIV_START;

	/*
	 * Lock the jobs dir so we don't have to worry about someone
	 * else grabbing a file name out from under us.
	 * Set an alarm so we don't sleep forever waiting on the lock.
	 * If we don't succeed with ALARMC seconds, something is wrong...
	 */
	memset(&act, 0, sizeof act);
	act.sa_handler = alarmc;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGALRM, &act, NULL);
	alarm(ALARMC);
	lockdes = open(_PATH_ATJOBS, O_RDONLY|O_EXLOCK, 0);
	alarm(0);

	if (lockdes < 0)
		perr("Cannot lock jobs dir");

	/*
	 * Create the file. The x bit is only going to be set after it has
	 * been completely written out, to make sure it is not executed in
	 * the meantime.  To make sure they do not get deleted, turn off
	 * their r bit.  Yes, this is a kluge.
	 */
	cmask = umask(S_IRUSR | S_IWUSR | S_IXUSR);
	if ((fdes = newjob(runtimer, queue)) == -1)
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

	/*
	 * Get the shell to run the job under.  First check $SHELL, falling
	 * back to the user's shell in the password database or, failing
	 * that, /bin/sh.
	 */
	if ((shell = getenv("SHELL")) == NULL || *shell == '\0') {
		pass_entry = getpwuid(real_uid);
		if (pass_entry != NULL && *pass_entry->pw_shell != '\0')
			shell = pass_entry->pw_shell;
		else
			shell = _PATH_BSHELL;
	}

	if (atinput != NULL) {
		fpin = freopen(atinput, "r", stdin);
		if (fpin == NULL)
			perr("Cannot open input file");
	}
	(void)fprintf(fp, "#!/bin/sh\n# atrun uid=%ld gid=%ld\n# mail %*s %d\n",
	    (long)real_uid, (long)real_gid, LOGNAMESIZE, mailname, send_mail);

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
	if ((ap = getcwd(path, sizeof(path))) == NULL)
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
	(void)fprintf(fp, " || {\n\t echo 'Execution directory inaccessible'"
	    " >&2\n\t exit 1\n}\n");

	if ((ch = getchar()) == EOF)
		panic("Input error");

	/* We want the job to run under the user's shell. */
	fprintf(fp, "%s << '_END_OF_AT_JOB'\n", shell);

	do {
		(void)fputc(ch, fp);
	} while ((ch = getchar()) != EOF);

	(void)fprintf(fp, "\n_END_OF_AT_JOB\n");
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

	/* Poke cron so it knows to reload the at spool. */
	poke_daemon();

	runtime = *localtime(&runtimer);
	strftime(timestr, TIMESIZE, "%a %b %e %T %Y", &runtime);
	(void)fprintf(stderr, "commands will be executed using %s\n", shell);
	(void)fprintf(stderr, "job %s at %s\n", &atfile[sizeof(_PATH_ATJOBS)],
	    timestr);
}

/* Sort by creation time. */
static int
byctime(const void *v1, const void *v2)
{
	const struct atjob *j1 = *(struct atjob **)v1;
	const struct atjob *j2 = *(struct atjob **)v2;

	return (j1->ctime - j2->ctime);
}

/* Sort by job number (and thus execution time). */
static int
byjobno(const void *v1, const void *v2)
{
	const struct atjob *j1 = *(struct atjob **)v1;
	const struct atjob *j2 = *(struct atjob **)v2;

	if (j1->runtimer == j2->runtimer)
		return (j1->queue - j2->queue);
	return (j1->runtimer - j2->runtimer);
}

static void
print_job(struct atjob *job, int n, struct stat *st, int shortformat)
{
	struct passwd *pw;
	struct tm runtime;
	char timestr[TIMESIZE];
	static char *ranks[] = {
		"th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th"
	};

	runtime = *localtime(&job->runtimer);
	if (shortformat) {
		strftime(timestr, TIMESIZE, "%a %b %e %T %Y", &runtime);
		(void)printf("%ld.%c\t%s\n", (long)job->runtimer,
		    job->queue, timestr);
	} else {
		pw = getpwuid(st->st_uid);
		/* Rank hack shamelessly stolen from lpq */
		if (n / 10 == 1)
			printf("%3d%-5s", n,"th");
		else
			printf("%3d%-5s", n, ranks[n % 10]);
		strftime(timestr, TIMESIZE, "%b %e, %Y %R", &runtime);
		(void)printf("%-21.18s%-11.8s%10ld.%c   %c%s\n",
		    timestr, pw ? pw->pw_name : "???",
		    (long)job->runtimer, job->queue, job->queue,
		    (S_IXUSR & st->st_mode) ? "" : " (done)");
	}
}

/*
 * List all of a user's jobs in the queue, by looping through
 * _PATH_ATJOBS, or all jobs if we are root.  If argc is > 0, argv
 * contains the list of users whose jobs shall be displayed. By
 * default, the list is sorted by execution date and queue.  If
 * csort is non-zero jobs will be sorted by creation/submission date.
 */
static void
list_jobs(int argc, char **argv, int count_only, int csort)
{
	struct passwd *pw;
	struct dirent *dirent;
	struct atjob **atjobs, *job;
	struct stat stbuf;
	time_t runtimer;
	uid_t *uids;
	long l;
	char queue, *ep;
	DIR *spool;
	int i, shortformat, numjobs, maxjobs;

	if (argc) {
		if ((uids = malloc(sizeof(uid_t) * argc)) == NULL)
			err(EXIT_FAILURE, "malloc");

		for (i = 0; i < argc; i++) {
			if ((pw = getpwnam(argv[i])) == NULL)
				errx(EXIT_FAILURE,
				    "%s: invalid user name", argv[i]);
			if (pw->pw_uid != real_uid && real_uid != 0)
				errx(EXIT_FAILURE, "Only the superuser may "
				    "display other users' jobs");
			uids[i] = pw->pw_uid;
		}
	} else
		uids = NULL;

	shortformat = strcmp(__progname, "at") == 0;

	PRIV_START;

	if (chdir(_PATH_ATJOBS) != 0)
		perr2("Cannot change to ", _PATH_ATJOBS);

	if ((spool = opendir(".")) == NULL)
		perr2("Cannot open ", _PATH_ATJOBS);

	PRIV_END;

	if (fstat(dirfd(spool), &stbuf) != 0)
		perr2("Cannot stat ", _PATH_ATJOBS);

	/*
	 * The directory's link count should give us a good idea
	 * of how many files are in it.  Fudge things a little just
	 * in case someone adds a job or two.
	 */
	numjobs = 0;
	maxjobs = stbuf.st_nlink + 4;
	atjobs = (struct atjob **)malloc(maxjobs * sizeof(struct atjob *));
	if (atjobs == NULL)
		err(EXIT_FAILURE, "malloc");

	/* Loop over every file in the directory. */
	while ((dirent = readdir(spool)) != NULL) {
		PRIV_START;

		if (stat(dirent->d_name, &stbuf) != 0)
			perr2("Cannot stat in ", _PATH_ATJOBS);

		PRIV_END;

		/*
		 * See it's a regular file and has its x bit turned on and
		 * is the user's
		 */
		if (!S_ISREG(stbuf.st_mode)
		    || ((stbuf.st_uid != real_uid) && !(real_uid == 0))
		    || !(S_IXUSR & stbuf.st_mode || vflag))
			continue;

		l = strtol(dirent->d_name, &ep, 10);
		if (*ep != '.' || !isalpha(*(ep + 1)) || *(ep + 2) != '\0' ||
		    l < 0 || l >= INT_MAX)
			continue;
		runtimer = (time_t)l;
		queue = *(ep + 1);

		if (atqueue && (queue != atqueue))
			continue;

		/* Check against specified user(s). */
		if (argc) {
			for (i = 0; i < argc; i++) {
				if (uids[0] == stbuf.st_uid)
					break;
			}
			if (i == argc)
				continue;	/* user doesn't match */
		}

		if (count_only) {
			numjobs++;
			continue;
		}

		job = (struct atjob *)malloc(sizeof(struct atjob));
		if (job == NULL)
			err(EXIT_FAILURE, "malloc");
		job->runtimer = runtimer;
		job->ctime = stbuf.st_ctime;
		job->queue = queue;
		if (numjobs == maxjobs) {
		    maxjobs *= 2;
		    atjobs = realloc(atjobs, maxjobs * sizeof(struct atjob *));
		    if (atjobs == NULL)
			err(EXIT_FAILURE, "realloc");
		}
		atjobs[numjobs++] = job;
	}
	free(uids);

	if (count_only || numjobs == 0) {
		if (numjobs == 0 && !shortformat)
			fprintf(stderr, "no files in queue.\n");
		else if (count_only)
			printf("%d\n", numjobs);
		free(atjobs);
		return;
	}

	/* Sort by job run time or by job creation time. */
	qsort(atjobs, numjobs, sizeof(struct atjob *),
	    csort ? byctime : byjobno);

	if (!shortformat)
		(void)puts(" Rank     Execution Date     Owner          "
		    "Job       Queue");

	for (i = 0; i < numjobs; i++) {
		print_job(atjobs[i], i + 1, &stbuf, shortformat);
		free(atjobs[i]);
	}
	free(atjobs);
}

static int
rmok(int job)
{
	int ch, junk;

	printf("%d: remove it? ", job);
	ch = getchar();
	while ((junk = getchar()) != EOF && junk != '\n')
		;
	return (ch == 'y' || ch == 'Y');
}

/*
 * Loop through all jobs in _PATH_ATJOBS and display or delete ones
 * that match argv (may be job or username), or all if argc == 0.
 * Only the superuser may display/delete other people's jobs.
 */
static int
process_jobs(int argc, char **argv, int what)
{
	struct stat stbuf;
	struct dirent *dirent;
	struct passwd *pw;
	time_t runtimer;
	uid_t *uids;
	char **jobs, *ep, queue;
	long l;
	FILE *fp;
	DIR *spool;
	int job_matches, jobs_len, uids_len;
	int error, i, ch, changed;

	PRIV_START;

	if (chdir(_PATH_ATJOBS) != 0)
		perr2("Cannot change to ", _PATH_ATJOBS);

	if ((spool = opendir(".")) == NULL)
		perr2("Cannot open ", _PATH_ATJOBS);

	PRIV_END;

	/* Convert argv into a list of jobs and uids. */
	jobs = NULL;
	uids = NULL;
	jobs_len = uids_len = 0;
	if (argc > 0) {
		if ((jobs = malloc(sizeof(char *) * argc)) == NULL ||
		    (uids = malloc(sizeof(uid_t) * argc)) == NULL)
			err(EXIT_FAILURE, "malloc");

		for (i = 0; i < argc; i++) {
			l = strtol(argv[i], &ep, 10);
			if (*ep == '.' && isalpha(*(ep + 1)) &&
			    *(ep + 2) == '\0' && l > 0 && l < INT_MAX)
				jobs[jobs_len++] = argv[i];
			else if ((pw = getpwnam(argv[i])) != NULL) {
				if (real_uid != pw->pw_uid && real_uid != 0)
					errx(EXIT_FAILURE,
					    "Only the superuser may %s"
					    " other users' jobs", what == ATRM
					    ? "remove" : "print");
				uids[uids_len++] = pw->pw_uid;
			} else
				errx(EXIT_FAILURE,
				    "%s: invalid user name", argv[i]);
		}
	}

	/* Loop over every file in the directory */
	changed = 0;
	while ((dirent = readdir(spool)) != NULL) {

		PRIV_START;
		if (stat(dirent->d_name, &stbuf) != 0)
			perr2("Cannot stat in ", _PATH_ATJOBS);
		PRIV_END;

		if (stbuf.st_uid != real_uid && real_uid != 0)
			continue;

		l = strtol(dirent->d_name, &ep, 10);
		if (*ep != '.' || !isalpha(*(ep + 1)) || *(ep + 2) != '\0' ||
		    l < 0 || l >= INT_MAX)
			continue;
		runtimer = (time_t)l;
		queue = *(ep + 1);

		/* Check runtimer against argv; argc==0 means do all. */
		job_matches = (argc == 0) ? 1 : 0;
		if (!job_matches) {
			for (i = 0; i < jobs_len; i++) {
				if (jobs[i] != NULL &&
				    strcmp(dirent->d_name, jobs[i]) == 0) {
					jobs[i] = NULL;
					job_matches = 1;
					break;
				}
			}
		}
		if (!job_matches) {
			for (i = 0; i < uids_len; i++) {
				if (uids[i] == stbuf.st_uid) {
					job_matches = 1;
					break;
				}
			}
		}

		if (job_matches) {
			switch (what) {
			case ATRM:
				PRIV_START;

				if (!interactive ||
				    (interactive && rmok(runtimer))) {
					if (unlink(dirent->d_name) == 0)
						changed = 1;
					else
						perr(dirent->d_name);
					if (!force && !interactive)
						fprintf(stderr,
						    "%s removed\n",
						    dirent->d_name);
				}

				PRIV_END;

				break;

			case CAT:
				PRIV_START;

				fp = fopen(dirent->d_name, "r");

				PRIV_END;

				if (!fp)
					perr("Cannot open file");

				while ((ch = getc(fp)) != EOF)
					putchar(ch);

				break;

			default:
				errx(EXIT_FAILURE,
				    "Internal error, process_jobs = %d",
				    what);
				break;
			}
		}
	}
	for (error = 0, i = 0; i < jobs_len; i++) {
		if (jobs[i] != NULL) {
			if (!force)
				warnx("%s: no such job", jobs[i]);
			error++;
		}
	}
	free(jobs);
	free(uids);

	/* If we modied the spool, poke cron so it knows to reload. */
	if (changed)
		poke_daemon();

	return (error);
}

#define	ATOI2(s)	((s) += 2, ((s)[-2] - '0') * 10 + ((s)[-1] - '0'))

/*
 * This is pretty much a copy of stime_arg1() from touch.c.
 */
static time_t
ttime(const char *arg)
{
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

#define RELOAD_AT	0x4	/* XXX - from cron's macros.h */

/* XXX - share with crontab */
static void
poke_daemon() {
	int sock, flags;
	unsigned char poke;
	struct sockaddr_un sun;

	PRIV_START;

	if (utime(_PATH_ATJOBS, NULL) < 0) {
		warn("can't update mtime on %s", _PATH_ATJOBS);
		PRIV_END;
		return;
	}

	/* Failure to poke the daemon socket is not a fatal error. */
	(void) signal(SIGPIPE, SIG_IGN);
	strlcpy(sun.sun_path, CRONDIR "/" SPOOL_DIR "/" CRONSOCK,
	    sizeof(sun.sun_path));
	sun.sun_family = AF_UNIX;
	sun.sun_len = strlen(sun.sun_path);
	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0 &&
	    connect(sock, (struct sockaddr *)&sun, sizeof(sun)) == 0) {
		poke = RELOAD_AT;
		write(sock, &poke, 1);
		close(sock);
	} else
		fprintf(stderr, "Warning, cron does not appear to be running.\n");
	(void) signal(SIGPIPE, SIG_DFL);

	PRIV_END;
}

int
main(int argc, char **argv)
{
	time_t timer = -1;
	char queue = DEFAULT_AT_QUEUE;
	char queue_set = 0;
	char *options = "q:f:t:bcdlmrv";	/* default options for at */
	int ch;
	int aflag = 0;
	int cflag = 0;
	int nflag = 0;

	RELINQUISH_PRIVS;

	/* find out what this program is supposed to do */
	if (strcmp(__progname, "atq") == 0) {
		program = ATQ;
		options = "cnvq:";
	} else if (strcmp(__progname, "atrm") == 0) {
		program = ATRM;
		options = "afi";
	} else if (strcmp(__progname, "batch") == 0) {
		program = BATCH;
		options = "f:q:mv";
	}

	/* process whatever options we can process */
	while ((ch = getopt(argc, argv, options)) != -1) {
		switch (ch) {
		case 'a':
			aflag = 1;
			break;

		case 'i':
			interactive = 1;
			force = 0;
			break;

		case 'v':	/* show completed but unremoved jobs */
			/*
			 * This option is only useful when we are invoked
			 * as atq but we accept (and ignore) this flag in
			 * the other programs for backwards compatibility.
			 */
			vflag = 1;
			break;

		case 'm':	/* send mail when job is complete */
			send_mail = 1;
			break;

		case 'f':
			if (program == ATRM) {
				force = 1;
				interactive = 0;
			} else
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
			program = ATRM;
			options = "";
			break;

		case 't':
			timer = ttime(optarg);
			break;

		case 'l':
			program = ATQ;
			options = "cnvq:";
			break;

		case 'b':
			program = BATCH;
			options = "f:q:mv";
			break;

		case 'c':
			if (program == ATQ) {
				cflag = 1;
			} else {
				program = CAT;
				options = "";
			}
			break;

		case 'n':
			nflag = 1;
			break;

		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (!check_permission())
		errx(EXIT_FAILURE, "You do not have permission to use %s.",
		     __progname);

	/* select our program */
	switch (program) {
	case ATQ:
		list_jobs(argc, argv, nflag, cflag);
		break;

	case ATRM:
	case CAT:
		if ((aflag && argc) || (!aflag && !argc))
			usage();
		exit(process_jobs(argc, argv, program));
		break;

	case AT:
		/* Time may have been specified via the -t flag. */
		if (timer == -1)
			timer = parsetime(argc, argv);
		writefile(timer, queue);
		break;

	case BATCH:
		if (queue_set)
			queue = toupper(queue);
		else
			queue = DEFAULT_BATCH_QUEUE;

		if (argc > 0)
			timer = parsetime(argc, argv);
		else
			timer = time(NULL);

		writefile(timer, queue);
		break;

	default:
		panic("Internal error");
		break;
	}
	exit(EXIT_SUCCESS);
}
