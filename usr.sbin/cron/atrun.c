/*	$OpenBSD: atrun.c,v 1.6 2003/02/20 20:38:08 millert Exp $	*/

/*
 * Copyright (c) 2002-2003 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND TODD C. MILLER DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL TODD C. MILLER BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if !defined(lint) && !defined(LINT)
static const char rcsid[] = "$OpenBSD: atrun.c,v 1.6 2003/02/20 20:38:08 millert Exp $";
#endif

#include "cron.h"
#include <limits.h>
#include <sys/resource.h>

static void unlink_job(at_db *, atjob *);
static void run_job(atjob *, char *);

#ifndef	UID_MAX
#define	UID_MAX	INT_MAX
#endif
#ifndef	GID_MAX
#define	GID_MAX	INT_MAX
#endif

/*
 * Scan the at jobs dir and build up a list of jobs found.
 */
int
scan_atjobs(at_db *old_db, struct timeval *tv)
{
	DIR *atdir = NULL;
	int cwd, queue, pending;
	long l;
	TIME_T run_time;
	char *ep;
	at_db new_db;
	atjob *job, *tjob;
	struct dirent *file;
	struct stat statbuf;

	Debug(DLOAD, ("[%ld] scan_atjobs()\n", (long)getpid()))

	if (stat(AT_DIR, &statbuf) != 0) {
		log_it("CRON", getpid(), "CAN'T STAT", AT_DIR);
		return (0);
	}

	if (old_db->mtime == statbuf.st_mtime) {
		Debug(DLOAD, ("[%ld] at jobs dir mtime unch, no load needed.\n",
		    (long)getpid()))
		return (0);
	}

	/* XXX - would be nice to stash the crontab cwd */
	if ((cwd = open(".", O_RDONLY, 0)) < 0) {
		log_it("CRON", getpid(), "CAN'T OPEN", ".");
		return (0);
	}

	if (chdir(AT_DIR) != 0 || (atdir = opendir(".")) == NULL) {
		if (atdir == NULL)
			log_it("CRON", getpid(), "OPENDIR FAILED", AT_DIR);
		else
			log_it("CRON", getpid(), "CHDIR FAILED", AT_DIR);
		fchdir(cwd);
		close(cwd);
		return (0);
	}

	new_db.mtime = statbuf.st_mtime;	/* stash at dir mtime */
	new_db.head = new_db.tail = NULL;

	pending = 0;
	while ((file = readdir(atdir))) {
		if (stat(file->d_name, &statbuf) != 0 ||
		    !S_ISREG(statbuf.st_mode))
			continue;

		/*
		 * at jobs are named as RUNTIME.QUEUE
		 * RUNTIME is the time to run in seconds since the epoch
		 * QUEUE is a letter that designates the job's queue
		 */
		l = strtol(file->d_name, &ep, 10);
		if (ep[0] != '.' || !isalpha((unsigned char)ep[1]) || l < 0 ||
		    l >= INT_MAX)
			continue;
		run_time = (TIME_T)l;
		queue = ep[1];
		if (!isalpha(queue))
			continue;

		job = (atjob *)malloc(sizeof(*job));
		if (job == NULL) {
			for (job = new_db.head; job != NULL; ) {
				tjob = job;
				job = job->next;
				free(tjob);
			}
			return (0);
		}
		job->uid = statbuf.st_uid;
		job->gid = statbuf.st_gid;
		job->queue = queue;
		job->run_time = run_time;
		job->prev = new_db.tail;
		job->next = NULL;
		if (new_db.head == NULL)
			new_db.head = job;
		if (new_db.tail != NULL)
			new_db.tail->next = job;
		new_db.tail = job;
		if (tv != NULL && run_time <= tv->tv_sec)
			pending = 1;
	}
	closedir(atdir);

	/* Free up old at db */
	Debug(DLOAD, ("unlinking old at database:\n"))
	for (job = old_db->head; job != NULL; ) {
		Debug(DLOAD, ("\t%ld.%c\n", (long)job->run_time, job->queue))
		tjob = job;
		job = job->next;
		free(tjob);
	}

	/* Change back to the normal cron dir. */
	fchdir(cwd);
	close(cwd);

	/* Install the new database */
	*old_db = new_db;
	Debug(DLOAD, ("scan_atjobs is done\n"))

	return (pending);
}

/*
 * Loop through the at job database and run jobs whose time have come.
 */
void
atrun(at_db *db, double batch_maxload, TIME_T now)
{
	char atfile[MAX_FNAME];
	struct stat statbuf;
	double la;
	atjob *job, *batch;

	Debug(DPROC, ("[%ld] atrun()\n", (long)getpid()))

	for (batch = NULL, job = db->head; job; job = job->next) {
		/* Skip jobs in the future */
		if (job->run_time > now)
			continue;

		snprintf(atfile, sizeof(atfile), "%s/%ld.%c", AT_DIR,
		    (long)job->run_time, job->queue);

		if (stat(atfile, &statbuf) != 0)
			unlink_job(db, job);	/* disapeared */

		if (!S_ISREG(statbuf.st_mode))
			continue;		/* should not happen */

		/*
		 * Pending jobs have the user execute bit set.
		 */
		if (statbuf.st_mode & S_IXUSR) 	{
			/* new job to run */
			if (isupper(job->queue)) {
				/* we run one batch job per atrun() call */
				if (batch == NULL ||
				    job->run_time < batch->run_time)
					batch = job;
			} else {
				/* normal at job */
				run_job(job, atfile);
				unlink_job(db, job);
			}
		}
	}

	/* Run a single batch job if there is one pending. */
	if (batch != NULL
#ifdef HAVE_GETLOADAVG
	    && (batch_maxload == 0.0 ||
	    ((getloadavg(&la, 1) == 1) && la <= batch_maxload))
#endif
	    ) {
		snprintf(atfile, sizeof(atfile), "%s/%ld.%c", AT_DIR,
		    (long)batch->run_time, batch->queue);
		run_job(batch, atfile);
		unlink_job(db, batch);
	}
}

/*
 * Remove the specified at job from the database.
 */
static void
unlink_job(at_db *db, atjob *job)
{
	if (job->prev == NULL)
		db->head = job->next;
	else
		job->prev->next = job->next;

	if (job->next == NULL)
		db->tail = job->prev;
	else
		job->next->prev = job->prev;
}

/*
 * Run the specified job contained in atfile.
 */
static void
run_job(atjob *job, char *atfile)
{
	struct stat statbuf;
	struct passwd *pw;
	pid_t pid;
	long nuid, ngid;
	FILE *fp;
	WAIT_T waiter;
	char *cp, *ep, mailto[MAX_UNAME], buf[BUFSIZ];
	int fd, always_mail;
	int output_pipe[2];
	char *nargv[2], *nenvp[1];

	Debug(DPROC, ("[%ld] run_job('%s')\n", (long)getpid(), atfile))

	/* Open the file and unlink it so we don't try running it again. */
	if ((fd = open(atfile, O_RDONLY|O_NONBLOCK|O_NOFOLLOW, 0)) < OK) {
		log_it("CRON", getpid(), "CAN'T OPEN", atfile);
		return;
	}
	unlink(atfile);

	/* Fork so other pending jobs don't have to wait for us to finish. */
	switch (fork()) {
	case 0:
		/* child */
		break;
	case -1:
		/* error */
		log_it("CRON", getpid(), "error", "can't fork");
		/* FALLTHROUGH */
	default:
		/* parent */
		close(fd);
		return;
	}

	acquire_daemonlock(1);			/* close lock fd */

	/*
	 * We don't want the main cron daemon to wait for our children--
	 * we will do it ourselves via waitpid().
	 */
	(void) signal(SIGCHLD, SIG_DFL);

	/*
	 * Verify the user still exists and their account has not expired.
	 */
	pw = getpwuid(job->uid);
	if (pw == NULL) {
		log_it("CRON", getpid(), "ORPHANED JOB", atfile);
		_exit(ERROR_EXIT);
	}
#if (defined(BSD)) && (BSD >= 199103)
	if (pw->pw_expire && time(NULL) >= pw->pw_expire) {
		log_it(pw->pw_name, getpid(), "ACCOUNT EXPIRED, JOB ABORTED",
		    atfile);
		_exit(ERROR_EXIT);
	}
#endif

	/* Sanity checks */
	if (fstat(fd, &statbuf) < OK) {
		log_it(pw->pw_name, getpid(), "FSTAT FAILED", atfile);
		_exit(ERROR_EXIT);
	}
	if (!S_ISREG(statbuf.st_mode)) {
		log_it(pw->pw_name, getpid(), "NOT REGULAR", atfile);
		_exit(ERROR_EXIT);
	}
	if ((statbuf.st_mode & ALLPERMS) != (S_IRUSR | S_IWUSR | S_IXUSR)) {
		log_it(pw->pw_name, getpid(), "BAD FILE MODE", atfile);
		_exit(ERROR_EXIT);
	}
	if (statbuf.st_uid != 0 && statbuf.st_uid != job->uid) {
		log_it(pw->pw_name, getpid(), "WRONG FILE OWNER", atfile);
		_exit(ERROR_EXIT);
	}
	if (statbuf.st_nlink > 1) {
		log_it(pw->pw_name, getpid(), "BAD LINK COUNT", atfile);
		_exit(ERROR_EXIT);
	}

	if ((fp = fdopen(dup(fd), "r")) == NULL) {
		log_it("CRON", getpid(), "error", "dup(2) failed");
		_exit(ERROR_EXIT);
	}

	/*
	 * Check the at job header for sanity and extract the
	 * uid, gid, mailto user and always_mail flag.
	 *
	 * The header should look like this:
	 * #!/bin/sh
	 * # atrun uid=123 gid=123
	 * # mail                         joeuser 0
	 */
	if (fgets(buf, sizeof(buf), fp) == NULL ||
	    strcmp(buf, "#!/bin/sh\n") != 0 ||
	    fgets(buf, sizeof(buf), fp) == NULL ||
	    strncmp(buf, "# atrun uid=", 12) != 0)
		goto bad_file;

	/* Pull out uid */
	cp = buf + 12;
	errno = 0;
	nuid = strtol(cp, &ep, 10);
	if (errno == ERANGE || (uid_t)nuid > UID_MAX || cp == ep ||
	    strncmp(ep, " gid=", 5) != 0)
		goto bad_file;

	/* Pull out gid */
	cp = ep + 5;
	errno = 0;
	ngid = strtol(cp, &ep, 10);
	if (errno == ERANGE || (uid_t)ngid > GID_MAX || cp == ep || *ep != '\n')
		goto bad_file;

	/* Pull out mailto user (and always_mail flag) */
	if (fgets(buf, sizeof(buf), fp) == NULL ||
	    strncmp(buf, "# mail ", 7) != 0)
		goto bad_file;
	cp = buf + 7;
	while (isspace((unsigned char)*cp))
		cp++;
	ep = cp;
	while (!isspace((unsigned char)*ep) && *ep != '\0')
		ep++;
	if (*ep == '\0' || *ep != ' ' || ep - cp >= sizeof(mailto))
		goto bad_file;
	memcpy(mailto, cp, ep - cp);
	mailto[ep - cp] = '\0';
	always_mail = ep[1] == '1';

	(void)fclose(fp);
	if (!safe_p(pw->pw_name, mailto))
		_exit(ERROR_EXIT);
	if ((uid_t)nuid != job->uid) {
		log_it(pw->pw_name, getpid(), "UID MISMATCH", atfile);
		_exit(ERROR_EXIT);
	}
	if ((gid_t)ngid != job->gid) {
		log_it(pw->pw_name, getpid(), "GID MISMATCH", atfile);
		_exit(ERROR_EXIT);
	}

#ifdef CAPITALIZE_FOR_PS
	/* mark ourselves as different to PS command watchers by upshifting
	 * our program name.  This has no effect on some kernels.
	 * XXX - really want to set proc title to at job name instead
	 */
	/*local*/{
		char *pch;

		for (pch = ProgramName; *pch; pch++)
			*pch = MkUpper(*pch);
	}
#endif /* CAPITALIZE_FOR_PS */

	pipe(output_pipe);	/* child's stdout/stderr */
	
	/* Fork again, child will run the job, parent will catch output. */
	switch ((pid = fork())) {
	case -1:
		log_it("CRON", getpid(), "error", "can't fork");
		_exit(ERROR_EXIT);
		/*NOTREACHED*/
	case 0:
		Debug(DPROC, ("[%ld] grandchild process fork()'ed\n",
			      (long)getpid()))

		/* Write log message now that we have our real pid. */
		log_it(pw->pw_name, getpid(), "ATJOB", atfile);

		/* Close log file (or syslog) */
		log_close();

		/* Connect grandchild's stdin to the at job file. */
		if (lseek(fd, (off_t) 0, SEEK_SET) < 0) {
			perror("lseek");
			_exit(ERROR_EXIT);
		}
		if (fd != STDIN) {
			dup2(fd, STDIN);
			close(fd);
		}

		/* Connect stdout/stderr to the pipe from our parent. */
		if (output_pipe[WRITE_PIPE] != STDOUT) {
			dup2(output_pipe[WRITE_PIPE], STDOUT);
			close(output_pipe[WRITE_PIPE]);
		}
		dup2(STDOUT, STDERR);
		close(output_pipe[READ_PIPE]);

		(void) setsid();

#ifdef LOGIN_CAP
		{
			login_cap_t *lc;
# ifdef BSD_AUTH
			auth_session_t *as;
# endif
			if ((lc = login_getclass(pw->pw_class)) == NULL) {
				fprintf(stderr,
				    "Cannot get login class for %s\n",
				    pw->pw_name);
				_exit(ERROR_EXIT);

			}

			if (setusercontext(lc, pw, pw->pw_uid, LOGIN_SETALL)) {
				fprintf(stderr,
				    "setusercontext failed for %s\n",
				    pw->pw_name);
				_exit(ERROR_EXIT);
			}
# ifdef BSD_AUTH
			as = auth_open();
			if (as == NULL || auth_setpwd(as, pw) != 0) {
				fprintf(stderr, "can't malloc\n");
				_exit(ERROR_EXIT);
			}
			if (auth_approval(as, lc, pw->pw_name, "cron") <= 0) {
				fprintf(stderr, "approval failed for %s\n",
				    pw->pw_name);
				_exit(ERROR_EXIT);
			}
			auth_close(as);
# endif /* BSD_AUTH */
			login_close(lc);
		}
#else
		setgid(pw->pw_gid);
		initgroups(pw->pw_name, pw->pw_gid);
#if (defined(BSD)) && (BSD >= 199103)
		setlogin(pw->pw_name);
#endif
		setuid(pw->pw_uid);

#endif /* LOGIN_CAP */

		chdir("/");		/* at job will chdir to correct place */

		/* If this is a low priority job, nice ourself. */
		if (job->queue > 'b')
			(void)setpriority(PRIO_PROCESS, 0, job->queue - 'b');

#if DEBUGGING
		if (DebugFlags & DTEST) {
			fprintf(stderr,
			    "debug DTEST is on, not exec'ing at job %s\n",
			    atfile);
			_exit(OK_EXIT);
		}
#endif /*DEBUGGING*/

		/*
		 * Exec /bin/sh with stdin connected to the at job file
		 * and stdout/stderr hooked up to our parent.
		 * The at file will set the environment up for us.
		 */
		nargv[0] = "sh";
		nargv[1] = NULL;
		nenvp[0] = NULL;
		if (execve(_PATH_BSHELL, nargv, nenvp) != 0) {
			perror("execve: " _PATH_BSHELL);
			_exit(ERROR_EXIT);
		}
		break;
	default:
		/* parent */
		break;
	}

	Debug(DPROC, ("[%ld] child continues, closing output pipe\n",
	    (long)getpid()))

	/* Close the atfile's fd and the end of the pipe we don't use. */
	close(fd);
	close(output_pipe[WRITE_PIPE]);

	/* Read piped output (if any) from the at job. */
	Debug(DPROC, ("[%ld] child reading output from grandchild\n",
	    (long)getpid()))

	fp = fdopen(output_pipe[READ_PIPE], "r");
	if (always_mail || !feof(fp)) {
		FILE	*mail;
		int	bytes = 0;
		int	status = 0;
		char	mailcmd[MAX_COMMAND];
		char	hostname[MAXHOSTNAMELEN];
		size_t	nread;

		Debug(DPROC|DEXT, ("[%ld] got data from grandchild\n",
		    (long)getpid()))

		if (gethostname(hostname, sizeof(hostname)) != 0)
			strcpy(hostname, "unknown");
		if (snprintf(mailcmd, sizeof mailcmd,  MAILFMT,
		    MAILARG) >= sizeof mailcmd) {
			fprintf(stderr, "mailcmd too long\n");
			(void) _exit(ERROR_EXIT);
		}
		if (!(mail = cron_popen(mailcmd, "w", pw))) {
			perror(mailcmd);
			(void) _exit(ERROR_EXIT);
		}
		fprintf(mail, "From: %s (Atrun Service)\n", pw->pw_name);
		fprintf(mail, "To: %s\n", mailto);
		fprintf(mail, "Subject: Output from \"at\" job\n");
#ifdef MAIL_DATE
		fprintf(mail, "Date: %s\n", arpadate(&StartTime));
#endif /*MAIL_DATE*/
		fprintf(mail, "\nYour \"at\" job on %s\n\"%s/%s\"\n",
		    hostname, CRONDIR, atfile);
		fprintf(mail, "\nproduced the following output:\n\n");

		/* Pipe the job's output to sendmail. */
		while ((nread = fread(buf, 1, sizeof(buf), fp)) > 0) {
			bytes += nread;
			fwrite(buf, nread, 1, mail);
		}

		/*
		 * If the mailer exits with non-zero exit status, log
		 * this fact so the problem can (hopefully) be debugged.
		 */
		Debug(DPROC, ("[%ld] closing pipe to mail\n",
		    (long)getpid()))
		if ((status = cron_pclose(mail)) != 0) {
			snprintf(buf, sizeof(buf), "mailed %d byte%s of output"
			    " but got status 0x%04x\n",
			    bytes, (bytes == 1) ? "" : "s", status);
			log_it(pw->pw_name, getpid(), "MAIL", buf);
		}
	}
	Debug(DPROC, ("[%ld] got EOF from grandchild\n", (long)getpid()))

	fclose(fp);	/* also closes output_pipe[READ_PIPE] */

	/* Wait for grandchild to die.  */
	Debug(DPROC, ("[%ld] waiting for grandchild (%ld) to finish\n",
		      (long)getpid(), (long)pid))
	for (;;) {
		if (waitpid(pid, &waiter, 0) == -1) {
			if (errno == EINTR)
				continue;
			Debug(DPROC,
			    ("[%ld] no grandchild process--mail written?\n",
			    (long)getpid()))
			break;
		} else {
			Debug(DPROC, ("[%ld] grandchild (%ld) finished, status=%04x",
			    (long)getpid(), (long)pid, WEXITSTATUS(waiter)))
			if (WIFSIGNALED(waiter) && WCOREDUMP(waiter))
				Debug(DPROC, (", dumped core"))
			Debug(DPROC, ("\n"))
			break;
		}
	}
	_exit(OK_EXIT);

bad_file:
	log_it(pw->pw_name, getpid(), "BAD FILE FORMAT", atfile);
	_exit(ERROR_EXIT);
}
