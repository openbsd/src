/*	$OpenBSD: skeyaudit.c,v 1.15 2003/03/14 04:29:04 millert Exp $	*/

/*
 * Copyright (c) 1997, 2000 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
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

	left = aflag = iflag = 0;
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

	if (argc - optind > 0)
		usage();

	/* Need key.keyfile zero'd at the very least */
	(void)memset(&key, 0, sizeof(key));

	if (aflag) {
		while ((ch = skeygetnext(&key)) == 0) {
			left = key.n - 1;
			if ((pw = getpwnam(key.logname)) == NULL)
				continue;
			if (left >= limit)
				continue;
			notify(pw, left, iflag);
		}
		if (ch == -1)
			errx(-1, "cannot open %s", _PATH_SKEYDIR);
		else
			(void)fclose(key.keyfile);
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

	(void)fclose(out);
	if (!interactive)
		(void)waitpid(pid, NULL, 0);
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
		if (setusercontext(NULL, pw, pw->pw_uid, LOGIN_SETALL) != 0) {
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

	(void)fprintf(stderr, "Usage: %s [-i] [-l limit]\n",
	    __progname);
	exit(1);
}
