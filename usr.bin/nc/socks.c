/*	$OpenBSD: socks.c,v 1.6 2002/12/30 17:55:25 stevesk Exp $	*/

/*
 * Copyright (c) 1999 Niklas Hallqvist.  All rights reserved.
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
 *	This product includes software developed by Niklas Hallqvist.
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SOCKS_PORT	"1080"
#define SOCKS_V5	5
#define SOCKS_V4	4
#define SOCKS_NOAUTH	0
#define SOCKS_NOMETHOD	0xff
#define SOCKS_CONNECT	1
#define SOCKS_IPV4	1
#define SOCKS_MAXCMDSZ	10

int	remote_connect(char *, char *, struct addrinfo);

static in_addr_t
decode_addr (const char *s)
{
	struct hostent *hp = gethostbyname (s);
	struct in_addr retval;

	if (hp)
		return *(in_addr_t *)hp->h_addr_list[0];
	if (inet_aton (s, &retval))
		return retval.s_addr;
	errx (1, "cannot decode address \"%s\"", s);
}

static in_port_t
decode_port (const char *s)
{
	struct servent *sp;
	in_port_t port;
	char *p;

	port = strtol (s, &p, 10);
	if (s == p) {
		sp = getservbyname (s, "tcp");
		if (sp)
			return sp->s_port;
	}
	if (*s != '\0' && *p == '\0')
		return htons (port);
	errx (1, "cannot decode port \"%s\"", s);
}

int
socks_connect (char *host, char *port, struct addrinfo hints,
    char *proxyhost, char *proxyport, struct addrinfo proxyhints,
    int socksv)
{
	int proxyfd;
	unsigned char buf[SOCKS_MAXCMDSZ];
	ssize_t cnt;
	in_addr_t serveraddr;
	in_port_t serverport;

	if (proxyport)
		proxyfd = remote_connect(proxyhost, proxyport, proxyhints);
	else
		proxyfd = remote_connect(proxyhost, SOCKS_PORT, proxyhints);

	if (proxyfd < 0)
		return -1;

	serveraddr = decode_addr (host);
	serverport = decode_port (port);

	if (socksv == 5) {
		/* Version 5, one method: no authentication */
		buf[0] = SOCKS_V5;
		buf[1] = 1;
		buf[2] = SOCKS_NOAUTH;
		cnt = write (proxyfd, buf, 3);
		if (cnt == -1)
			err (1, "write failed");
		if (cnt != 3)
			errx (1, "short write, %d (expected 3)", cnt);

		read (proxyfd, buf, 2);
		if (buf[1] == SOCKS_NOMETHOD)
			errx (1, "authentication method negotiation failed");

		/* Version 5, connect: IPv4 address */
		buf[0] = SOCKS_V5;
		buf[1] = SOCKS_CONNECT;
		buf[2] = 0;
		buf[3] = SOCKS_IPV4;
		memcpy (buf + 4, &serveraddr, sizeof serveraddr);
		memcpy (buf + 8, &serverport, sizeof serverport);

		/* XXX Handle short writes better */
		cnt = write (proxyfd, buf, 10);
		if (cnt == -1)
			err (1, "write failed");
		if (cnt != 10)
			errx (1, "short write, %d (expected 10)", cnt);

		/* XXX Handle short reads better */
		cnt = read (proxyfd, buf, sizeof buf);
		if (cnt == -1)
			err (1, "read failed");
		if (cnt != 10)
			errx (1, "unexpected reply size %d (expected 10)", cnt);
		if (buf[1] != 0)
			errx (1, "connection failed, SOCKS error %d", buf[1]);
	} else {
		/* Version 4 */
		buf[0] = SOCKS_V4;
		buf[1] = SOCKS_CONNECT;	/* connect */
		memcpy (buf + 2, &serverport, sizeof serverport);
		memcpy (buf + 4, &serveraddr, sizeof serveraddr);
		buf[8] = 0;	/* empty username */

		cnt = write (proxyfd, buf, 9);
		if (cnt == -1)
			err (1, "write failed");
		if (cnt != 9)
			errx (1, "short write, %d (expected 9)", cnt);

		/* XXX Handle short reads better */
		cnt = read (proxyfd, buf, 8);
		if (cnt == -1)
			err (1, "read failed");
		if (cnt != 8)
			errx (1, "unexpected reply size %d (expected 8)", cnt);
		if (buf[1] != 90)
			errx (1, "connection failed, SOCKS error %d", buf[1]);
	}

	return proxyfd;
}
