/*	$OpenBSD: atrun.c,v 1.21 2002/05/14 18:05:39 millert Exp $	*/

/*
 *  atrun.c - run jobs queued by at; run with root privileges.
 *  Copyright (C) 1993, 1994 Thomas Koenig
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

#include <sys/fcntl.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include <login_cap.h>
#include <bsd_auth.h>

#define MAIN
#include "privs.h"
#include "pathnames.h"
#include "atrun.h"

#if (MAXLOGNAME-1) > UT_NAMESIZE
#define LOGNAMESIZE UT_NAMESIZE
#else
#define LOGNAMESIZE (MAXLOGNAME-1)
#endif

static const char rcsid[] = "$OpenBSD: atrun.c,v 1.21 2002/05/14 18:05:39 millert Exp $";
static int debug = 0;

static void
perr(const char *a)
{
	if (debug)
		perror(a);
	else
		syslog(LOG_ERR, "%s: %m", a);

	exit(EXIT_FAILURE);
}

static void
perr2(char *a, char *b)
{
	if (debug) {
		(void)fputs(a, stderr);
		perror(b);
	} else
		syslog(LOG_ERR, "%s%s: %m", a, b);

	exit(EXIT_FAILURE);
}

static int
write_string(int fd, const char *a)
{
	return(write(fd, a, strlen(a)));
}

/*
 * Run a file by spawning off a process which redirects I/O,
 * spawns a subshell, then waits for it to complete and spawns
 * another process to send mail to the user.
 */
static void
run_file(const char *filename, uid_t uid, gid_t gid)
{
	pid_t pid;
	int fd_out, fd_in;
	int queue;
	char mailbuf[LOGNAMESIZE + 1], *fmt;
	char *mailname = NULL;
	FILE *stream;
	int send_mail = 0;
	struct stat stbuf;
	off_t size;
	struct passwd *pw;
	int fflags;
	uid_t nuid;
	gid_t ngid;
	login_cap_t *lc;
	auth_session_t *as;

	PRIV_START;

	if (chmod(filename, S_IRUSR) != 0)
		perr("Cannot change file permissions");

	PRIV_END;

	pid = fork();
	if (pid == -1)
		perr("Cannot fork");
	else if (pid != 0)
		return;

	/*
	 * Let's see who we mail to.  Hopefully, we can read it from the
	 * command file; if not, send it to the owner, or, failing that, to
	 * root.
	 */
	pw = getpwuid(uid);
	if (pw == NULL) {
		syslog(LOG_ERR,"Userid %u not found - aborting job %s",
		    uid, filename);
		exit(EXIT_FAILURE);
	}

	as = auth_open();
	if (as == NULL || auth_setpwd(as, pw) != 0) {
		syslog(LOG_ERR,"Unable to allocate memory - aborting job %s",
		    filename);
		exit(EXIT_FAILURE);
	}

	if (pw->pw_expire && time(NULL) >= pw->pw_expire) {
		syslog(LOG_ERR, "Userid %u has expired - aborting job %s",
		    uid, filename);
		exit(EXIT_FAILURE);
	}

	PRIV_START;

	fd_in = open(filename, O_RDONLY | O_NONBLOCK | O_NOFOLLOW, 0);

	PRIV_END;

	if (fd_in < 0)
		perr("Cannot open input file");

	if (fstat(fd_in, &stbuf) == -1)
		perr("Error in fstat of input file descriptor");

	if (!S_ISREG(stbuf.st_mode)) {
		syslog(LOG_ERR, "Job %s is not a regular file - aborting",
		    filename);
		exit(EXIT_FAILURE);
	}
	if (stbuf.st_uid != uid) {
		syslog(LOG_ERR, "Uid mismatch for job %s", filename);
		exit(EXIT_FAILURE);
	}
	if (stbuf.st_nlink != 1) {
		syslog(LOG_ERR, "Bad link count for job %s", filename);
		exit(EXIT_FAILURE);
	}
	if ((stbuf.st_mode & ALLPERMS) != S_IRUSR) {
		syslog(LOG_ERR, "Bad file mode for job %s", filename);
		exit(EXIT_FAILURE);
	}
	if ((fflags = fcntl(fd_in, F_GETFD)) < 0)
		perr("Error in fcntl");

	(void)fcntl(fd_in, F_SETFD, fflags & ~FD_CLOEXEC);

	(void) asprintf(&fmt,
	    "#!/bin/sh\n# atrun uid=%%u gid=%%u\n# mail %%%ds %%d",
	    LOGNAMESIZE);
	if (fmt == NULL) {
		syslog(LOG_ERR, "out of memory - aborting");
		exit(EXIT_FAILURE);
	}
	if ((stream = fdopen(dup(fd_in), "r")) == NULL)
		perr("Error duplicating input file descriptor");
	if (fscanf(stream, fmt, &nuid, &ngid, mailbuf, &send_mail) != 4) {
		syslog(LOG_ERR, "File %s is in wrong format - aborting",
		    filename);
		exit(EXIT_FAILURE);
	}
	(void) fclose(stream);
	free(fmt);
	if (mailbuf[0] == '-') {
		syslog(LOG_ERR, "illegal mail name %s in %s", mailbuf, filename);
		exit(EXIT_FAILURE);
	}
	mailname = mailbuf;
	if (nuid != uid) {
		syslog(LOG_ERR, "Job %s - userid %u does not match file uid %u",
		    filename, nuid, uid);
		exit(EXIT_FAILURE);
	}
	if (ngid != gid) {
		syslog(LOG_ERR, "Job %s - groupid %u does not match file gid %u",
		    filename, ngid, gid);
		exit(EXIT_FAILURE);
	}

	PRIV_START;

	if (chdir(_PATH_ATSPOOL) < 0)
		perr2("Cannot chdir to ", _PATH_ATSPOOL);

	/*
	 * Create a file to hold the output of the job we are about to
	 * run. Write the mail header.
	 */

	if ((fd_out = open(filename,
		    O_WRONLY | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR)) < 0)
		perr("Cannot create output file");

	PRIV_END;

	write_string(fd_out, "To: ");
	write_string(fd_out, mailname);
	write_string(fd_out, "\nSubject: Output from your job ");
	write_string(fd_out, filename);
	write_string(fd_out, "\n\n");
	if (fstat(fd_out, &stbuf) == -1)
		perr("Error in fstat of output file descriptor");
	size = stbuf.st_size;

	(void)close(STDIN_FILENO);
	(void)close(STDOUT_FILENO);
	(void)close(STDERR_FILENO);

	pid = fork();
	if (pid < 0)
		perr("Error in fork");
	else if (pid == 0) {
		char *nul = NULL;
		char **nenvp = &nul;

		/*
		 * Set up things for the child; we want standard input from
		 * the input file, and standard output and error sent to
		 * our output file.
		 */
		if (lseek(fd_in, (off_t) 0, SEEK_SET) < 0)
			perr("Error in lseek");

		if (dup(fd_in) != STDIN_FILENO)
			perr("Error in I/O redirection");

		if (dup(fd_out) != STDOUT_FILENO)
			perr("Error in I/O redirection");

		if (dup(fd_out) != STDERR_FILENO)
			perr("Error in I/O redirection");

		(void)close(fd_in);
		(void)close(fd_out);

		PRIV_START;

		if (chdir(_PATH_ATJOBS) < 0)
			perr2("Cannot chdir to ", _PATH_ATJOBS);

		if ((lc = login_getclass(pw->pw_class)) == NULL)
			perr("Cannot get login class");

		if (setusercontext(lc, pw, pw->pw_uid, LOGIN_SETALL) < 0)
			perr("Cannot set user context");

		if (auth_approval(as, lc, pw->pw_name, "at") <= 0)
			perr2("Approval failure for ", pw->pw_name);

		auth_close(as);
		login_close(lc);

		if (chdir(pw->pw_dir) < 0)
			chdir("/");

		/* First letter indicates requested job priority */
		queue = tolower((unsigned char) *filename);
		if (queue > 'b')
			(void) setpriority(PRIO_PROCESS, 0, queue - 'b');

		if (execle(_PATH_BSHELL, "sh", (char *)NULL, nenvp) != 0)
			perr("Exec failed for /bin/sh");

		PRIV_END;
	}
	/* We're the parent.  Let's wait. */
	(void)close(fd_in);
	(void)close(fd_out);
	waitpid(pid, (int *)NULL, 0);

	/*
	 * Send mail.  Unlink the output file first, so it is deleted
	 * after the run.
	 */
	PRIV_START;

	if (stat(filename, &stbuf) == -1)
		perr("Error in stat of output file");
	if (open(filename, O_RDONLY | O_NOFOLLOW) != STDIN_FILENO)
		perr("Open of jobfile failed");

	(void)unlink(filename);

	PRIV_END;

	if ((stbuf.st_size != size) || send_mail) {
		/* Fork off a child for sending mail */

		PRIV_START;

		if (setusercontext(0, pw, pw->pw_uid, LOGIN_SETALL) < 0)
			perr("Cannot set user context");

		if (chdir(pw->pw_dir))
			chdir("/");

		execl(_PATH_SENDMAIL, "sendmail", "-F", "Atrun Service",
		    "-odi", "-oem", "-t", (char *)NULL);
		perr("Exec failed for mail command");

		PRIV_END;
	}
	exit(EXIT_SUCCESS);
}


int
main(int argc, char **argv)
{
	/*
	 * Browse through  _PATH_ATJOBS, looking for jobs that should be
	 * be executed and/or deleted.  The filename consists of the date
	 * (in seconds since the epoch) followed by a '.' and then the
	 * queue (a letter).  A file which has not been executed yet will
	 * have its execute bit set.  For each file which is to be executed,
	 * run_file() is called, which forks off a child to take care of
	 * I/O redirection, forking off another child for execution and
	 * yet another one, optionally, for sending mail.  Files which
	 * have already run are removed during the next invocation.
	 */
	DIR *spool;
	struct dirent *dirent;
	struct stat stbuf;
	char queue;
	char *ep;
	time_t now, run_time;
	char batch_name[FILENAME_MAX];
	uid_t batch_uid;
	gid_t batch_gid;
	long l;
	int c;
	int run_batch;
	double la, load_avg = ATRUN_MAXLOAD;

	/*
	 * We don't need root privileges all the time; running under uid
	 * and gid nobody is fine except for priviledged operations.
	 */
	RELINQUISH_PRIVS_ROOT(NOBODY_UID, NOBODY_GID);

	openlog("atrun", LOG_PID, LOG_CRON);

	errno = 0;
	while ((c = getopt(argc, argv, "dl:")) != -1) {
		switch (c) {
		case 'l':
			errno = EINVAL;
			load_avg = strtod(optarg, &ep);
			if (*ep != '\0' || (errno == ERANGE &&
			    (load_avg == DBL_MAX || load_avg == DBL_MIN)))
				perr2("bad load average: %s", optarg);
			if (load_avg <= 0.)
				load_avg = ATRUN_MAXLOAD;
			break;

		case 'd':
			debug++;
			break;

		default:
			syslog(LOG_ERR, "unknown option");
			exit(EXIT_FAILURE);
		}
	}

	PRIV_START;

	if (chdir(_PATH_ATJOBS) != 0)
		perr2("Cannot change to ", _PATH_ATJOBS);

	/*
	 * Main loop. Open spool directory for reading and look over all
	 * the files in there. If the filename indicates that the job
	 * should be run and the x bit is set, fork off a child which sets
	 * its user and group id to that of the files and exec a /bin/sh
	 * which executes the shell script. Unlink older files if they
	 * should no longer be run.  For deletion, their r bit has to be
	 * turned on.
	 *
	 * Also, pick the oldest batch job to run, at most one per
	 * invocation of atrun.
	 */
	if ((spool = opendir(".")) == NULL)
		perr2("Cannot read ", _PATH_ATJOBS);

	PRIV_END;

	now = time(NULL);
	run_batch = 0;
	batch_uid = (uid_t) -1;
	batch_gid = (gid_t) -1;
	batch_name[0] = '\0';

	while ((dirent = readdir(spool)) != NULL) {
		PRIV_START;

		if (stat(dirent->d_name, &stbuf) != 0)
			perr2("Cannot stat in ", _PATH_ATJOBS);

		PRIV_END;

		/* We don't want directories */
		if (!S_ISREG(stbuf.st_mode))
			continue;

		l = strtol(dirent->d_name, &ep, 10);
		if (*ep != '.' || !isalpha(*(ep + 1)) || l < 0 || l >= INT_MAX)
			continue;
		run_time = (time_t)l;
		queue = *(ep + 1);

		if ((S_IXUSR & stbuf.st_mode) && (run_time <= now)) {
			if (isupper(queue) &&
			    (strcmp(batch_name, dirent->d_name) > 0)) {
				run_batch = 1;
				(void)strlcpy(batch_name, dirent->d_name,
				    sizeof(batch_name));
				batch_uid = stbuf.st_uid;
				batch_gid = stbuf.st_gid;
			}

			/* The file is executable and old enough */
			if (islower(queue))
				run_file(dirent->d_name, stbuf.st_uid,
				    stbuf.st_gid);
		}

		/* Delete older files */
		if ((run_time < now) && !(S_IXUSR & stbuf.st_mode) &&
		    (S_IRUSR & stbuf.st_mode)) {
			PRIV_START;

			(void)unlink(dirent->d_name);

			PRIV_END;
		}
	}

	/* Run the single batch file, if any */
	if (run_batch && ((getloadavg(&la, 1) == 1) && la < load_avg))
		run_file(batch_name, batch_uid, batch_gid);

	closelog();
	exit(EXIT_SUCCESS);
}
