/*
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: gxbn_test.c,v 1.10 2001/01/09 21:41:06 bwelling Exp $ */

#include <config.h>

#include <stdio.h>

#include <isc/net.h>

#include <lwres/netdb.h>

static void
print_he(struct hostent *he, int error, const char *fun, const char *name) {
	char **c;
	int i;

	if (he != NULL) {
		 printf("%s(%s):\n", fun, name);
		 printf("\tname = %s\n", he->h_name);
		 printf("\taddrtype = %d\n", he->h_addrtype);
		 printf("\tlength = %d\n", he->h_length);
		 c = he->h_aliases;
		 i = 1;
		 while (*c != NULL) {
			printf("\talias[%d] = %s\n", i, *c);
			i++;
			c++;
		 }
		 c = he->h_addr_list;
		 i = 1;
		 while (*c != NULL) {
			char buf[128];
			inet_ntop(he->h_addrtype, *c, buf, sizeof (buf));
			printf("\taddress[%d] = %s\n", i, buf);
			c++;
			i++;
		}
	} else {
		printf("%s(%s): error = %d (%s)\n", fun, name, error,
		       hstrerror(error));
	}
}

int
main(int argc, char **argv) {
	struct hostent *he;
	int error;

	(void)argc;

	while (argv[1] != NULL) {
		he = gethostbyname(argv[1]);
		print_he(he, h_errno, "gethostbyname", argv[1]);

		he = getipnodebyname(argv[1], AF_INET6, AI_DEFAULT|AI_ALL,
				     &error);
		print_he(he, error, "getipnodebyname", argv[1]);
		if (he != NULL)
			freehostent(he);

		he = getipnodebyname(argv[1], AF_INET6, AI_DEFAULT,
				     &error);
		print_he(he, error, "getipnodebyname", argv[1]);
		if (he != NULL)
			freehostent(he);
		argv++;
	}
	return (0);
}
