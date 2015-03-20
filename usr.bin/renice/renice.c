/*	$OpenBSD: renice.c,v 1.17 2015/03/20 19:42:29 millert Exp $	*/

/*
 * Copyright (c) 2009, 2015 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <unistd.h>

#define	RENICE_NONE		0
#define	RENICE_ABSOLUTE		1
#define	RENICE_INCREMENT	2

struct renice_param {
	int pri;
	short pri_type;
	short id_type;
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
	int ch, id_type = PRIO_PROCESS;
	int pri = 0, pri_type = RENICE_NONE;
	char *ep, *idstr;
	const char *errstr;

	if (argc < 3)
		usage();

	/* Allocate enough space for the worst case. */
	params = p = reallocarray(NULL, argc - 1, sizeof(*params));
	if (params == NULL)
		err(1, NULL);

	/* Backwards compatibility: first arg may be priority. */
	if (isdigit((unsigned char)argv[1][0]) ||
	    ((argv[1][0] == '+' || argv[1][0] == '-') &&
	    isdigit((unsigned char)argv[1][1]))) {
		pri = (int)strtol(argv[1], &ep, 10);
		if (*ep != '\0' || ep == argv[1]) {
			warnx("invalid priority %s", argv[1]);
			usage();
		}
		pri_type = RENICE_ABSOLUTE;
		optind = 2;
	}

	/*
	 * Slightly tricky getopt() usage since it is legal to have
	 * option flags interleaved with arguments.
	 */
	for (;;) {
		if ((ch = getopt(argc, argv, "g:n:p:u:")) != -1) {
			switch (ch) {
			case 'g':
				id_type = PRIO_PGRP;
				idstr = optarg;
				break;
			case 'n':
				pri = (int)strtol(optarg, &ep, 10);
				if (*ep != '\0' || ep == optarg) {
					warnx("invalid increment %s", optarg);
					usage();
				}

				/* Set priority for previous entries? */
				if (pri_type == RENICE_NONE) {
					struct renice_param *pp;
					for (pp = params; pp != p; pp++) {
						pp->pri = pri;
						pp->pri_type = RENICE_INCREMENT;
					}
				}
				pri_type = RENICE_INCREMENT;
				continue;
			case 'p':
				id_type = PRIO_PROCESS;
				idstr = optarg;
				break;
			case 'u':
				id_type = PRIO_USER;
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
		p->id_type = id_type;
		p->pri = pri;
		p->pri_type = pri_type;
		if (id_type == PRIO_USER) {
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
	if (pri_type == RENICE_NONE)
		usage();
	exit(renice(params, p));
}

static int
renice(struct renice_param *p, struct renice_param *end)
{
	int new, old, errors = 0;

	for (; p < end; p++) {
		errno = 0;
		old = getpriority(p->id_type, p->id);
		if (errno) {
			warn("getpriority: %d", p->id);
			errors++;
			continue;
		}
		if (p->pri_type == RENICE_INCREMENT)
			p->pri += old;
		new = p->pri > PRIO_MAX ? PRIO_MAX :
		    p->pri < PRIO_MIN ? PRIO_MIN : p->pri;
		if (setpriority(p->id_type, p->id, new) == -1) {
			warn("setpriority: %d", p->id);
			errors++;
			continue;
		}
		printf("%d: old priority %d, new priority %d\n",
		    p->id, old, new);
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
