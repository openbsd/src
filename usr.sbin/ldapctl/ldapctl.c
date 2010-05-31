/*	$OpenBSD: ldapctl.c,v 1.1 2010/05/31 17:36:31 martinh Exp $	*/

/*
 * Copyright (c) 2009, 2010 Martin Hedenfalk <martin@bzero.se>
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@vantronix.net>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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
#include <sys/un.h>
#include <sys/tree.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <event.h>

#include "ldapd.h"

enum action {
	NONE,
	SHOW_STATS,
	LOG_VERBOSE,
	LOG_BRIEF,
	COMPACT_DB,
	INDEX_DB
};

__dead void	 usage(void);
void		 show_stats(struct imsg *imsg);
void		 show_dbstats(const char *prefix, struct btree_stat *st);
void		 show_nsstats(struct imsg *imsg);
void		 show_compact_status(struct imsg *imsg);
void		 show_index_status(struct imsg *imsg);

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-s socket] command [arg ...]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int			 ctl_sock;
	int			 done = 0, verbose = 0;
	ssize_t			 n;
	int			 ch;
	enum action		 action = NONE;
	const char		*sock = LDAPD_SOCKET;
	struct sockaddr_un	 sun;
	struct imsg		 imsg;
	struct imsgbuf		 ibuf;

	while ((ch = getopt(argc, argv, "s:")) != -1) {
		switch (ch) {
		case 's':
			sock = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	if (strcmp(argv[0], "stats") == 0)
		action = SHOW_STATS;
	else if (strcmp(argv[0], "compact") == 0)
		action = COMPACT_DB;
	else if (strcmp(argv[0], "index") == 0)
		action = INDEX_DB;
	else if (strcmp(argv[0], "log") == 0) {
		if (argc != 2)
			usage();
		if (strcmp(argv[1], "verbose") == 0)
			action = LOG_VERBOSE;
		else if (strcmp(argv[1], "brief") == 0)
			action = LOG_BRIEF;
		else
			usage();
	} else
		usage();

	/* connect to ldapd control socket */
	if ((ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, sock, sizeof(sun.sun_path));
	if (connect(ctl_sock, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "connect: %s", sock);

	imsg_init(&ibuf, ctl_sock);
	done = 0;

	/* process user request */
	switch (action) {
	case SHOW_STATS:
		imsg_compose(&ibuf, IMSG_CTL_STATS, 0, 0, -1, NULL, 0);
		break;
	case COMPACT_DB:
		imsg_compose(&ibuf, IMSG_CTL_COMPACT, 0, 0, -1, NULL, 0);
		break;
	case INDEX_DB:
		imsg_compose(&ibuf, IMSG_CTL_INDEX, 0, 0, -1, NULL, 0);
		break;
	case LOG_VERBOSE:
		verbose = 1;
		/* FALLTHROUGH */
	case LOG_BRIEF:
		imsg_compose(&ibuf, IMSG_CTL_LOG_VERBOSE, 0, 0, -1,
		    &verbose, sizeof(verbose));
		printf("logging request sent.\n");
		done = 1;
		break;
	case NONE:
		break;
	}

	while (ibuf.w.queued)
		if (msgbuf_write(&ibuf.w) < 0)
			err(1, "write error");

	while (!done) {
		if ((n = imsg_read(&ibuf)) == -1)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		while (!done) {
			if ((n = imsg_get(&ibuf, &imsg)) == -1)
				errx(1, "imsg_get error");
			if (n == 0)
				break;
			switch (imsg.hdr.type) {
			case IMSG_CTL_STATS:
				show_stats(&imsg);
				break;
			case IMSG_CTL_NSSTATS:
				show_nsstats(&imsg);
				break;
			case IMSG_CTL_COMPACT_STATUS:
				show_compact_status(&imsg);
				break;
			case IMSG_CTL_INDEX_STATUS:
				show_index_status(&imsg);
				break;
			case IMSG_CTL_END:
				done = 1;
				break;
			case NONE:
				break;
			}
			imsg_free(&imsg);
		}
	}
	close(ctl_sock);

	return (0);
}

void
show_stats(struct imsg *imsg)
{
	struct ldapd_stats	*st;

	st = imsg->data;

	printf("start time: %s", ctime(&st->started_at));
	printf("requests: %llu\n", st->requests);
	printf("search requests: %llu\n", st->req_search);
	printf("bind requests: %llu\n", st->req_bind);
	printf("modify requests: %llu\n", st->req_mod);
	printf("timeouts: %llu\n", st->timeouts);
	printf("unindexed searches: %llu\n", st->unindexed);
	printf("active connections: %u\n", st->conns);
	printf("active searches: %u\n", st->searches);
}

#define ZDIV(t,n)	((n) == 0 ? 0 : (float)(t) / (n))

void
show_dbstats(const char *prefix, struct btree_stat *st)
{
	printf("%s timestamp: %s", prefix, ctime(&st->created_at));
	printf("%s page size: %u\n", prefix, st->psize);
	printf("%s depth: %u\n", prefix, st->depth);
	printf("%s revisions: %u\n", prefix, st->revisions);
	printf("%s entries: %llu\n", prefix, st->entries);
	printf("%s branch/leaf/overflow pages: %u/%u/%u\n",
	    prefix, st->branch_pages, st->leaf_pages, st->overflow_pages);

	printf("%s cache size: %u of %u (%.1f%% full)\n", prefix,
	    st->cache_size, st->max_cache,
	    100 * ZDIV(st->cache_size, st->max_cache));
	printf("%s page reads: %llu\n", prefix, st->reads);
	printf("%s cache hits: %llu (%.1f%%)\n", prefix, st->hits,
	    100 * ZDIV(st->hits, (st->hits + st->reads)));
}

void
show_nsstats(struct imsg *imsg)
{
	struct ns_stat		*nss;

	nss = imsg->data;

	printf("\nsuffix: %s\n", nss->suffix);
	show_dbstats("data", &nss->data_stat);
	show_dbstats("indx", &nss->indx_stat);
}

void
show_compact_status(struct imsg *imsg)
{
	struct compaction_status	*cs;

	cs = imsg->data;
	printf("%s (%s): %s\n", cs->suffix,
	    cs->db == 1 ? "entries" : "index",
	    cs->status == 0 ? "ok" : "failed");
}

void
show_index_status(struct imsg *imsg)
{
	struct indexer_status	*is;

	is = imsg->data;
	if (is->status != 0)
		printf("\r%s: %s  \n", is->suffix,
		    is->status < 0 ? "failed" : "ok");
	else if (is->entries > 0)
		printf("\r%s: %i%%",
		    is->suffix,
		    (int)(100 * (double)is->ncomplete / is->entries));
	else
		printf("\r%s: 100%%", is->suffix);

	fflush(stdout);
}

