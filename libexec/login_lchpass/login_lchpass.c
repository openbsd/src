/*	$OpenBSD: login_lchpass.c,v 1.3 2001/10/24 13:06:35 mpech Exp $	*/

/*-
 * Copyright (c) 1995,1996 Berkeley Software Design, Inc. All rights reserved.
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
 *      This product includes software developed by Berkeley Software Design,
 *      Inc.
 * 4. The name of Berkeley Software Design, Inc.  may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI $From: login_lchpass.c,v 1.4 1997/08/08 18:58:23 prb Exp $
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdarg.h>
#include <login_cap.h>

int local_passwd __P((char *, int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	FILE *back;
	struct passwd *pwd;
	char localhost[MAXHOSTNAMELEN];
    	char *username = 0;
	char *salt;
	char *p;
    	int c;
	struct rlimit rl;

	rl.rlim_cur = 0;
	rl.rlim_max = 0;
	(void)setrlimit(RLIMIT_CORE, &rl);

	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)setpriority(PRIO_PROCESS, 0, 0);

	openlog("login", LOG_ODELAY, LOG_AUTH);

	if (gethostname(localhost, sizeof(localhost)) < 0)
		syslog(LOG_ERR, "couldn't get local hostname: %m");

    	while ((c = getopt(argc, argv, "v:s:")) != -1)
		switch(c) {
		case 'v':
			break;
		case 's':	/* service */
			if (strcmp(optarg, "login") != 0) {
				syslog(LOG_ERR, "%s: invalid service", optarg);
				exit(1);
			}
			break;
		default:
			syslog(LOG_ERR, "usage error");
			exit(1);
		}

	switch(argc - optind) {
	case 2:
		/* class is not used */
	case 1:
		username = argv[optind];
		break;
	default:
		syslog(LOG_ERR, "usage error");
		exit(1);
	}

	pwd = getpwnam(username);

	if (!(back = fdopen(3, "a")))  {
		syslog(LOG_ERR, "reopening back channel");
		exit(1);
	}

	if (pwd && *pwd->pw_passwd == '\0') {
		syslog(LOG_ERR, "%s attempting to add password", username);
		fprintf(back, BI_SILENT "\n");
		exit(0);
	}

	if (pwd)
		salt = pwd->pw_passwd;
	else
		salt = "xx";

	(void)setpriority(PRIO_PROCESS, 0, -4);

	printf("Changing local password for %s.\n", pwd->pw_name);
	p = getpass("Old Password:");

	salt = crypt(p, salt);
	memset(p, 0, strlen(p));
	if (!pwd || strcmp(salt, pwd->pw_passwd) != 0)
		exit(1);
	local_passwd(pwd->pw_name, 1);
	fprintf(back, BI_SILENT "\n");
	exit(0);
}
