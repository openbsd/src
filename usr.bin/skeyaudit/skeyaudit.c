/*	$OpenBSD: skeyaudit.c,v 1.2 1997/07/23 04:31:17 millert Exp $	*/

/*
 * Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *	This product includes software developed by Todd C. Miller.
 * 4. The name of the author may not be used to endorse or promote products
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

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <skey.h>

#include <sys/types.h>
#include <sys/param.h>

extern char *__progname;

void usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct passwd *pw;
	struct skey key;
	int ch, errs, left = 0, iflag = 0, limit = 12;
	char *name, hostname[MAXHOSTNAMELEN];
	FILE *out;

	while ((ch = getopt(argc, argv, "il:")) != -1)
		switch(ch) {
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

	if ((pw = getpwuid(getuid())) == NULL)
		errx(1, "no passwd entry for uid %u", getuid());
	if ((name = strdup(pw->pw_name)) == NULL)
		err(1, "cannot allocate memory");
	sevenbit(name);

	errs = skeylookup(&key, name);
	switch (errs) {
		case 0:		/* Success! */
			left = key.n - 1;
			break;
		case -1:	/* File error */
			/* XXX - _PATH_SKEYFILE should be in paths.h? */
			warnx("cannot open /etc/skeykeys");
			break;
		case 1:		/* Unknown user */
			warnx("%s is not listed in /etc/skeykeys", name);
	}

	setuid(getuid());	/* Run sendmail as user not root */

	if (errs || left >= limit)
		exit(errs);

	if (gethostname(hostname, sizeof(hostname)) == -1)
		strcpy(hostname, "unknown");

	if (iflag) {
		out = stdout;
	} else {
		char cmd[sizeof(_PATH_SENDMAIL) + 3];

		sprintf(cmd, "%s -t", _PATH_SENDMAIL);
		out = popen(cmd, "w");
	}

	if (!iflag)
		(void)fprintf(out,
		    "To: %s\nSubject: IMPORTANT action required\n", name);

	(void)fprintf(out,
"\nYou are nearing the end of your current S/Key sequence for account\n\
%s on system %s.\n\n\
Your S/key sequence number is now %d.  When it reaches zero\n\
you will no longer be able to use S/Key to login into the system.\n\n\
Type \"skeyinit -s\" to reinitialize your sequence number.\n\n",
name, hostname, left - 1);

	if (iflag)
		(void)fclose(out);
	else
		(void)pclose(out);
	
	exit(0);
}

void
usage()
{
	(void)fprintf(stderr, "Usage: %s [-i] [-l limit]\n",
	    __progname);
	exit(1);
}
