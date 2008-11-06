/*	$OpenBSD: pwd_check.c,v 1.12 2008/11/06 05:35:56 djm Exp $	*/

/*
 * Copyright 2000 Niels Provos <provos@citi.umich.edu>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <err.h>
#include <regex.h>
#include <grp.h>
#include <paths.h>
#include <login_cap.h>
#include <signal.h>

int pwd_check(login_cap_t *, char *);
int pwd_gettries(login_cap_t *);

struct pattern {
	char *match;
	int flags;
	char *response;
};

struct pattern patterns[] = {
	{
		"^[0-9]*$",
		REG_EXTENDED|REG_NOSUB,
		"Please don't use all-digit passwords."
	},
	{
		"^[a-z]{1,9}$",
		REG_EXTENDED|REG_NOSUB,
		"Please don't use an all-lower case password."
	},
	{
		"^[a-z]{1,6}[0-9]+$",
		REG_EXTENDED|REG_NOSUB|REG_ICASE,
		"Please use a more complicated password."
	},
	{
		"^([a-z][0-9]){1,4}$",
		REG_EXTENDED|REG_NOSUB|REG_ICASE,
		"Please use a more complicated password."
	},
	{
		"^([0-9][a-z]){1,4}$",
		REG_EXTENDED|REG_NOSUB|REG_ICASE,
		"Please use a more complicated password."
	}
};

int
pwd_check(login_cap_t *lc, char *password)
{
	regex_t rgx;
	int i, res, min_len;
	char *checker;
	char *argp[] = { "sh", "-c", NULL, NULL};
	int pipefds[2];
	pid_t child;
	uid_t uid;
	gid_t gid;

	min_len = (int)login_getcapnum(lc, "minpasswordlen", 6, 6);
	if (min_len > 0 && strlen(password) < min_len) {
		printf("Please enter a longer password.\n");
		return (0);
	}

	/* External password check program */
	checker = login_getcapstr(lc, "passwordcheck", NULL, NULL);

	/* Pipes are only used for external checker */
	if (checker != NULL && pipe(pipefds) == -1) {
		warn("pipe");
		goto out;
	}

	/* Check password in low-privileged child */
	switch (child = fork()) {
	case -1:
		warn("fork");
		goto out;
	case 0:
		(void)signal(SIGINT, SIG_DFL);
		(void)signal(SIGQUIT, SIG_DFL);
		uid = getuid();
		gid = getgid();
		if (setresgid(gid, gid, gid) == -1) {
			warn("setresgid");
			exit(1);
		}
		if (setgroups(1, &gid) == -1) {
			warn("setgroups");
			exit(1);
		}
		if (setresuid(uid, uid, uid) == -1) {
			warn("setresuid");
			exit(1);
		}

		for (i = 0; i < sizeof(patterns) / sizeof(*patterns); i++) {
			if (regcomp(&rgx, patterns[i].match,
			    patterns[i].flags) != 0)
				continue;
			res = regexec(&rgx, password, 0, NULL, 0);
			regfree(&rgx);
			if (res == 0) {
				printf("%s\n", patterns[i].response);
				exit(1);
			}
		}

		/* If no external checker in use, accept the password */
		if (checker == NULL)
			exit(0);

		/* Otherwise, pass control to checker program */
		argp[2] = checker;
		if (dup2(pipefds[0], STDIN_FILENO) == -1) {
			warn("dup2");
			exit(1);
		}
		close(pipefds[0]);
		close(pipefds[1]);

		if (execv(_PATH_BSHELL, argp) == -1) {
			warn("exec");
			exit(1);
		}
		/* NOTREACHED */
	default:
		break; /* parent continues below */
	}

	if (checker != NULL) {
		/* Send the password to STDIN of child */
		close(pipefds[0]);
		write(pipefds[1], password, strlen(password) + 1);
		close(pipefds[1]);
	}

	/* get the return value from the child */
	wait(&child);
	if (WIFEXITED(child) && WEXITSTATUS(child) == 0) {
		if (checker != NULL)
			free(checker);
		return (1);
	}

 out:
	if (checker != NULL)
		free(checker);
	printf("Please use a different password. Unusual capitalization,\n");
	printf("control characters, or digits are suggested.\n");

	return (0);
}

int
pwd_gettries(login_cap_t *lc)
{
	quad_t ntries;

	if ((ntries = login_getcapnum(lc, "passwordtries", -1, -1)) != -1) {
		if (ntries >= 0 && ntries <= INT_MAX)
			return((int)ntries);
		fprintf(stderr,
		    "Warning: pwdtries out of range in /etc/login.conf");
	}

	/*
	 * If no amount of tries is specified, return a default of 3,
	 * meaning that after 3 attempts where the user is foiled by the
	 * password checks, it will no longer be checked and they can set
	 * it to whatever they like.  This is the historic BSD behavior.
	 */
	return (3);
}
