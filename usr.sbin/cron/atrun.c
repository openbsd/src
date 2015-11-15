/*	$OpenBSD: atrun.c,v 1.41 2015/11/15 23:24:24 millert Exp $	*/

/*
 * Copyright (c) 2002-2003 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <bitstring.h>		/* for structs.h */
#include <bsd_auth.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <login_cap.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "pathnames.h"
#include "macros.h"
#include "structs.h"
#include "funcs.h"
#include "globals.h"

static void run_job(atjob *, char *);

static int
strtot(const char *nptr, char **endptr, time_t *tp)
{
	long long ll;

	errno = 0;
	ll = strtoll(nptr, endptr, 10);
	if (*endptr == nptr)
		return (-1);
	if (ll < 0 || (errno == ERANGE && ll == LLONG_MAX) || (time_t)ll != ll)
		return (-1);
	*tp = (time_t)ll;
	return (0);
}

/*
 * Scan the at jobs dir and build up a list of jobs found.
 */
int
scan_atjobs(at_db **db, struct timespec *ts)
{
	DIR *atdir = NULL;
	int dfd, queue, pending;
	time_t run_time;
	char *ep;
	at_db *new_db, *old_db = *db;
	atjob *job;
	struct dirent *file;
	struct stat sb;

	if ((dfd = open(_PATH_AT_SPOOL, O_RDONLY|O_DIRECTORY)) == -1) {
		syslog(LOG_ERR, "(CRON) OPEN FAILED (%s)", _PATH_AT_SPOOL);
		return (0);
	}
	if (fstat(dfd, &sb) != 0) {
		syslog(LOG_ERR, "(CRON) FSTAT FAILED (%s)", _PATH_AT_SPOOL);
		close(dfd);
		return (0);
	}
	if (old_db != NULL && old_db->mtime == sb.st_mtime) {
		close(dfd);
		return (0);
	}

	if ((atdir = fdopendir(dfd)) == NULL) {
		syslog(LOG_ERR, "(CRON) OPENDIR FAILED (%s)", _PATH_AT_SPOOL);
		close(dfd);
		return (0);
	}

	if ((new_db = malloc(sizeof(*new_db))) == NULL) {
		closedir(atdir);
		return (0);
	}
	new_db->mtime = sb.st_mtime;	/* stash at dir mtime */
	TAILQ_INIT(&new_db->jobs);

	pending = 0;
	while ((file = readdir(atdir)) != NULL) {
		if (fstatat(dfd, file->d_name, &sb, AT_SYMLINK_NOFOLLOW) != 0 ||
		    !S_ISREG(sb.st_mode))
			continue;

		/*
		 * at jobs are named as RUNTIME.QUEUE
		 * RUNTIME is the time to run in seconds since the epoch
		 * QUEUE is a letter that designates the job's queue
		 */
		if (strtot(file->d_name, &ep, &run_time) == -1)
			continue;
		if (ep[0] != '.' || !isalpha((unsigned char)ep[1]))
			continue;
		queue = (unsigned char)ep[1];

		job = malloc(sizeof(*job));
		if (job == NULL) {
			while ((job = TAILQ_FIRST(&new_db->jobs))) {
				TAILQ_REMOVE(&new_db->jobs, job, entries);
				free(job);
			}
			free(new_db);
			closedir(atdir);
			return (0);
		}
		job->uid = sb.st_uid;
		job->gid = sb.st_gid;
		job->queue = queue;
		job->run_time = run_time;
		TAILQ_INSERT_TAIL(&new_db->jobs, job, entries);
		if (ts != NULL && run_time <= ts->tv_sec)
			pending = 1;
	}
	closedir(atdir);

	/* Free up old at db and install new one */
	if (old_db != NULL) {
		while ((job = TAILQ_FIRST(&old_db->jobs))) {
			TAILQ_REMOVE(&old_db->jobs, job, entries);
			free(job);
		}
		free(old_db);
	}
	*db = new_db;

	return (pending);
}

/*
 * Loop through the at job database and run jobs whose time have come.
 */
void
atrun(at_db *db, double batch_maxload, time_t now)
{
	char atfile[PATH_MAX];
	struct stat sb;
	double la;
	atjob *job, *tjob, *batch = NULL;

	if (db == NULL)
		return;

	TAILQ_FOREACH_SAFE(job, &db->jobs, entries, tjob) {
		/* Skip jobs in the future */
		if (job->run_time > now)
			continue;

		snprintf(atfile, sizeof(atfile), "%s/%lld.%c", _PATH_AT_SPOOL,
		    (long long)job->run_time, job->queue);

		if (lstat(atfile, &sb) != 0 || !S_ISREG(sb.st_mode)) {
			TAILQ_REMOVE(&db->jobs, job, entries);
			free(job);
			continue;		/* disapeared or not a file */
		}

		/*
		 * Pending jobs have the user execute bit set.
		 */
		if (sb.st_mode & S_IXUSR) {
			/* new job to run */
			if (isupper(job->queue)) {
				/* we run one batch job per atrun() call */
				if (batch == NULL ||
				    job->run_time < batch->run_time)
					batch = job;
			} else {
				/* normal at job */
				run_job(job, atfile);
				TAILQ_REMOVE(&db->jobs, job, entries);
				free(job);
			}
		}
	}

	/* Run a single batch job if there is one pending. */
	if (batch != NULL
	    && (batch_maxload == 0.0 ||
	    ((getloadavg(&la, 1) == 1) && la <= batch_maxload))
	    ) {
		snprintf(atfile, sizeof(atfile), "%s/%lld.%c", _PATH_AT_SPOOL,
		    (long long)batch->run_time, batch->queue);
		run_job(batch, atfile);
		TAILQ_REMOVE(&db->jobs, batch, entries);
		free(job);
	}
}

/*
 * Run the specified job contained in atfile.
 */
static void
run_job(atjob *job, char *atfile)
{
	struct stat sb;
	struct passwd *pw;
	login_cap_t *lc;
	auth_session_t *as;
	pid_t pid;
	long nuid, ngid;
	FILE *fp;
	int waiter;
	size_t nread;
	char *cp, *ep, mailto[MAX_UNAME], buf[BUFSIZ];
	int fd, always_mail;
	int output_pipe[2];
	char *nargv[2], *nenvp[1];

	/* Open the file and unlink it so we don't try running it again. */
	if ((fd = open(atfile, O_RDONLY|O_NONBLOCK|O_NOFOLLOW, 0)) < 0) {
		syslog(LOG_ERR, "(CRON) CAN'T OPEN (%s)", atfile);
		return;
	}
	unlink(atfile);

	/* We don't want the atjobs dir in the log messages. */
	if ((cp = strrchr(atfile, '/')) != NULL)
		atfile = cp + 1;

	/* Fork so other pending jobs don't have to wait for us to finish. */
	switch (fork()) {
	case 0:
		/* child */
		break;
	case -1:
		/* error */
		syslog(LOG_ERR, "(CRON) CAN'T FORK (%m)");
		/* FALLTHROUGH */
	default:
		/* parent */
		close(fd);
		return;
	}

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
		syslog(LOG_WARNING, "(CRON) ORPHANED JOB (%s)", atfile);
		_exit(EXIT_FAILURE);
	}
	if (pw->pw_expire && time(NULL) >= pw->pw_expire) {
		syslog(LOG_NOTICE, "(%s) ACCOUNT EXPIRED, JOB ABORTED (%s)",
		    pw->pw_name, atfile);
		_exit(EXIT_FAILURE);
	}

	/* Sanity checks */
	if (fstat(fd, &sb) < 0) {
		syslog(LOG_ERR, "(%s) FSTAT FAILED (%s)", pw->pw_name, atfile);
		_exit(EXIT_FAILURE);
	}
	if (!S_ISREG(sb.st_mode)) {
		syslog(LOG_WARNING, "(%s) NOT REGULAR (%s)", pw->pw_name,
		    atfile);
		_exit(EXIT_FAILURE);
	}
	if ((sb.st_mode & ALLPERMS) != (S_IRUSR | S_IWUSR | S_IXUSR)) {
		syslog(LOG_WARNING, "(%s) BAD FILE MODE (%s)", pw->pw_name,
		    atfile);
		_exit(EXIT_FAILURE);
	}
	if (sb.st_uid != 0 && sb.st_uid != job->uid) {
		syslog(LOG_WARNING, "(%s) WRONG FILE OWNER (%s)", pw->pw_name,
		    atfile);
		_exit(EXIT_FAILURE);
	}
	if (sb.st_nlink > 1) {
		syslog(LOG_WARNING, "(%s) BAD LINK COUNT (%s)", pw->pw_name,
		    atfile);
		_exit(EXIT_FAILURE);
	}

	if ((fp = fdopen(dup(fd), "r")) == NULL) {
		syslog(LOG_ERR, "(CRON) DUP FAILED (%m)");
		_exit(EXIT_FAILURE);
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
	if (errno == ERANGE || (gid_t)ngid > GID_MAX || cp == ep || *ep != '\n')
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
		_exit(EXIT_FAILURE);
	if ((uid_t)nuid != job->uid) {
		syslog(LOG_WARNING, "(%s) UID MISMATCH (%s)", pw->pw_name,
		    atfile);
		_exit(EXIT_FAILURE);
	}
	if ((gid_t)ngid != job->gid) {
		syslog(LOG_WARNING, "(%s) GID MISMATCH (%s)", pw->pw_name,
		    atfile);
		_exit(EXIT_FAILURE);
	}

	/* mark ourselves as different to PS command watchers */
	setproctitle("atrun %s", atfile);

	pipe(output_pipe);	/* child's stdout/stderr */

	/* Fork again, child will run the job, parent will catch output. */
	switch ((pid = fork())) {
	case -1:
		syslog(LOG_ERR, "(CRON) CAN'T FORK (%m)");
		_exit(EXIT_FAILURE);
		/*NOTREACHED*/
	case 0:
		/* Write log message now that we have our real pid. */
		syslog(LOG_INFO, "(%s) ATJOB (%s)", pw->pw_name, atfile);

		/* Connect grandchild's stdin to the at job file. */
		if (lseek(fd, 0, SEEK_SET) < 0) {
			syslog(LOG_ERR, "(CRON) LSEEK (%m)");
			_exit(EXIT_FAILURE);
		}
		if (fd != STDIN_FILENO) {
			dup2(fd, STDIN_FILENO);
			close(fd);
		}

		/* Connect stdout/stderr to the pipe from our parent. */
		if (output_pipe[WRITE_PIPE] != STDOUT_FILENO) {
			dup2(output_pipe[WRITE_PIPE], STDOUT_FILENO);
			close(output_pipe[WRITE_PIPE]);
		}
		dup2(STDOUT_FILENO, STDERR_FILENO);
		close(output_pipe[READ_PIPE]);

		(void) setsid();

		/*
		 * From this point on, anything written to stderr will be
		 * mailed to the user as output.
		 */

		/* Setup execution environment as per login.conf */
		if ((lc = login_getclass(pw->pw_class)) == NULL) {
			warnx("unable to get login class for %s",
			    pw->pw_name);
			syslog(LOG_ERR, "(CRON) CAN'T GET LOGIN CLASS (%s)",
			    pw->pw_name);
			_exit(EXIT_FAILURE);

		}
		if (setusercontext(lc, pw, pw->pw_uid, LOGIN_SETALL)) {
			warn("setusercontext failed for %s", pw->pw_name);
			syslog(LOG_ERR, "(%s) SETUSERCONTEXT FAILED (%m)",
			    pw->pw_name);
			_exit(EXIT_FAILURE);
		}

		/* Run any approval scripts. */
		as = auth_open();
		if (as == NULL || auth_setpwd(as, pw) != 0) {
			warn("auth_setpwd");
			syslog(LOG_ERR, "(%s) AUTH_SETPWD FAILED (%m)",
			    pw->pw_name);
			_exit(EXIT_FAILURE);
		}
		if (auth_approval(as, lc, pw->pw_name, "cron") <= 0) {
			warnx("approval failed for %s", pw->pw_name);
			syslog(LOG_ERR, "(%s) APPROVAL FAILED (cron)",
			    pw->pw_name);
			_exit(EXIT_FAILURE);
		}
		auth_close(as);
		login_close(lc);

		/* If this is a low priority job, nice ourself. */
		if (job->queue > 'b') {
			if (setpriority(PRIO_PROCESS, 0, job->queue - 'b') != 0)
				syslog(LOG_ERR, "(%s) CAN'T NICE (%m)",
				    pw->pw_name);
		}

		(void) signal(SIGPIPE, SIG_DFL);

		/*
		 * Exec /bin/sh with stdin connected to the at job file
		 * and stdout/stderr hooked up to our parent.
		 * The at file will set the environment up for us.
		 */
		nargv[0] = "sh";
		nargv[1] = NULL;
		nenvp[0] = NULL;
		if (execve(_PATH_BSHELL, nargv, nenvp) != 0) {
			warn("unable to execute %s", _PATH_BSHELL);
			syslog(LOG_ERR, "(%s) CAN'T EXEC (%s: %m)", pw->pw_name,
			    _PATH_BSHELL);
			_exit(EXIT_FAILURE);
		}
		break;
	default:
		/* parent */
		break;
	}

	/* Close the atfile's fd and the end of the pipe we don't use. */
	close(fd);
	close(output_pipe[WRITE_PIPE]);

	/* Read piped output (if any) from the at job. */
	if ((fp = fdopen(output_pipe[READ_PIPE], "r")) == NULL) {
		syslog(LOG_ERR, "(%s) FDOPEN (%m)", pw->pw_name);
		(void) _exit(EXIT_FAILURE);
	}
	nread = fread(buf, 1, sizeof(buf), fp);
	if (nread != 0 || always_mail) {
		FILE	*mail;
		pid_t	mailpid;
		size_t	bytes = 0;
		int	status = 0;
		char	mailcmd[MAX_COMMAND];
		char	hostname[HOST_NAME_MAX + 1];

		if (gethostname(hostname, sizeof(hostname)) != 0)
			strlcpy(hostname, "unknown", sizeof(hostname));
		if (snprintf(mailcmd, sizeof mailcmd, MAILFMT,
		    MAILARG) >= sizeof mailcmd) {
			syslog(LOG_ERR, "(%s) ERROR (mailcmd too long)",
			    pw->pw_name);
			(void) _exit(EXIT_FAILURE);
		}
		if (!(mail = cron_popen(mailcmd, "w", pw, &mailpid))) {
			syslog(LOG_ERR, "(%s) POPEN (%s)", pw->pw_name, mailcmd);
			(void) _exit(EXIT_FAILURE);
		}
		fprintf(mail, "From: %s (Atrun Service)\n", pw->pw_name);
		fprintf(mail, "To: %s\n", mailto);
		fprintf(mail, "Subject: Output from \"at\" job\n");
		fprintf(mail, "Auto-Submitted: auto-generated\n");
		fprintf(mail, "\nYour \"at\" job on %s\n\"%s/%s\"\n",
		    hostname, _PATH_AT_SPOOL, atfile);
		fprintf(mail, "\nproduced the following output:\n\n");

		/* Pipe the job's output to sendmail. */
		do {
			bytes += nread;
			fwrite(buf, nread, 1, mail);
		} while ((nread = fread(buf, 1, sizeof(buf), fp)) != 0);

		/*
		 * If the mailer exits with non-zero exit status, log
		 * this fact so the problem can (hopefully) be debugged.
		 */
		if ((status = cron_pclose(mail, mailpid)) != 0) {
			syslog(LOG_NOTICE, "(%s) MAIL (mailed %zu byte%s of "
			    "output but got status 0x%04x)", pw->pw_name,
			    bytes, (bytes == 1) ? "" : "s", status);
		}
	}

	fclose(fp);	/* also closes output_pipe[READ_PIPE] */

	/* Wait for grandchild to die.  */
	for (;;) {
		if (waitpid(pid, &waiter, 0) == -1) {
			if (errno == EINTR)
				continue;
			break;
		} else {
			/*
			if (WIFSIGNALED(waiter) && WCOREDUMP(waiter))
				Debug(DPROC, (", dumped core"))
			*/
			break;
		}
	}
	_exit(EXIT_SUCCESS);

bad_file:
	syslog(LOG_ERR, "(%s) BAD FILE FORMAT (%s)", pw->pw_name, atfile);
	_exit(EXIT_FAILURE);
}
