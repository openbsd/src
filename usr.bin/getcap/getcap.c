/*	$OpenBSD: getcap.c,v 1.1 2005/02/19 22:15:41 millert Exp $	*/

/*
 * Copyright (c) 2005 Todd C. Miller <Todd.Miller@courtesan.com>
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

#ifndef lint
static const char rcsid[] = "$OpenBSD: getcap.c,v 1.1 2005/02/19 22:15:41 millert Exp $";
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum captype {
	boolean,
	number,
	string,
	raw
};

void lookup_cap(char *, char *, enum captype, int);
__dead void usage(void);

int
main(int argc, char *argv[])
{
	int ch, aflag;
	enum captype type;
	char *cp, *buf, *cap = NULL, **pathvec = NULL;
	size_t n;

	aflag = type = 0;
	while ((ch = getopt(argc, argv, "ab:c:f:n:s:")) != -1) {
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'b':
			if (*optarg == '\0')
				usage();
			cap = optarg;
			type = boolean;
			break;
		case 'n':
			if (*optarg == '\0')
				usage();
			cap = optarg;
			type = number;
			break;
		case 's':
			if (*optarg == '\0')
				usage();
			cap = optarg;
			type = string;
			break;
		case 'c':
			if (*optarg == '\0')
				usage();
			cap = optarg;
			type = raw;
			break;
		case 'f':
			if (pathvec != NULL)
				errx(1, "only one -f option may be specified");
			for (n = 1, cp = optarg; (cp = strchr(cp, ':')); n++)
				continue;
			pathvec = calloc(n + 1, sizeof(char *));
			for (n = 0; (pathvec[n] = strsep(&optarg, ":"));) {
				if (*pathvec[n] != '\0')
					n++;
			}
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (pathvec == NULL) {
		warnx("no path specified");
		usage();
	}
	if (!aflag && !argc) {
		warnx("must specify -a or a record name");
		usage();
	}

	if (aflag) {
		while (cgetnext(&buf, pathvec) > 0) {
			lookup_cap(buf, cap, type, 1);
			free(buf);
		}
	} else {
		while (*argv != NULL) {
		    if (cgetent(&buf, pathvec, *argv) != 0)
			    errx(1, "unable to lookup %s", *argv); /* XXX */
		    lookup_cap(buf, cap, type, argc > 1);
		    free(buf);
		    argv++;
		}
	}
	exit(0);
}

void
lookup_cap(char *buf, char *cap, enum captype type, int useprefix)
{
	char *cp, *endp;
	long l;
	int ch, n, prefixlen;

	if (cap == NULL) {
		puts(buf);
		return;
	}

	prefixlen = useprefix ? strcspn(buf, "|:") : 0;

	switch (type) {
	case boolean:
		if (cgetcap(buf, cap, ':') == NULL)
			return;
		printf("%.*s%s%s\n", prefixlen, buf,
		    useprefix ? ": " : "", cap);
		break;
	case number:
		if (cgetnum(buf, cap, &l) == -1)
			return;
		printf("%.*s%s%ld\n", prefixlen, buf,
		    useprefix ? ": " : "", l);
		break;
	case string:
		if ((n = cgetstr(buf, cap, &cp)) == -1)
			return;
		else if (n == -2)
			err(1, NULL);	/* ENOMEM */
		if (cgetstr(buf, cap, &cp) < 0)
			return;
		printf("%.*s%s%s\n", prefixlen, buf,
		    useprefix ? ": " : "", cp);
		break;
	case raw:
		n = strlen(cap) - 1;
		ch = cap[n];
		cap[n] = '\0';
		cp = cgetcap(buf, cap, ch);
		cap[n] = ch;
		if (cp != NULL) {
			if ((endp = strchr(cp, ':')) != NULL)
				printf("%.*s%s%.*s\n", prefixlen, buf,
				    useprefix ? ": " : "", endp - cp, cp);
			else
				printf("%.*s%s%s\n", prefixlen, buf,
				    useprefix ? ": " : "", cp);
		}
		break;
	}
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-b boolean | -c capability | -n number | -s string] -a -f path\n"
	    "       %s [-b boolean | -c capability | -n number | -s string] -f path record ...\n",
	    __progname, __progname);
	exit(1);
}
