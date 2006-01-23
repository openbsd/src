/*	$OpenBSD: net.c,v 1.11 2006/01/23 17:29:22 millert Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Tony Nardo of the Johns Hopkins University/Applied Physics Lab.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
/*static char sccsid[] = "from: @(#)net.c	5.5 (Berkeley) 6/1/90";*/
static const char rcsid[] = "$OpenBSD: net.c,v 1.11 2006/01/23 17:29:22 millert Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <err.h>
#include "finger.h"
#include "extern.h"

void
netfinger(name)
	char *name;
{
	FILE *fp;
	int c, lastc;
	int s;
	char *host;
	struct addrinfo hints, *res0, *res;
	int error;
	char hbuf[NI_MAXHOST];

	lastc = 0;
	if (!(host = strrchr(name, '@')))
		return;
	*host++ = '\0';
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, "finger", &hints, &res0);
	if (error) {
		warnx("%s", gai_strerror(error));
		return;
	}

	s = -1;
	for (res = res0; res; res = res->ai_next) {
		if ((s = socket(res->ai_family, res->ai_socktype,
				res->ai_protocol)) < 0) {
			continue;
		}
		if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
			(void)close(s);
			s = -1;
			continue;
		}

		break;
	}

	if (s < 0) {
		perror("finger");
		freeaddrinfo(res0);
		return;
	}

	/* have network connection; identify the host connected with */
	if (getnameinfo(res->ai_addr, res->ai_addrlen, hbuf, sizeof(hbuf),
	    NULL, 0, NI_NUMERICHOST) != 0) {
		strlcpy(hbuf, "(invalid)", sizeof hbuf);
	}
	(void)printf("[%s/%s]\n", host, hbuf);

	freeaddrinfo(res0);

	/* -l flag for remote fingerd  */
	if (lflag)
		write(s, "/W ", 3);
	/* send the name followed by <CR><LF> */
	(void)write(s, name, strlen(name));
	(void)write(s, "\r\n", 2);

	/*
	 * Read from the remote system; once we're connected, we assume some
	 * data.  If none arrives, we hang until the user interrupts.
	 *
	 * If we see a <CR> or a <CR> with the high bit set, treat it as
	 * a newline; if followed by a newline character, only output one
	 * newline.
	 *
	 * Otherwise, all high bits are stripped; if it isn't printable and
	 * it isn't a space, we can simply set the 7th bit.  Every ASCII
	 * character with bit 7 set is printable.
	 */
	if ((fp = fdopen(s, "r")) != NULL)
		while ((c = getc(fp)) != EOF) {
			c &= 0x7f;
			if (c == '\r') {
				if (lastc == '\r')
					continue;
				c = '\n';
				lastc = '\r';
			} else {
				if (!isprint(c) && !isspace(c))
					c |= 0x40;
				if (lastc != '\r' || c != '\n')
					lastc = c;
				else {
					lastc = '\n';
					continue;
				}
			}
			putchar(c);
		}
	if (lastc != '\n')
		putchar('\n');
	(void)fclose(fp);
}
