/*	$OpenBSD: ethers.c,v 1.5 1998/03/17 06:16:55 millert Exp $	*/

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
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

/* 
 * ethers(3) a la Sun.
 * Originally Written by Roland McGrath <roland@frob.com> 10/14/93.
 * Substantially modified by Todd C. Miller <Todd.Miller@courtesan.com>
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: ethers.c,v 1.5 1998/03/17 06:16:55 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <sys/param.h>
#include <paths.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef _PATH_ETHERS
#define _PATH_ETHERS	"/etc/ethers"
#endif

static char * _ether_aton __P((char *, struct ether_addr *));

char *
ether_ntoa(e)
	struct ether_addr *e;
{
	static char a[] = "xx:xx:xx:xx:xx:xx";

	if (e->ether_addr_octet[0] > 0xFF || e->ether_addr_octet[1] > 0xFF ||
	    e->ether_addr_octet[2] > 0xFF || e->ether_addr_octet[3] > 0xFF ||
	    e->ether_addr_octet[4] > 0xFF || e->ether_addr_octet[5] > 0xFF)
		return (NULL);

	(void)sprintf(a, "%02x:%02x:%02x:%02x:%02x:%02x",
	    e->ether_addr_octet[0], e->ether_addr_octet[1],
	    e->ether_addr_octet[2], e->ether_addr_octet[3],
	    e->ether_addr_octet[4], e->ether_addr_octet[5]);

	return (a);
}

static char *
_ether_aton(s, e)
	char *s;
	struct ether_addr *e;
{
	int i;
	long l;
	char *pp;

	while (isspace(*s))
		s++;

	/* expect 6 hex octets separated by ':' or space/NUL if last octet */
	for (i = 0; i < 6; i++) {
		l = strtol(s, &pp, 16);
		if (pp == s || l > 0xFF)
			return (NULL);
		if (!(*pp == ':' || (i == 5 && (isspace(*pp) || *pp == '\0'))))
			return (NULL);
		e->ether_addr_octet[i] = (u_char)l;
		s = pp + 1;
	}

	/* return character after the octets ala strtol(3) */
	return (pp);
}

struct ether_addr *
ether_aton(s)
	char *s;
{
	static struct ether_addr n;

	return (_ether_aton(s, &n) ? &n : NULL);
}

int
ether_ntohost(hostname, e)
	char *hostname;
	struct ether_addr *e;
{
	FILE *f; 
	char buf[BUFSIZ+1], *p;
	size_t len;
	struct ether_addr try;

	if (e->ether_addr_octet[0] > 0xFF || e->ether_addr_octet[1] > 0xFF ||
	    e->ether_addr_octet[2] > 0xFF || e->ether_addr_octet[3] > 0xFF ||
	    e->ether_addr_octet[4] > 0xFF || e->ether_addr_octet[5] > 0xFF)
		return (NULL);

#ifdef YP
	char trybuf[sizeof("xx:xx:xx:xx:xx:xx")];
	int trylen;

	sprintf(trybuf, "%x:%x:%x:%x:%x:%x", 
	    e->ether_addr_octet[0], e->ether_addr_octet[1],
	    e->ether_addr_octet[2], e->ether_addr_octet[3],
	    e->ether_addr_octet[4], e->ether_addr_octet[5]);
	trylen = strlen(trybuf);
#endif

	f = fopen(_PATH_ETHERS, "r");
	if (f == NULL)
		return (-1);
	while ((p = fgetln(f, &len)) != NULL) {
		if (p[len-1] == '\n')
			len--;
		if (len > sizeof(buf) - 2)
			continue;
		(void)memcpy(buf, p, len);
		buf[len] = '\n';	/* code assumes newlines later on */
		buf[len+1] = '\0';
#ifdef YP
		/* A + in the file means try YP now.  */
		if (!strncmp(buf, "+\n", sizeof(buf))) {
			char *ypbuf, *ypdom;
			int ypbuflen;

			if (yp_get_default_domain(&ypdom))
				continue;
			if (yp_match(ypdom, "ethers.byaddr", trybuf,
			    trylen, &ypbuf, &ypbuflen))
				continue;
			if (ether_line(ypbuf, &try, hostname) == 0) {
				free(ypbuf);
				(void)fclose(f);
				return (0);
			}
			free(ypbuf);
			continue;
		}
#endif
		if (ether_line(buf, &try, hostname) == 0 &&
		    memcmp((void *)&try, (void *)e, sizeof(try)) == 0) {
			(void)fclose(f);
			return (0);
		}     
	}
	(void)fclose(f);
	errno = ENOENT;
	return (-1);
}

int
ether_hostton(hostname, e)
	char *hostname;
	struct ether_addr *e;
{
	FILE *f;
	char buf[BUFSIZ+1], *p;
	char try[MAXHOSTNAMELEN];
	size_t len;
#ifdef YP
	int hostlen = strlen(hostname);
#endif

	f = fopen(_PATH_ETHERS, "r");
	if (f==NULL)
		return (-1);

	while ((p = fgetln(f, &len)) != NULL) {
		if (p[len-1] == '\n')
			len--;
		if (len > sizeof(buf) - 2)
			continue;
		memcpy(buf, p, len);
		buf[len] = '\n';	/* code assumes newlines later on */
		buf[len+1] = '\0';
#ifdef YP
		/* A + in the file means try YP now.  */
		if (!strncmp(buf, "+\n", sizeof(buf))) {
			char *ypbuf, *ypdom;
			int ypbuflen;

			if (yp_get_default_domain(&ypdom))
				continue;
			if (yp_match(ypdom, "ethers.byname", hostname, hostlen,
			    &ypbuf, &ypbuflen))
				continue;
			if (ether_line(ypbuf, e, try) == 0) {
				free(ypbuf);
				(void)fclose(f);
				return (0);
			}
			free(ypbuf);
			continue;
		}
#endif
		if (ether_line(buf, e, try) == 0 && strcmp(hostname, try) == 0) {
			(void)fclose(f);
			return (0);
		}
	}
	(void)fclose(f);
	errno = ENOENT;
	return (-1);
}

int
ether_line(line, e, hostname)
	char *line;
	struct ether_addr *e;
	char *hostname;
{
	char *p;
	size_t n;

	/* Parse "xx:xx:xx:xx:xx:xx" */
	if ((p = _ether_aton(line, e)) == NULL || (*p != ' ' && *p != '\t'))
		goto bad;

	/* Now get the hostname */
	while (isspace(*p))
		p++;
	if (*p == '\0')
		goto bad;
	n = strcspn(p, " \t\n");
	if (n >= MAXHOSTNAMELEN)
		goto bad;
	(void)strncpy(hostname, p, n);
	hostname[n] = '\0';
	return (0);

bad:
	errno = EINVAL;
	return (-1);
}
