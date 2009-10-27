/*	$OpenBSD: renice.c,v 1.15 2009/10/27 23:59:42 deraadt Exp $	*/

/*
 * Copyright (c) 2009 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <sys/time.h>
#include <sys/resource.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct renice_param {
	int pri;
	int type;
	id_t id;
};

int main(int, char **);
static int renice(struct renice_param *, struct renice_param *);
__dead void usage(void);

int
main(int argc, char **argv)
{
	struct renice_param *params, *p;
	struct passwd *pw;
	int ch, type = PRIO_PROCESS;
	int nflag = 0, pri = 0;
	char *ep, *idstr;
	const char *errstr;
	long l;

	if (argc < 3)
		usage();

	/* Allocate enough space for the worst case. */
	params = p = calloc(argc - 1, sizeof(*params));
	if (params == NULL)
		err(1, NULL);

	/* Backwards compatibility: first arg may be priority. */
	if (isdigit((unsigned char)argv[1][0]) ||
	    (argv[1][0] == '-' && isdigit((unsigned char)argv[1][1]))) {
		argv[0] = "-n";
		argc++;
		argv--;
	}

	/*
	 * Slightly tricky getopt() usage since it is legal to have
	 * option flags interleaved with arguments.
	 */
	for (;;) {
		if ((ch = getopt(argc, argv, "g:n:p:u:")) != -1) {
			switch (ch) {
			case 'g':
				type = PRIO_PGRP;
				idstr = optarg;
				break;
			case 'n':
				l = strtol(optarg, &ep, 10);
				if (*ep != '\0' || ep == optarg) {
					warnx("invalid increment %s", optarg);
					usage();
				}
				pri = l > PRIO_MAX ? PRIO_MAX :
				    l < PRIO_MIN ? PRIO_MIN : (int)l;

				/* Set priority for previous entries? */
				if (!nflag) {
					struct renice_param *pp;
					for (pp = params; pp != p; pp++) {
						pp->pri = pri;
					}
				}
				nflag = 1;
				continue;
			case 'p':
				type = PRIO_PROCESS;
				idstr = optarg;
				break;
			case 'u':
				type = PRIO_USER;
				idstr = optarg;
				break;
			default:
				usage();
				break;
			}
		} else {
			idstr = argv[optind++];
			if (idstr == NULL)
				break;
		}
		p->type = type;
		p->pri = pri;
		if (type == PRIO_USER) {
			if ((pw = getpwnam(idstr)) == NULL) {
				uid_t id = strtonum(idstr, 0, UID_MAX, &errstr);
				if (!errstr)
					pw = getpwuid(id);
			}
			if (pw == NULL) {
				warnx("unknown user %s", idstr);
				continue;
			}
			p->id = pw->pw_uid;
		} else {
			p->id = strtonum(idstr, 0, UINT_MAX, &errstr);
			if (errstr) {
				warnx("%s is %s", idstr, errstr);
				continue;
			}
		}
		p++;
	}
	if (!nflag)
		usage();
	exit(renice(params, p));
}

static int
renice(struct renice_param *p, struct renice_param *end)
{
	int old, errors = 0;

	for (; p < end; p++) {
		errno = 0;
		old = getpriority(p->type, p->id);
		if (errno) {
			warn("getpriority: %d", p->id);
			errors++;
			continue;
		}
		if (setpriority(p->type, p->id, p->pri) == -1) {
			warn("setpriority: %d", p->id);
			errors++;
			continue;
		}
		printf("%d: old priority %d, new priority %d\n",
		    p->id, old, p->pri);
	}
	return (errors);
}

__dead void
usage(void)
{
	fprintf(stderr, "usage: renice -n increment [[-g] pgrp ...] "
	    "[[-p] pid ...] [[-u] user ...]\n");
	exit(1);
}
