/*	$OpenBSD: login_radius.c,v 1.3 2002/09/29 03:23:01 millert Exp $	*/

/*-
 * Copyright (c) 1996, 1997 Berkeley Software Design, Inc. All rights reserved.
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
 *	BSDI $From: login_radius.c,v 1.4 1998/04/14 00:39:02 prb Exp $
 */
/*
 * Copyright(c) 1996 by tfm associates.
 * All rights reserved.
 *
 * tfm associates
 * P.O. Box 2086
 * Eugene OR 97402-0031
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of tfm associates may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TFM ASSOC``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TFM ASSOCIATES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <netinet/in.h>

#include <ctype.h>
#include <err.h>
#include <login_cap.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <bsd_auth.h>

static int cleanstring(char *);
int raddauth(char *, char *, char *, char *, char *, char **);

int
main(int argc, char **argv)
{
	FILE *back;
	char challenge[1024];
	char *class, *service, *style, *username, *password, *emsg;
	int c, n;
	extern char *__progname;

	back = NULL;
	password = class = service = NULL;

	/*
	 * Usage: login_xxx [-s service] [-v var] user [class]
	 */
	while ((c = getopt(argc, argv, "ds:v:")) != -1)
		switch (c) {
		case 'd':
			back = stdout;
			break;
		case 'v':
			break;
		case 's':       /* service */
			service = optarg;
			if (strcmp(service, "login") != 0 &&
			    strcmp(service, "challenge") != 0 &&
			    strcmp(service, "response") != 0)
				errx(1, "%s not supported", service);
			break;
		default:
			syslog(LOG_ERR, "usage error");
			exit(1);
		}

	if (service == NULL)
		service = LOGIN_DEFSERVICE;

	switch (argc - optind) {
	case 2:
		class = argv[optind + 1];
	case 1:
		username = argv[optind];
		break;
	default:
		syslog(LOG_ERR, "usage error");
		exit(1);
	}

	if ((style = strrchr(__progname, '/')))
		++style;
	else
		style = __progname;

	if (strncmp(style, "login_", 6) == 0)
		style += 6;

	if (!cleanstring(style))
		errx(1, "style contains non-printables");
	if (!cleanstring(username))
		errx(1, "username contains non-printables");
	if (service && !cleanstring(service))
		errx(1, "service contains non-printables");
	if (class && !cleanstring(class))
		errx(1, "class contains non-printables");

	/*
	 * Filedes 3 is the back channel, where we send back results.
	 */
        if (back == NULL && (back = fdopen(3, "a")) == NULL)  {
                syslog(LOG_ERR, "reopening back channel");
                exit(1);
        }

	memset(challenge, 0, sizeof(challenge));

	if (strcmp(service, "response") == 0) {
		c = -1;
		n = 0;
		while (++c < sizeof(challenge) &&
		    read(3, &challenge[c], 1) == 1) {
			if (challenge[c] == '\0' && ++n == 2)
				break;
			if (challenge[c] == '\0' && n == 1)
				password = challenge + c + 1;
		}
		if (n < 2) {
			syslog(LOG_ERR, "protocol error on back channel");
			exit(1);
		}
	}

	emsg = NULL;

	c = raddauth(username, class, style, 
	    strcmp(service, "login") ? challenge : NULL, password, &emsg);

	if (c == 0) {
		if (challenge == NULL || *challenge == '\0')
			(void)fprintf(back, BI_AUTH "\n");
		else {
			(void)fprintf(back, BI_VALUE " challenge %s\n",
			    auth_mkvalue(challenge));
			(void)fprintf(back, BI_CHALLENGE "\n");
		}
		exit(0);
	}
	if (emsg && strcmp(service, "login") == 0)
		(void)fprintf(stderr, "%s\n", emsg);
	else if (emsg)
		(void)fprintf(back, "value errormsg %s\n", auth_mkvalue(emsg));
	if (strcmp(service, "challenge") == 0) {
		(void)fprintf(back, BI_SILENT "\n");
		exit(0);
	}
	(void)fprintf(back, BI_REJECT "\n");
	exit(1);
}

static int
cleanstring(char *s)
{

	while (*s && isgraph(*s) && *s != '\\')
		++s;
	return(*s == '\0' ? 1 : 0);
}
