/* $OpenBSD: delivery_lmtp.c,v 1.7 2015/01/20 17:37:54 deraadt Exp $ */

/*
 * Copyright (c) 2013 Ashish SHUKLA <ashish.is@lostca.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/un.h>

#include <ctype.h>
#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <paths.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "smtpd.h"
#include "log.h"


/* mda backend */
static void delivery_lmtp_open(struct deliver *);

static int inet_socket(char *);
static int unix_socket(char *);
static char* lmtp_getline(FILE *);

struct delivery_backend delivery_backend_lmtp = {
	 0, delivery_lmtp_open
};

enum lmtp_state {
	 LMTP_BANNER,
	 LMTP_LHLO,
	 LMTP_MAIL_FROM,
	 LMTP_RCPT_TO,
	 LMTP_DATA,
	 LMTP_QUIT,
	 LMTP_BYE
};

static int
inet_socket (char *address)
{
	 int s, n;
	 char *hostname, *servname;
	 struct addrinfo hints;
	 struct addrinfo *result0, *result;

	 servname = strchr(address, ':');
	 if (servname == NULL)
		 errx(1, "invalid address: %s", address);

	 *servname++ = '\0';
	 hostname = address;
	 s = -1;

	 memset(&hints, 0, sizeof(hints));
	 hints.ai_family = PF_UNSPEC;
	 hints.ai_socktype = SOCK_STREAM;
	 hints.ai_flags = AI_NUMERICSERV;

	 n = getaddrinfo(hostname, servname, &hints, &result0);
	 if (n)
		 errx(1, "%s", gai_strerror(n));

	 for (result = result0; s < 0 && result; result = result->ai_next) {
		 if ((s = socket(result->ai_family, result->ai_socktype,
			     result->ai_protocol)) == -1) {
			 warn("socket");
			 continue;
		 }
		 if (connect(s, result->ai_addr, result->ai_addrlen) == -1) {
			 warn("connect");
			 close(s);
			 s = -1;
			 continue;
		 }
		 break;
	 }

	 freeaddrinfo(result0);

	 return s;
}

static int
unix_socket(char *path) {
	 struct sockaddr_un addr;
	 int s;

	 memset(&addr, 0, sizeof(addr));

	 if ((s = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1) {
		 warn("socket");
		 return -1;
	 }

	 addr.sun_family = AF_UNIX;
	 if (strlcpy(addr.sun_path, path, sizeof(addr.sun_path))
	     >= sizeof(addr.sun_path)) {
		 warnx("socket path too long");
		 close(s);
		 return -1;
	 }

	 if (connect(s, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
		 warn("connect");
		 close(s);
		 return -1;
	 }

	 return s;
}

static void
delivery_lmtp_open(struct deliver *deliver)
{
	 char *buffer;
	 char *lbuf;
	 char lhloname[255];
	 int s;
	 FILE	*fp;
	 enum lmtp_state state = LMTP_BANNER;
	 size_t	len;

	 fp = NULL;

	 if (deliver->to[0] == '/')
		 s = unix_socket(deliver->to);
	 else
		 s = inet_socket(deliver->to);

	 if (s == -1 || (fp = fdopen(s, "r+")) == NULL)
		 err(1, "couldn't establish connection");

	 while (!feof(fp) && !ferror(fp) && state != LMTP_BYE) {
		 buffer = lmtp_getline(fp);
		 if (buffer == NULL)
			 err(1, "No input received");

		 switch (state) {
		 case LMTP_BANNER:
			 if (strncmp("220 ", buffer, 4) != 0)
				 errx(1, "Invalid LMTP greeting: %s\n", buffer);
			 gethostname(lhloname, sizeof lhloname );
			 fprintf(fp, "LHLO %s\r\n", lhloname);
			 state = LMTP_LHLO;
			 break;

		 case LMTP_LHLO:
			 if (buffer[0] != '2')
				 errx(1, "LHLO rejected: %s\n", buffer);
			 if (strlen(buffer) < 4)
				 errx(1, "Invalid LMTP LHLO answer: %s\n", buffer);
			 if (buffer[3] == '-')
				 continue; /* multi-line */
			 fprintf(fp, "MAIL FROM:<%s>\r\n", deliver->from);
			 state = LMTP_MAIL_FROM;
			 break;

		 case LMTP_MAIL_FROM:
			 if (buffer[0] != '2')
				 errx(1, "MAIL FROM rejected: %s\n", buffer);
			 fprintf(fp, "RCPT TO:<%s>\r\n", deliver->user);
			 state = LMTP_RCPT_TO;
			 break;
			 
		 case LMTP_RCPT_TO:
			 if (buffer[0] != '2')
				 errx(1, "RCPT TO rejected: %s\n", buffer);
			 fprintf(fp, "DATA\r\n");
			 state = LMTP_DATA;
			 break;
			 
		 case LMTP_DATA:
			 if (buffer[0] != '3')
				 errx(1, "DATA rejected: %s\n", buffer);
			 lbuf = NULL;
			 while ((buffer = fgetln(stdin, &len))) {
				 if (buffer[len- 1] == '\n')
					 buffer[len - 1] = '\0';
				 else {
					 if ((lbuf = malloc(len + 1)) == NULL)
						 err(1, NULL);
					 memcpy(lbuf, buffer, len);
					 lbuf[len] = '\0';
					 buffer = lbuf;
				 }
				 fprintf(fp, "%s%s\r\n",
				     *buffer == '.' ? "." : "", buffer);
			 }
			 free(lbuf);
			 fprintf(fp, ".\r\n");
			 state = LMTP_QUIT;
			 break;

		 case LMTP_QUIT:
			 if (buffer[0] != '2')
				 errx(1, "Delivery error: %s\n", buffer);
			 fprintf(fp, "QUIT\r\n");
			 state = LMTP_BYE;
			 break;

		 default:
			errx(1, "Bogus state %d", state);
		 }
	 }

	 _exit(0);
}

static char*
lmtp_getline(FILE *fp)
{
	char   *buffer;
	size_t	len;
	
	if ((buffer = fgetln(fp, &len)) != NULL) {
		if (len >= 2 && buffer[len-2] == '\r')
			buffer[len-2] = '\0';
		buffer[len-1] = '\0';
	}

	return buffer;
}
