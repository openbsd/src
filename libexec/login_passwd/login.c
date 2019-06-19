/*	$OpenBSD: login.c,v 1.19 2018/09/30 13:29:24 ajacoutot Exp $	*/

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
	char pbuf[1024];
	char response[1024];
	char *wheel = NULL, *class = NULL;
	struct passwd *pwd;

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
			else if (strncmp(optarg, "lastchance=", 11) == 0)
				lastchance = (strcmp(optarg + 11, "yes") == 0);
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
		/*FALLTHROUGH*/
	case 1:
		username = argv[optind];
		break;
	default:
		syslog(LOG_ERR, "usage error2");
		exit(1);
	}

	/* get the password hash before pledge(2) or it will return '*' */
	pwd = getpwnam_shadow(username);

	if (pledge("stdio rpath tty id", NULL) == -1) {
		syslog(LOG_ERR, "pledge: %m");
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
		password = readpassphrase("Password:", pbuf, sizeof(pbuf), RPP_ECHO_OFF);
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

	ret = pwd_login(username, password, wheel, lastchance, class, pwd);

	if (password != NULL)
		explicit_bzero(password, strlen(password));
	if (ret != AUTH_OK)
		fprintf(back, BI_REJECT "\n");

	closelog();

	exit(0);
}
