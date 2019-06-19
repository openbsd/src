/*	$OpenBSD: sysctl.c,v 1.12 2018/06/25 16:29:00 deraadt Exp $	*/

/*
 * Copyright (c) 2009 Theo de Raadt <deraadt@openbsd.org>
 * Copyright (c) 2007 Kenneth R. Westerback <krw@openbsd.org>
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
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <netinet/in.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SOIIKEY_LEN 16

struct var {
	char *name;
	int (*print)(struct var *);
	int nmib;
	int mib[3];
};

int	pstring(struct var *);
int	pint(struct var *);

struct var vars[] = {
	{ "kern.osrelease", pstring, 2,
	    { CTL_KERN, KERN_OSRELEASE }},
	{ "hw.machine", pstring, 2,
	    { CTL_HW, HW_MACHINE }},
	{ "hw.model", pstring, 2,
	    { CTL_HW, HW_MODEL }},
	{ "hw.product", pstring, 2,
	    { CTL_HW, HW_PRODUCT }},
	{ "hw.disknames", pstring, 2,
	    { CTL_HW, HW_DISKNAMES }},
	{ "hw.ncpufound", pint, 2,
	    { CTL_HW, HW_NCPUFOUND }},
};

int	nflag;
char	*name;

int
pint(struct var *v)
{
	int n;
	size_t len = sizeof(n);

	if (sysctl(v->mib, v->nmib, &n, &len, NULL, 0) != -1) {
		if (nflag == 0)
			printf("%s=", v->name);
		printf("%d\n", n);
		return (0);
	}
	return (1);
}

int
pstring(struct var *v)
{
	char *p;
	size_t len;

	if (sysctl(v->mib, v->nmib, NULL, &len, NULL, 0) != -1)
		if ((p = malloc(len)) != NULL)
			if (sysctl(v->mib, v->nmib, p, &len, NULL, 0) != -1) {
				if (nflag == 0)
					printf("%s=", v->name);
				printf("%s\n", p);
				return (0);
			}
	return (1);
}

int
parse_hex_char(char ch)
{
	if (ch >= '0' && ch <= '9')
		return (ch - '0');

	ch = tolower((unsigned char)ch);
	if (ch >= 'a' && ch <= 'f')
		return (ch - 'a' + 10);

	return (-1);
}

int
set_soii_key(char *src)
{
	uint8_t key[SOIIKEY_LEN];
	int mib[4] = {CTL_NET, PF_INET6, IPPROTO_IPV6, IPV6CTL_SOIIKEY};
	int i, c;

	for(i = 0; i < SOIIKEY_LEN; i++) {
		if ((c = parse_hex_char(src[2 * i])) == -1)
			return (-1);
		key[i] = c << 4;
		if ((c = parse_hex_char(src[2 * i + 1])) == -1)
			return (-1);
		key[i] |= c;
	}

	return sysctl(mib, sizeof(mib) / sizeof(mib[0]), NULL, NULL, key,
	    SOIIKEY_LEN);
}

int
main(int argc, char *argv[])
{
	int ch, i;

	while ((ch = getopt(argc, argv, "n")) != -1) {
		switch (ch) {
		case 'n':
			nflag = 1;
			break;
		default:
			fprintf(stderr, "usage: sysctl [-n] name\n");
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		for (i = 0; i < sizeof(vars)/sizeof(vars[0]); i++)
			(vars[i].print)(&vars[i]);
		exit(0);
	}

	while (argc--) {
		name = *argv++;
		/*
		 * strlen("net.inet6.ip6.soiikey="
		 *     "00000000000000000000000000000000") == 54
		 * strlen("net.inet6.ip6.soiikey=") == 22
		 */
		if (strlen(name) == 54 && strncmp(name,
		    "net.inet6.ip6.soiikey=", 22) == 0) {
			set_soii_key(name + 22);
			continue;
		}

		for (i = 0; i < sizeof(vars)/sizeof(vars[0]); i++) {
			if (strcmp(name, vars[i].name) == 0) {
				(vars[i].print)(&vars[i]);
				break;
			}
		}
	}

	exit(0);
}
