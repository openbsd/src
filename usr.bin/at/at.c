/*	$OpenBSD: at.c,v 1.49 2007/05/23 22:30:53 millert Exp $	*/

/*
 *  at.c : Put file into atrun queue
 *  Copyright (C) 1993, 1994  Thomas Koenig
 *
 *  Atrun & Atq modifications
 *  Copyright (C) 1993  David Parsons
 *
 *  Traditional BSD behavior and other significant modifications
 *  Copyright (C) 2002-2003  Todd C. Miller
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

#define	MAIN_PROGRAM

#include "cron.h"
#include "at.h"
#include "privs.h"
#include <limits.h>

#define ALARMC 10		/* Number of seconds to wait for timeout */
#define TIMESIZE 50		/* Size of buffer passed to strftime() */

#ifndef lint
static const char rcsid[] = "$OpenBSD: at.c,v 1.49 2007/05/23 22:30:53 millert Exp $";
#endif

/* Variables to remove from the job's environment. */
char *no_export[] =
{
	"TERM", "TERMCAP", "DISPLAY", "_", "SHELLOPTS", "BASH_VERSINFO",
	"EUID", "GROUPS", "PPID", "UID", "SSH_AUTH_SOCK", "SSH_AGENT_PID",
};

int program = AT;		/* default program mode */
char atfile[MAX_FNAME];		/* path to the at spool file */
int fcreated;			/* whether or not we created the file yet */
char *atinput = NULL;		/* where to get input from */
char atqueue = 0;		/* which queue to examine for jobs (atq) */
char vflag = 0;			/* show completed but unremoved jobs (atq) */
char force = 0;			/* suppress errors (atrm) */
char interactive = 0;		/* interactive mode (atrm) */
static int send_mail = 0;	/* whether we are sending mail */

static void sigc(int);
static void alarmc(int);
static void writefile(const char *, time_t, char);
static void list_jobs(int, char **, int, int);
static time_t ttime(char *);
static int check_permission(void);
static __dead void panic(const char *);
static void perr(const char *);
static void perr2(const char *, const char *);
static __dead void usage(void);
time_t parsetime(int, char **);

/*
 * Something fatal has happened, print error message and exit.
 */
static __dead void
panic(const char *a)
{
	(void)fprintf(stderr, "%s: %s\n", ProgramName, a);
	if (fcreated) {
		PRIV_START;
		unlink(atfile);
		PRIV_END;
	}

	exit(ERROR_EXIT);
}

/*
 * Two-parameter version of panic().
 */
static __dead void
panic2(const char *a, const char *b)
{
	(void)fprintf(stderr, "%s: %s%s\n", ProgramName, a, b);
	if (fcreated) {
		PRIV_START;
		unlink(atfile);
		PRIV_END;
	}

	exit(ERROR_EXIT);
}

/*
 * Some operating system error; print error message and exit.
 */
static __dead void
perr(const char *a)
{
	if (!force)
		perror(a);
	if (fcreated) {
		PRIV_START;
		unlink(atfile);
		PRIV_END;
	}

	exit(ERROR_EXIT);
}

/*
 * Two-parameter version of perr().
 */
static __dead void
perr2(const char *a, const char *b)
{
	if (!force)
		(void)fputs(a, stderr);
	perr(b);
}

/* ARGSUSED */
static void
sigc(int signo)
{
	/* If the user presses ^C, remove the spool file and exit. */
	if (fcreated) {
		PRIV_START;
		(void)unlink(atfile);
		PRIV_END;
	}

	_exit(ERROR_EXIT);
}

/* ARGSUSED */
static void
alarmc(int signo)
{
	/* just return */
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
		snprintf(atfile, sizeof(atfile), "%s/%ld.%c", AT_DIR,
		    (long)runtimer, queue);
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
writefile(const char *cwd, time_t runtimer, char queue)
{
	const char *ap;
	char *mailname, *shell;
	char timestr[TIMESIZE];
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
	bzero(&act, sizeof act);
	act.sa_handler = sigc;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);

	PRIV_START;

	if ((lockdes = open(AT_DIR, O_RDONLY, 0)) < 0)
		perr("Cannot open jobs dir");

	/*
	 * Lock the jobs dir so we don't have to worry about someone
	 * else grabbing a file name out from under us.
	 * Set an alarm so we don't sleep forever waiting on the lock.
	 * If we don't succeed with ALARMC seconds, something is wrong...
	 */
	bzero(&act, sizeof act);
	act.sa_handler = alarmc;
	sigemptyset(&act.sa_mask);
#ifdef SA_INTERRUPT
	act.sa_flags = SA_INTERRUPT;
#endif
	sigaction(SIGALRM, &act, NULL);
	alarm(ALARMC);
	ch = flock(lockdes, LOCK_EX);
	alarm(0);
	if (ch != 0)
		panic("Unable to lock jobs dir");

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
	    (strlen(mailname) > MAX_UNAME) || (getpwnam(mailname) == NULL)) {
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
	(void)fprintf(fp, "#!/bin/sh\n# atrun uid=%lu gid=%lu\n# mail %*s %d\n",
	    (unsigned long)real_uid, (unsigned long)real_gid,
	    MAX_UNAME, mailname, send_mail);

	/* Write out the umask at the time of invocation */
	(void)fprintf(fp, "umask %o\n", cmask);

	/*
	 * Write out the environment. Anything that may look like a special
	 * character to the shell is quoted, except for \n, which is done
	 * with a pair of "'s.  Don't export the no_export list (such as
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
	(void)fputs("cd ", fp);
	for (ap = cwd; *ap != '\0'; ap++) {
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
	PRIV_START;
	poke_daemon(AT_DIR, RELOAD_AT);
	PRIV_END;

	runtime = *localtime(&runtimer);
	strftime(timestr, TIMESIZE, "%a %b %e %T %Y", &runtime);
	(void)fprintf(stderr, "commands will be executed using %s\n", shell);
	(void)fprintf(stderr, "job %s at %s\n", &atfile[sizeof(AT_DIR)],
	    timestr);
}

/* Sort by creation time. */
static int
byctime(const void *v1, const void *v2)
{
	const struct atjob *j1 = *(const struct atjob **)v1;
	const struct atjob *j2 = *(const struct atjob **)v2;

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
print_job(struct atjob *job, int n, int shortformat)
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
		pw = getpwuid(job->uid);
		/* Rank hack shamelessly stolen from lpq */
		if (n / 10 == 1)
			printf("%3d%-5s", n,"th");
		else
			printf("%3d%-5s", n, ranks[n % 10]);
		strftime(timestr, TIMESIZE, "%b %e, %Y %R", &runtime);
		(void)printf("%-21.18s%-11.8s%10ld.%c   %c%s\n",
		    timestr, pw ? pw->pw_name : "???",
		    (long)job->runtimer, job->queue, job->queue,
		    (S_IXUSR & job->mode) ? "" : " (done)");
	}
}

/*
 * List all of a user's jobs in the queue, by looping through
 * AT_DIR, or all jobs if we are root.  If argc is > 0, argv
 * contains the list of users whose jobs shall be displayed. By
 * default, the list is sorted by execution date and queue.  If
 * csort is non-zero jobs will be sorted by creation/submission date.
 */
static void
list_jobs(int argc, char **argv, int count_only, int csort)
{
	struct passwd *pw;
	struct dirent *dirent;
	struct atjob **atjobs, **newatjobs, *job;
	struct stat stbuf;
	time_t runtimer;
	uid_t *uids;
	long l;
	char queue, *ep;
	DIR *spool;
	int i, shortformat, numjobs, maxjobs;

	if (argc) {
		if ((uids = malloc(sizeof(uid_t) * argc)) == NULL)
			panic("Insufficient virtual memory");

		for (i = 0; i < argc; i++) {
			if ((pw = getpwnam(argv[i])) == NULL)
				panic2(argv[i], ": invalid user name");
			if (pw->pw_uid != real_uid && real_uid != 0)
				panic("Only the superuser may display other users' jobs");
			uids[i] = pw->pw_uid;
		}
	} else
		uids = NULL;

	shortformat = strcmp(ProgramName, "at") == 0;

	PRIV_START;

	if (chdir(AT_DIR) != 0)
		perr2("Cannot change to ", AT_DIR);

	if ((spool = opendir(".")) == NULL)
		perr2("Cannot open ", AT_DIR);

	PRIV_END;

	if (fstat(spool->dd_fd, &stbuf) != 0)
		perr2("Cannot stat ", AT_DIR);

	/*
	 * The directory's link count should give us a good idea
	 * of how many files are in it.  Fudge things a little just
	 * in case someone adds a job or two.
	 */
	numjobs = 0;
	maxjobs = stbuf.st_nlink + 4;
	atjobs = (struct atjob **)malloc(maxjobs * sizeof(struct atjob *));
	if (atjobs == NULL)
		panic("Insufficient virtual memory");

	/* Loop over every file in the directory. */
	while ((dirent = readdir(spool)) != NULL) {
		PRIV_START;

		if (stat(dirent->d_name, &stbuf) != 0)
			perr2("Cannot stat in ", AT_DIR);

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
			panic("Insufficient virtual memory");
		job->runtimer = runtimer;
		job->ctime = stbuf.st_ctime;
		job->uid = stbuf.st_uid;
		job->mode = stbuf.st_mode;
		job->queue = queue;
		if (numjobs == maxjobs) {
			int newjobs = maxjobs * 2;
			newatjobs = realloc(atjobs, newjobs * sizeof(job));
			if (newatjobs == NULL)
				panic("Insufficient virtual memory");
			atjobs = newatjobs;
			maxjobs = newjobs;
		}
		atjobs[numjobs++] = job;
	}
	free(uids);
	closedir(spool);

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
		print_job(atjobs[i], i + 1, shortformat);
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
 * Loop through all jobs in AT_DIR and display or delete ones
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
	char **jobs, *ep;
	long l;
	FILE *fp;
	DIR *spool;
	int job_matches, jobs_len, uids_len;
	int error, i, ch, changed;

	PRIV_START;

	if (chdir(AT_DIR) != 0)
		perr2("Cannot change to ", AT_DIR);

	if ((spool = opendir(".")) == NULL)
		perr2("Cannot open ", AT_DIR);

	PRIV_END;

	/* Convert argv into a list of jobs and uids. */
	jobs = NULL;
	uids = NULL;
	jobs_len = uids_len = 0;
	if (argc > 0) {
		if ((jobs = malloc(sizeof(char *) * argc)) == NULL ||
		    (uids = malloc(sizeof(uid_t) * argc)) == NULL)
			panic("Insufficient virtual memory");

		for (i = 0; i < argc; i++) {
			l = strtol(argv[i], &ep, 10);
			if (*ep == '.' && isalpha(*(ep + 1)) &&
			    *(ep + 2) == '\0' && l > 0 && l < INT_MAX)
				jobs[jobs_len++] = argv[i];
			else if ((pw = getpwnam(argv[i])) != NULL) {
				if (real_uid != pw->pw_uid && real_uid != 0) {
					fprintf(stderr, "%s: Only the superuser"
					    " may %s other users' jobs\n",
					    ProgramName, what == ATRM
					    ? "remove" : "view");
					exit(ERROR_EXIT);
				}
				uids[uids_len++] = pw->pw_uid;
			} else
				panic2(argv[i], ": invalid user name");
		}
	}

	/* Loop over every file in the directory */
	changed = 0;
	while ((dirent = readdir(spool)) != NULL) {

		PRIV_START;
		if (stat(dirent->d_name, &stbuf) != 0)
			perr2("Cannot stat in ", AT_DIR);
		PRIV_END;

		if (stbuf.st_uid != real_uid && real_uid != 0)
			continue;

		l = strtol(dirent->d_name, &ep, 10);
		if (*ep != '.' || !isalpha(*(ep + 1)) || *(ep + 2) != '\0' ||
		    l < 0 || l >= INT_MAX)
			continue;
		runtimer = (time_t)l;

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

				fclose(fp);
				break;

			default:
				panic("Internal error");
				break;
			}
		}
	}
	closedir(spool);

	for (error = 0, i = 0; i < jobs_len; i++) {
		if (jobs[i] != NULL) {
			if (!force)
				fprintf(stderr, "%s: %s: no such job\n",
				    ProgramName, jobs[i]);
			error++;
		}
	}
	free(jobs);
	free(uids);

	/* If we modied the spool, poke cron so it knows to reload. */
	if (changed) {
		PRIV_START;
		if (chdir(CRONDIR) != 0)
			perror(CRONDIR);
		else
			poke_daemon(AT_DIR, RELOAD_AT);
		PRIV_END;
	}

	return (error);
}

#define	ATOI2(s)	((s) += 2, ((s)[-2] - '0') * 10 + ((s)[-1] - '0'))

/*
 * Adapted from date(1)
 */
static time_t
ttime(char *arg)
{
	time_t now, then;
	struct tm *lt;
	int yearset;
	char *dot, *p;

	if (time(&now) == (time_t)-1 || (lt = localtime(&now)) == NULL)
		panic("Cannot get current time");

	/* Valid date format is [[CC]YY]MMDDhhmm[.SS] */
	for (p = arg, dot = NULL; *p != '\0'; p++) {
		if (*p == '.' && dot != NULL)
			dot = p;
		else if (!isdigit((unsigned char)*p))
			goto terr;
	}
	if (dot == NULL)
		lt->tm_sec = 0;
	else {
		*dot++ = '\0';
		if (strlen(dot) != 2)
			goto terr;
		lt->tm_sec = ATOI2(p);
		if (lt->tm_sec > 61)	/* could be leap second */
			goto terr;
	}

	yearset = 0;
	switch(strlen(arg)) {
	case 12:			/* CCYYMMDDhhmm */
		lt->tm_year = ATOI2(arg);
		lt->tm_year *= 100;
		yearset = 1;
		/* FALLTHROUGH */
	case 10:			/* YYMMDDhhmm */
		if (yearset) {
			yearset = ATOI2(arg);
			lt->tm_year += yearset;
		} else {
			yearset = ATOI2(arg);
			lt->tm_year = yearset + 2000;
		}
		lt->tm_year -= 1900;	/* Convert to Unix time */
		/* FALLTHROUGH */
	case 8:				/* MMDDhhmm */
		lt->tm_mon = ATOI2(arg);
		if (lt->tm_mon > 12 || lt->tm_mon == 0)
			goto terr;
		--lt->tm_mon;		/* Convert from 01-12 to 00-11 */
		lt->tm_mday = ATOI2(arg);
		if (lt->tm_mday > 31 || lt->tm_mday == 0)
			goto terr;
		lt->tm_hour = ATOI2(arg);
		if (lt->tm_hour > 23)
			goto terr;
		lt->tm_min = ATOI2(arg);
		if (lt->tm_min > 59)
			goto terr;
		break;
	default:
		goto terr;
	}

	lt->tm_isdst = -1;		/* mktime will deduce DST. */
	then = mktime(lt);
	if (then == (time_t)-1) {
    terr:
		panic("illegal time specification: [[CC]YY]MMDDhhmm[.SS]");
	}
	if (then < now)
		panic("cannot schedule jobs in the past");
	return (then);
}

static int
check_permission(void)
{
	int ok;
	uid_t uid = geteuid();
	struct passwd *pw;

	if ((pw = getpwuid(uid)) == NULL) {
		perror("Cannot access password database");
		exit(ERROR_EXIT);
	}

	PRIV_START;

	ok = allowed(pw->pw_name, AT_ALLOW, AT_DENY);

	PRIV_END;

	return (ok);
}

static __dead void
usage(void)
{
	/* Print usage and exit.  */
	switch (program) {
	case AT:
	case CAT:
		(void)fprintf(stderr,
		    "usage: at [-bm] [-f file] [-l [user ...]] [-q queue] "
		    "-t time_arg | timespec\n"
		    "       at -c | -r job ...\n");
		break;
	case ATQ:
		(void)fprintf(stderr,
		    "usage: atq [-cnv] [-q queue] [name ...]\n");
		break;
	case ATRM:
		(void)fprintf(stderr,
		    "usage: atrm [-afi] [[job] [name] ...]\n");
		break;
	case BATCH:
		(void)fprintf(stderr,
		    "usage: batch [-m] [-f file] [-q queue] [timespec]\n");
		break;
	}
	exit(ERROR_EXIT);
}

int
main(int argc, char **argv)
{
	time_t timer = -1;
	char queue = DEFAULT_AT_QUEUE;
	char queue_set = 0;
	char *options = "q:f:t:bcdlmrv";	/* default options for at */
	char cwd[PATH_MAX];
	int ch;
	int aflag = 0;
	int cflag = 0;
	int nflag = 0;

	if (argc < 1)
		usage();

	if ((ProgramName = strrchr(argv[0], '/')) != NULL)
		ProgramName++;
	else
		ProgramName = argv[0];

	RELINQUISH_PRIVS;

	/* find out what this program is supposed to do */
	if (strcmp(ProgramName, "atq") == 0) {
		program = ATQ;
		options = "cnvq:";
	} else if (strcmp(ProgramName, "atrm") == 0) {
		program = ATRM;
		options = "afi";
	} else if (strcmp(ProgramName, "batch") == 0) {
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

	if (getcwd(cwd, sizeof(cwd)) == NULL)
		perr("Cannot get current working directory");

	set_cron_cwd();

	if (!check_permission())
		panic("You do not have permission to use at.");

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
		if (timer == -1) {
			if (argc == 0)
				usage();
			else if ((timer = parsetime(argc, argv)) == -1)
				exit(ERROR_EXIT);
		}
		writefile(cwd, timer, queue);
		break;

	case BATCH:
		if (queue_set)
			queue = toupper(queue);
		else
			queue = DEFAULT_BATCH_QUEUE;

		if (argc == 0)
			timer = time(NULL);
		else if ((timer = parsetime(argc, argv)) == -1)
			exit(ERROR_EXIT);

		writefile(cwd, timer, queue);
		break;

	default:
		panic("Internal error");
		break;
	}
	exit(OK_EXIT);
}
