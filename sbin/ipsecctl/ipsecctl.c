/*	$OpenBSD: ipsecctl.c,v 1.4 2005/04/12 06:57:36 deraadt Exp $	*/
/*
 * Copyright (c) 2004, 2005 Hans-Joerg Hoexer <hshoexer@openbsd.org>
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
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <net/pfkeyv2.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip_ipsp.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "ipsecctl.h"

int		 ipsecctl_rules(char *, int);
FILE		*ipsecctl_fopen(const char *, const char *);
int		 ipsecctl_commit(struct ipsecctl *);
int		 ipsecctl_add_rule(struct ipsecctl *, struct ipsec_rule *);
void		 ipsecctl_print_addr(struct ipsec_addr *);
void		 ipsecctl_print_rule(struct ipsec_rule *, int);
int		 ipsecctl_flush(int);
void		 ipsecctl_get_rules(struct ipsecctl *);
void		 ipsecctl_show(int);
void		 usage(void);

const char	*infile;	/* Used by parse.y */

int
ipsecctl_rules(char *filename, int opts)
{
	FILE		*fin;
	struct ipsecctl	 ipsec;
	int		 error = 0;

	memset(&ipsec, 0, sizeof(ipsec));
	ipsec.opts = opts;
	TAILQ_INIT(&ipsec.rule_queue);

	if (strcmp(filename, "-") == 0) {
		fin = stdin;
		infile = "stdin";
	} else {
		if ((fin = ipsecctl_fopen(filename, "r")) == NULL) {
			warn("%s", filename);
			return 1;
		}
		infile = filename;
	}

	if (parse_rules(fin, &ipsec) < 0) {
		warnx("Syntax error in config file: ipsec rules not loaded");
		error = 1;
	}
	if (((opts & IPSECCTL_OPT_NOACTION) == 0) && (error == 0))
		if (ipsecctl_commit(&ipsec))
			err(1, NULL);

	return error;
}

FILE *
ipsecctl_fopen(const char *name, const char *mode)
{
	struct stat	 st;
	FILE		*fp;

	fp = fopen(name, mode);
	if (fp == NULL)
		return NULL;

	if (fstat(fileno(fp), &st)) {
		fclose(fp);
		return NULL;
	}
	if (S_ISDIR(st.st_mode)) {
		fclose(fp);
		errno = EISDIR;
		return NULL;
	}
	return fp;
}

int
ipsecctl_commit(struct ipsecctl *ipsec)
{
	struct ipsec_rule *rp;

	if (pfkey_init() == -1)
		errx(1, "failed to open PF_KEY socket");

	while ((rp = TAILQ_FIRST(&ipsec->rule_queue))) {
		TAILQ_REMOVE(&ipsec->rule_queue, rp, entries);

		if (pfkey_ipsec_establish(rp) == -1)
			warnx("failed to add rule %d", rp->nr);

		free(rp->src);
		free(rp->dst);
		free(rp->peer);
		if (rp->auth.srcid)
			free(rp->auth.srcid);
		if (rp->auth.dstid)
			free(rp->auth.dstid);
		free(rp);
	}

	return 0;
}

int
ipsecctl_add_rule(struct ipsecctl *ipsec, struct ipsec_rule *r)
{
	TAILQ_INSERT_TAIL(&ipsec->rule_queue, r, entries);

	if ((ipsec->opts & IPSECCTL_OPT_VERBOSE) && !(ipsec->opts &
	    IPSECCTL_OPT_SHOW))
		ipsecctl_print_rule(r, ipsec->opts & IPSECCTL_OPT_VERBOSE2);

	return 0;
}

void
ipsecctl_print_addr(struct ipsec_addr *ipa)
{
	u_int32_t	mask;
	char		buf[48];

	if (ipa == NULL) {
		printf("?");
		return;
	}
	if (inet_ntop(ipa->af, &ipa->v4, buf, sizeof(buf)) == NULL)
		printf("?");
	else
		printf("%s", buf);

	if (ipa->v4mask.mask32 != 0xffffffff) {
		mask = ntohl(ipa->v4mask.mask32);
		if (mask == 0)
			printf("/0");
		else
			printf("/%d", 32 - ffs((int) mask) + 1);
	}
}

void
ipsecctl_print_rule(struct ipsec_rule *r, int verbose)
{
	static const char *direction[] = {"?", "in", "out"};
	static const char *proto[] = {"?", "esp", "ah"};
	static const char *auth[] = {"?", "psk", "rsa"};

	if (verbose)
		printf("@%d ", r->nr);

	printf("flow %s %s", proto[r->proto], direction[r->direction]);
	printf(" from ");
	ipsecctl_print_addr(r->src);
	printf(" to ");
	ipsecctl_print_addr(r->dst);
	printf(" peer ");
	ipsecctl_print_addr(r->peer);

	if (r->auth.srcid)
		printf(" srcid %s", r->auth.srcid);
	if (r->auth.dstid)
		printf(" dstid %s", r->auth.dstid);

	if (r->auth.type > 0)
		printf(" %s", auth[r->auth.type]);

	printf("\n");
}

int
ipsecctl_flush(int opts)
{
	if (opts & IPSECCTL_OPT_NOACTION)
		return 0;

	if (pfkey_init() == -1)
		errx(1, "failed to open PF_KEY socket");

	pfkey_ipsec_flush();

	return 0;
}

void
ipsecctl_get_rules(struct ipsecctl *ipsec)
{
	struct ipsec_policy *ipo;
	struct ipsec_rule *rule;
	int		 mib[4];
	size_t		 need;
	char		*buf, *lim, *next;

	mib[0] = CTL_NET;
	mib[1] = PF_KEY;
	mib[2] = PF_KEY_V2;
	mib[3] = NET_KEY_SPD_DUMP;

	if (sysctl(mib, 4, NULL, &need, NULL, 0) == -1)
		err(1, "sysctl");

	if (need == 0)
		return;
	if ((buf = malloc(need)) == NULL)
		err(1, "malloc");
	if (sysctl(mib, 4, buf, &need, NULL, 0) == -1)
		err(1, "sysctl");

	lim = buf + need;
	for (next = buf; next < lim; next += sizeof(struct ipsec_policy)) {
		ipo = (struct ipsec_policy *)next;

		/*
		 * We only want static policies and are not interrested in
		 * policies attached to sockets.
		 */
		if (ipo->ipo_flags & IPSP_POLICY_SOCKET)
			continue;

		rule = calloc(1, sizeof(struct ipsec_rule));
		if (rule == NULL)
			err(1, "malloc");
		rule->nr = ipsec->rule_nr++;

		/* Source and destination. */
		if (ipo->ipo_addr.sen_type == SENT_IP4) {
			rule->src = calloc(1, sizeof(struct ipsec_addr));
			if (rule->src == NULL)
				err(1, "calloc");
			rule->src->af = AF_INET;

			bcopy(&ipo->ipo_addr.sen_ip_src.s_addr, &rule->src->v4,
			    sizeof(struct in_addr));
			bcopy(&ipo->ipo_mask.sen_ip_src.s_addr,
			    &rule->src->v4mask.mask, sizeof(struct in_addr));

			rule->dst = calloc(1, sizeof(struct ipsec_addr));
			if (rule->dst == NULL)
				err(1, "calloc");
			rule->dst->af = AF_INET;

			bcopy(&ipo->ipo_addr.sen_ip_dst.s_addr, &rule->dst->v4,
			    sizeof(struct in_addr));
			bcopy(&ipo->ipo_mask.sen_ip_dst.s_addr,
			    &rule->dst->v4mask.mask, sizeof(struct in_addr));
		} else
			warnx("unsupported encapsulation policy type %d",
			    ipo->ipo_addr.sen_type);

		/* IPsec gateway. */
		if (ipo->ipo_dst.sa.sa_family == AF_INET) {
			rule->peer = calloc(1, sizeof(struct ipsec_addr));
			if (rule->peer == NULL)
				err(1, "calloc");
			rule->peer->af = AF_INET;

			bcopy(&((struct sockaddr_in *)&ipo->ipo_dst.sa)->sin_addr,
			    &rule->peer->v4, sizeof(struct in_addr));

			/* No netmask for peer. */
			memset(&rule->peer->v4mask, 0xff, sizeof(u_int32_t));

			if (ipo->ipo_sproto == IPPROTO_ESP)
				rule->proto = IPSEC_ESP;
			else if (ipo->ipo_sproto == IPPROTO_AH)
				rule->proto = IPSEC_AH;
			else {
				rule->proto = PROTO_UNKNWON;
				warnx("unsupported protocol %d",
				    ipo->ipo_sproto);
			}

			if (ipo->ipo_addr.sen_direction == IPSP_DIRECTION_OUT)
				rule->direction = IPSEC_OUT;
			else if (ipo->ipo_addr.sen_direction == IPSP_DIRECTION_IN)
				rule->direction = IPSEC_IN;
			else {
				rule->direction = DIRECTION_UNKNOWN;
				warnx("bogus direction %d",
				    ipo->ipo_addr.sen_direction);
			}
		} else
			warnx("unsupported address family %d",
			    ipo->ipo_dst.sa.sa_family);

		ipsecctl_add_rule(ipsec, rule);
	}
}

void
ipsecctl_show(int opts)
{
	struct ipsecctl ipsec;
	struct ipsec_rule *rp;

	memset(&ipsec, 0, sizeof(ipsec));
	ipsec.opts = opts;
	TAILQ_INIT(&ipsec.rule_queue);

	ipsecctl_get_rules(&ipsec);

	while ((rp = TAILQ_FIRST(&ipsec.rule_queue))) {
		TAILQ_REMOVE(&ipsec.rule_queue, rp, entries);

		ipsecctl_print_rule(rp, ipsec.opts & IPSECCTL_OPT_VERBOSE2);

		free(rp->src);
		free(rp->dst);
		free(rp->peer);
		free(rp);
	}

	return;
}

__dead void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-Fnsv] [-f file]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int		 error = 0;
	int		 ch;
	int		 opts = 0;
	char		*rulesopt = NULL;

	if (argc < 2)
		usage();

	while ((ch = getopt(argc, argv, "f:Fnvs")) != -1) {
		switch (ch) {
		case 'f':
			rulesopt = optarg;
			break;

		case 'F':
			opts |= IPSECCTL_OPT_FLUSH;
			break;

		case 'n':
			opts |= IPSECCTL_OPT_NOACTION;
			break;

		case 'v':
			if (opts & IPSECCTL_OPT_VERBOSE)
				opts |= IPSECCTL_OPT_VERBOSE2;
			opts |= IPSECCTL_OPT_VERBOSE;
			break;

		case 's':
			opts |= IPSECCTL_OPT_SHOW;
			break;

		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (argc != optind) {
		warnx("unknown command line argument: %s ...", argv[optind]);
		usage();
		/* NOTREACHED */
	}
	if (opts & IPSECCTL_OPT_FLUSH)
		if (ipsecctl_flush(opts))
			error = 1;

	if (rulesopt != NULL)
		if (ipsecctl_rules(rulesopt, opts))
			error = 1;

	if (opts & IPSECCTL_OPT_SHOW)
		ipsecctl_show(opts);

	exit(error);
}
