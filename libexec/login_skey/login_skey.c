/*	$OpenBSD: login_skey.c,v 1.5 2001/12/06 05:37:04 millert Exp $	*/

/*-
 * Copyright (c) 1995 Berkeley Software Design, Inc. All rights reserved.
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
 *	BSDI $From: login_skey.c,v 1.3 1996/09/04 05:24:56 prb Exp $
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <ctype.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <readpassphrase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <login_cap.h>
#include <bsd_auth.h>
#include <sha1.h>
#include <skey.h>

void timedout __P((int));

int
main(argc, argv)
	int argc;
	char **argv;
{
	FILE *back = NULL;
    	char *class = 0;
    	char *username = 0;
	struct skey skey;
	char skeyprompt[SKEY_MAX_CHALLENGE+17];
	char passbuf[SKEY_MAX_PW_LEN+1];
	int c, haskey;
	int mode = 0;

	skeyprompt[0] = '\0';

	(void)signal(SIGALRM, timedout);
	(void)setpriority(PRIO_PROCESS, 0, 0);

	openlog(NULL, LOG_ODELAY, LOG_AUTH);

    	while ((c = getopt(argc, argv, "ds:v:")) != -1)
		switch(c) {
		case 'd':	/* to remain undocumented */
			back = stdout;
			break;
		case 'v':
			break;
		case 's':	/* service */
			if (strcmp(optarg, "login") == 0)
				mode = 0;
			else if (strcmp(optarg, "challenge") == 0)
				mode = 1;
			else if (strcmp(optarg, "response") == 0)
				mode = 2;
			else {
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
		class = argv[optind + 1];
	case 1:
		username = argv[optind];
		break;
	default:
		syslog(LOG_ERR, "usage error");
		exit(1);
	}


	if (back == NULL && (back = fdopen(3, "r+")) == NULL)  {
		syslog(LOG_ERR, "reopening back channel: %m");
		exit(1);
	}

	if (mode == 2) {
		mode = 0;
		c = -1;
		/* XXX - redo these loops! */
		while (++c < sizeof(skeyprompt) &&
		    read(3, &skeyprompt[c], 1) == 1) {
			if (skeyprompt[c] == '\0') {
				mode++;
				break;
			}
		}
		if (mode == 1) {
			c = -1;
			while (++c < sizeof(passbuf) &&
			    read(3, &passbuf[c], 1) == 1) {
				if (passbuf[c] == '\0') {
					mode++;
					break;
				}
			}
		}
		if (mode < 2) {
			syslog(LOG_ERR, "protocol error on back channel");
			exit(1);
		}
		/*
		 * Sigh.  S/Key really is a stateful protocol.
		 * We must assume that a user will only try to
		 * authenticate one at a time and that this call to
		 * skeychallenge will produce the same results as
		 * the call to skeychallenge when mode was 1.
		 *
		 * Furthermore, RFC2289 requires that an entry be
		 * locked against a partial guess race which is
		 * simply not possible if the calling program queries
		 * the user for the passphrase itself.  Oh well.
		 */
		haskey = (skeychallenge(&skey, username, skeyprompt) == 0);
	} else {
		/*
		 * Attempt an S/Key challenge.
		 * The OpenBSD skeychallenge() will always fill in a
		 * challenge, even if it has to cons one up.
		 */
		haskey = (skeychallenge(&skey, username, skeyprompt) == 0);
		strcat(skeyprompt, "\nS/Key Password: ");
		if (mode == 1) {
			fprintf(back, BI_VALUE " challenge %s\n",
			    auth_mkvalue(skeyprompt));
			fprintf(back, BI_CHALLENGE "\n");
			exit(0);
		}

		/* Time out getting passphrase after 2 minutes to avoid a DoS */
		if (haskey)
			alarm(120);
		readpassphrase(skeyprompt, passbuf, sizeof(passbuf), 0);
		if (passbuf[0] == '\0')
			readpassphrase("S/Key Password [echo on]: ",
			    passbuf, sizeof(passbuf), RPP_ECHO_ON);
		alarm(0);
	}

	if (haskey && skeyverify(&skey, passbuf) == 0) {
		if (mode == 0) {
			if (skey.n <= 1)
				printf("Warning! You MUST change your "
				    "S/Key password now!\n");
			else if (skey.n < 5)
				printf("Warning! Change S/Key password soon\n");
		}
		fprintf(back, BI_AUTH "\n");
		fprintf(back, BI_SECURE "\n");
		exit(0);
	}
	fprintf(back, BI_REJECT "\n");
	exit(1);
}

void
timedout(signo)
	int signo;
{

	_exit(1);
}
