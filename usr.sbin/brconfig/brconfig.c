/*	$OpenBSD: brconfig.c,v 1.1 1999/02/26 17:52:12 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/if_bridge.h>
#include <sys/errno.h>
#include <string.h>
#include <err.h>
#include <sysexits.h>

void usage(void);
int main(int, char **);
int bridge_setflag(int, char *, short);
int bridge_clrflag(int, char *, short);
int bridge_list(int, char *, char *);
int bridge_routes(int, char *, char *);
int bridge_add(int, char *, char *);
int bridge_delete(int, char *, char *);
int bridge_status(int, char *);
int is_bridge(int, char *);
void printb(char *, unsigned short, char *);

/* if_flags bits: borrowed from ifconfig.c */
#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6NOTRAILERS\7RUNNING\10NOARP\
\11PROMISC\12ALLMULTI\13OACTIVE\14SIMPLEX\15LINK0\16LINK1\17LINK2\20MULTICAST"

void
usage() {
	fprintf(stderr,
	    "brconfig bridge_name [up] [down] [add interface_name]\n");
	fprintf(stderr, "\t[delete interface_name] ...\n");
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int error = 0, sock;
	char *brdg;

	if (argc < 2) {
		usage();
		return (EX_USAGE);
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		err(1, "socket");

	argc--; argv++;
	brdg = argv[0];

	if (!is_bridge(sock, brdg))
		return (EX_USAGE);

	if (argc == 1) {
		error = bridge_status(sock, brdg);
		return (error);
	}

	for (argc--, argv++; argc != 0; argc--, argv++) {
		if (strcmp("add", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("add requires an argument");
				return (EX_USAGE);
			}
			error = bridge_add(sock, brdg, argv[0]);
			if (error)
				return (error);
		}
		else if (strcmp("delete", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("delete requires an argument");
				return (EX_USAGE);
			}
			error = bridge_delete(sock, brdg, argv[0]);
			if (error)
				return (error);
		}
		else if (strcmp("list", argv[0]) == 0) {
			error = bridge_list(sock, brdg, "");
			if (error)
				return (error);
		}
		else if (strcmp("up", argv[0]) == 0) {
			error = bridge_setflag(sock, brdg, IFF_UP);
			if (error)
				return (error);
		}
		else if (strcmp("down", argv[0]) == 0) {
			error = bridge_clrflag(sock, brdg, IFF_UP);
			if (error)
				return (error);
		}
		else if (strcmp("link0", argv[0]) == 0) {
			error = bridge_setflag(sock, brdg, IFF_LINK0);
			if (error)
				return (error);
		}
		else if (strcmp("-link0", argv[0]) == 0) {
			error = bridge_clrflag(sock, brdg, IFF_LINK0);
			if (error)
				return (error);
		}
		else if (strcmp("link1", argv[0]) == 0) {
			error = bridge_setflag(sock, brdg, IFF_LINK1);
			if (error)
				return (error);
		}
		else if (strcmp("-link1", argv[0]) == 0) {
			error = bridge_clrflag(sock, brdg, IFF_LINK1);
			if (error)
				return (error);
		}
		else if (strcmp("routes", argv[0]) == 0) {
			error = bridge_routes(sock, brdg, "");
			if (error)
				return (error);
		}
		else if (strcmp("status", argv[0]) == 0) {
			error = bridge_status(sock, brdg);
			if (error)
				return (error);
		}
		else {
			warnx("unrecognized option: %s", argv[0]);
			return (EX_USAGE);
		}
	}

	return (0);
}

int
bridge_setflag(s, brdg, f)
	int s;
	char *brdg;
	short f;
{
	struct ifreq ifr;

	strncpy(ifr.ifr_name, brdg, sizeof ifr.ifr_name);

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("ioctl(SIOCGIFFLAGS)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	ifr.ifr_flags |= f;

	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("ioctl(SIOCSIFFLAGS)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	return (0);
}

int
bridge_clrflag(s, brdg, f)
	int s;
	char *brdg;
	short f;
{
	struct ifreq ifr;

	strncpy(ifr.ifr_name, brdg, sizeof ifr.ifr_name);

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("ioctl(SIOCGIFFLAGS)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	ifr.ifr_flags &= ~(f);

	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("ioctl(SIOCSIFFLAGS)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	return (0);
}

int
bridge_list(sock, brdg, delim)
	int sock;
	char *brdg, *delim;
{
	struct ifbreq req;
	u_int32_t i = 0;
	char buf[sizeof(req.ifsname) + 1];

	while (1) {
		strncpy(req.ifbname, brdg, sizeof(req.ifbname));
		req.index = i;
		if (ioctl(sock, SIOCBRDGIDX, &req) < 0) {
			if (errno == ENOENT)    /* end of list */
				return (0);
			warn("ioctl(SIOCBRDGIDX)");
			return (EX_IOERR);
		}

		bzero(buf, sizeof(buf));
		strncpy(buf, req.ifsname, sizeof(req.ifsname));
		printf("%s%s\n", delim, buf);
		i++;
	}

	return (0);             /* NOTREACHED */
}

int
bridge_add(s, brdg, ifn)
	int s;
	char *brdg, *ifn;
{
	struct ifbreq req;

	strncpy(req.ifbname, brdg, sizeof(req.ifbname));
	strncpy(req.ifsname, ifn, sizeof(req.ifsname));
	if (ioctl(s, SIOCBRDGADD, &req) < 0) {
		warn("ioctl(SIOCADDBRDG)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_delete(s, brdg, ifn)
	int s;
	char *brdg, *ifn;
{
	struct ifbreq req;

	strncpy(req.ifbname, brdg, sizeof(req.ifbname));
	strncpy(req.ifsname, ifn, sizeof(req.ifsname));
	if (ioctl(s, SIOCBRDGDEL, &req) < 0) {
		warn("ioctl(SIOCDELBRDG)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_routes(s, brdg, delim)
	int s;
	char *brdg, *delim;
{
	struct ifbrtreq req;
	u_int32_t i = 0;
	int r = 0;

	while (r == 0) {
		strncpy(req.ifbname, brdg, sizeof(req.ifbname));
		req.index = i;

		r = ioctl(s, SIOCBRDGRT, &req);
		if (r != 0) {
			if (errno == ENOENT || errno == ENETDOWN)
				return (0);
			warn("ioctl(SIOCBRDGRT)");
			return (EX_IOERR);
		}
		printf("%s%s %u %s\n", delim, ether_ntoa(&req.dst),
		    req.age, req.ifsname);
		i++;
	}

	return (0);
}

/*
 * Check to make sure 'brdg' is really a bridge interface.
 */
int
is_bridge(s, brdg)
	int s;
	char *brdg;
{
	struct ifreq ifr;
	struct ifbrtreq req;

	strncpy(ifr.ifr_name, brdg, sizeof ifr.ifr_name);

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("ioctl(SIOCGIFFLAGS)");
		return (0);
	}

	strncpy(req.ifbname, brdg, sizeof(req.ifbname));
	req.index = 0;
	if (ioctl(s, SIOCBRDGRT, (caddr_t)&req) < 0) {
		if (errno == ENOENT || errno == ENETDOWN)
			return (1);
		warn("ioctl(SIOCBRDGRT)");
		return (0);
	}
	return (1);
}

int
bridge_status(s, brdg)
	int s;
	char *brdg;
{
	struct ifreq ifr;
	int err;

	strncpy(ifr.ifr_name, brdg, sizeof ifr.ifr_name);

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("ioctl(SIOCGIFFLAGS)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	printf("%s: ", brdg);
	printb("flags", ifr.ifr_flags, IFFBITS);
	printf("\n");

	printf("\tInterfaces:\n");
	err = bridge_list(s, brdg, "\t\t");
	if (err)
		return (err);

	printf("\tRoutes:\n");
	err = bridge_routes(s, brdg, "\t\t");
	return (err);
}

/*
 * Print a value a la the %b format of the kernel's printf
 * (borrowed from ifconfig.c)
 */
void
printb(s, v, bits)
	char *s;
	char *bits;
	unsigned short v;
{
	register int i, any = 0;
	register char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v);
	else
		printf("%s=%x", s, v);
	bits++;
	if (bits) {
		putchar('<');
		while ((i = *bits++)) {
			if (v & (1 << (i-1))) {
				if (any)
					putchar(',');
				any = 1;
				for (; (c = *bits) > 32; bits++)
					putchar(c);
			} else
				for (; *bits > 32; bits++)
					;
		}
		putchar('>');
	}
}
