/*	$OpenBSD: login.c,v 1.5 2002/09/06 18:45:06 deraadt Exp $	*/

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
 *	BSDI $From: login_passwd.c,v 1.11 1997/08/08 18:58:24 prb Exp $
 */

#include "common.h"

FILE *back = NULL;

int
main(int argc, char **argv)
{
	int opt, mode = 0, ret, lastchance = 0;
	char *username, *password = NULL;
	char response[1024];
	int arg_login = 0, arg_notickets = 0;
	char invokinguser[MAXLOGNAME];
	char *wheel = NULL, *class = NULL;

	invokinguser[0] = '\0';

	setpriority(PRIO_PROCESS, 0, 0);

	openlog(NULL, LOG_ODELAY, LOG_AUTH);

	while ((opt = getopt(argc, argv, "ds:v:")) != -1) {
		switch (opt) {
		case 'd':
			back = stdout;
			break;
		case 's':	/* service */
			if (strcmp(optarg, "login") == 0)
				mode = MODE_LOGIN;
			else if (strcmp(optarg, "challenge") == 0)
				mode = MODE_CHALLENGE;
			else if (strcmp(optarg, "response") == 0)
				mode = MODE_RESPONSE;
			else {
				syslog(LOG_ERR, "%s: invalid service", optarg);
				exit(1);
			}
                        break;
		case 'v':
                        if (strncmp(optarg, "wheel=", 6) == 0)
                                wheel = optarg + 6;
                        else if (strncmp(optarg, "lastchance=", 10) == 0)
				lastchance = (strcmp(optarg + 10, "yes") == 0);
			else if (strcmp(optarg, "login=yes") == 0)
				arg_login = 1;
			else if (strcmp(optarg, "notickets=yes") == 0)
				arg_notickets = 1;
			else if (strncmp(optarg, "invokinguser=", 13) == 0)
				snprintf(invokinguser, sizeof(invokinguser),
				    "%s", &optarg[13]);
			/* Silently ignore unsupported variables */
			break;
		default:
			syslog(LOG_ERR, "usage error1");
			exit(1);
		}
	}

	switch (argc - optind) {
	case 2:
		class = argv[optind + 1];
	case 1:
		username = argv[optind];
		break;
	default:
		syslog(LOG_ERR, "usage error2");
		exit(1);
	}

	if (back == NULL && (back = fdopen(3, "r+")) == NULL) {
		syslog(LOG_ERR, "reopening back channel: %m");
		exit(1);
	}

	/*
	 * Read password, either as from the terminal or if the
	 * response mode is active from the caller program.
	 *
	 * XXX  This is completely ungrokkable, and should be rewritten.
	 */
	switch (mode) {
	case MODE_RESPONSE: {
		int count;
		mode = 0;
		count = -1;
		while (++count < sizeof(response) &&
		      read(3, &response[count], 1) == 1) {
			if (response[count] == '\0' && ++mode == 2)
				break;
			if (response[count] == '\0' && mode == 1) {
				password = response + count + 1;
			}
		}
		if (mode < 2) {
			syslog(LOG_ERR, "protocol error on back channel");
			exit(1);
		}
		break;
	}

	case MODE_LOGIN:
		password = getpass("Password:");
		break;
	case MODE_CHALLENGE:
		fprintf(back, BI_AUTH "\n");
		exit(0);
		break;
	default:
		syslog(LOG_ERR, "%d: unknown mode", mode);
		exit(1);
		break;
	}

	ret = AUTH_FAILED;
#ifdef KRB4
	ret = krb4_login(username, password, invokinguser, !arg_notickets);
#endif
#ifdef KRB5
	ret = krb5_login(username, invokinguser, password, arg_login,
			 !arg_notickets);
#endif
#ifdef PASSWD
	if (ret != AUTH_OK)
		ret = pwd_login(username, password, wheel, lastchance, class);
#endif

	memset(password, 0, strlen(password));
	if (ret != AUTH_OK)
		fprintf(back, BI_REJECT "\n");

	closelog();

	exit(0);
}
