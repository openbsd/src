/*	$OpenBSD: skeyaudit.c,v 1.24 2008/11/12 16:13:46 sobrado Exp $	*/

/*
 * Copyright (c) 1997, 2000, 2003 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <sys/param.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <login_cap.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <skey.h>

void notify(struct passwd *, int, int);
FILE *runsendmail(struct passwd *, int *);
__dead void usage(void);

int
main(int argc, char **argv)
{
	struct passwd *pw;
	struct skey key;
	char *name;
	int ch, left, aflag, iflag, limit;

	aflag = iflag = 0;
	limit = 12;
	while ((ch = getopt(argc, argv, "ail:")) != -1)
		switch(ch) {
		case 'a':
			if (getuid() != 0)
				errx(1, "only root may use the -a flag");
			aflag = 1;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'l':
			errno = 0;
			if ((limit = (int)strtol(optarg, NULL, 10)) == 0)
				errno = ERANGE;
			if (errno) {
				warn("key limit");
				usage();
			}
			break;
		default:
			usage();
	}

	/*
	 * Make sure STDIN_FILENO, STDOUT_FILENO, and STDERR_FILENO are open.
	 * If not, open /dev/null in their place or bail.
	 * If we are in interactive mode, STDOUT_FILENO *must* be open.
	 */
	for (ch = STDIN_FILENO; ch <= STDERR_FILENO; ch++) {
		if (fcntl(ch, F_GETFL, 0) == -1 && errno == EBADF) {
			if (ch == STDOUT_FILENO && iflag)
				exit(1);	/* need stdout for -i */
			if (open(_PATH_DEVNULL, O_RDWR, 0644) == -1)
				exit(1);	/* just bail */
		}
	}

	if (argc - optind > 0)
		usage();

	/* Need key.keyfile zero'd at the very least */
	(void)memset(&key, 0, sizeof(key));

	left = 0;
	if (aflag) {
		while ((ch = skeygetnext(&key)) == 0) {
			left = key.n - 1;
			if ((pw = getpwnam(key.logname)) == NULL)
				continue;
			if (left >= limit)
				continue;
			(void)fclose(key.keyfile);
			key.keyfile = NULL;
			notify(pw, left, iflag);
		}
		if (ch == -1)
			errx(-1, "cannot open %s", _PATH_SKEYDIR);
	} else {
		if ((pw = getpwuid(getuid())) == NULL)
			errx(1, "no passwd entry for uid %u", getuid());
		if ((name = strdup(pw->pw_name)) == NULL)
			err(1, "cannot allocate memory");
		sevenbit(name);

		switch (skeylookup(&key, name)) {
			case 0:		/* Success! */
				left = key.n - 1;
				break;
			case -1:	/* File error */
				errx(1, "cannot open %s/%s", _PATH_SKEYDIR,
				    name);
				break;
			case 1:		/* Unknown user */
				errx(1, "user %s is not listed in %s", name,
				    _PATH_SKEYDIR);
		}
		(void)fclose(key.keyfile);

		if (left < limit)
			notify(pw, left, iflag);
	}
		
	exit(0);
}

void
notify(struct passwd *pw, int seq, int interactive)
{
	static char hostname[MAXHOSTNAMELEN];
	pid_t pid;
	FILE *out;

	/* Only set this once */
	if (hostname[0] == '\0' && gethostname(hostname, sizeof(hostname)) == -1)
		strlcpy(hostname, "unknown", sizeof(hostname));

	if (interactive)
		out = stdout;
	else
		out = runsendmail(pw, &pid);

	if (!interactive)
		(void)fprintf(out,
		   "Auto-Submitted: auto-generated\n"
		   "To: %s\nSubject: IMPORTANT action required\n", pw->pw_name);

	if (seq)
		(void)fprintf(out,
"\nYou are nearing the end of your current S/Key sequence for account\n\
%s on system %s.\n\n\
Your S/Key sequence number is now %d.  When it reaches zero\n\
you will no longer be able to use S/Key to log into the system.\n\n",
pw->pw_name, hostname, seq);
	else
		(void)fprintf(out,
"\nYou are at the end of your current S/Key sequence for account\n\
%s on system %s.\n\n\
At this point you can no longer use S/Key to log into the system.\n\n",
pw->pw_name, hostname);
	(void)fprintf(out,
"Type \"skeyinit -s\" to reinitialize your sequence number.\n\n");

	if (!interactive) {
		(void)fclose(out);
		(void)waitpid(pid, NULL, 0);
	}
}

FILE *
runsendmail(struct passwd *pw, pid_t *pidp)
{
	FILE *fp;
	int pfd[2];
	pid_t pid;

	if (pipe(pfd) < 0)
		return(NULL);

	switch (pid = fork()) {
	case -1:			/* fork(2) failed */
		(void)close(pfd[0]);
		(void)close(pfd[1]);
		return(NULL);
	case 0:				/* In child */
		(void)close(pfd[1]);
		(void)dup2(pfd[0], STDIN_FILENO);
		(void)close(pfd[0]);

		/* Run sendmail as target user not root */
		if (getuid() == 0 &&
		    setusercontext(NULL, pw, pw->pw_uid, LOGIN_SETALL) != 0) {
			warn("cannot set user context");
			_exit(127);
		}

		execl(_PATH_SENDMAIL, "sendmail", "-t", (char *)NULL);
		warn("cannot run \"%s -t\"", _PATH_SENDMAIL);
		_exit(127);
	}

	/* In parent */
	*pidp = pid;
	fp = fdopen(pfd[1], "w");
	(void)close(pfd[0]);

	return(fp);
}

__dead void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s [-ai] [-l limit]\n",
	    __progname);
	exit(1);
}
