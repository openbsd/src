/*	$OpenBSD: pfctl_table.c,v 1.29 2003/01/25 23:17:34 cedric Exp $ */

/*
 * Copyright (c) 2002 Cedric Berger
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>
#include <arpa/inet.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pfctl_parser.h"
#include "pfctl.h"

#define BUF_SIZE 256

extern void	usage(void);
static int	pfctl_table(int, char *[], char *, char *, char *, int);
static void	grow_buffer(int, int);
static void	print_table(struct pfr_table *, int);
static void	print_tstats(struct pfr_tstats *, int);
static void	load_addr(int, char *[], char *, int);
static int	next_token(char [], FILE *);
static void	append_addr(char *, int);
static void	print_addrx(struct pfr_addr *, struct pfr_addr *, int);
static void	print_astats(struct pfr_astats *, int);
static void	radix_perror(void);
static void	inactive_cleanup(void);
static void	xprintf(int, char *, ...);

static union {
	caddr_t			 caddr;
	struct pfr_table	*tables;
	struct pfr_addr		*addrs;
	struct pfr_tstats	*tstats;
	struct pfr_astats	*astats;
} buffer, buffer2;

static int	 size, msize, ticket, inactive;
extern char	*__progname;

static char	*stats_text[PFR_DIR_MAX][PFR_OP_TABLE_MAX] = {
	{ "In/Block:",	"In/Pass:",	"In/XPass:" },
	{ "Out/Block:",	"Out/Pass:",	"Out/XPass:" }
};

#define RVTEST(fct) do {				\
		if ((!(opts & PF_OPT_NOACTION) ||	\
		    (opts & PF_OPT_DUMMYACTION)) &&	\
		    (fct)) {				\
			radix_perror();			\
			return (1);			\
		}					\
	} while (0)

#define CREATE_TABLE do {						\
		table.pfrt_flags |= PFR_TFLAG_PERSIST;			\
		RVTEST(pfr_add_tables(&table, 1, &nadd, flags));	\
		if (nadd) {						\
			xprintf(opts, "%d table created", nadd);	\
			if (opts & PF_OPT_NOACTION)			\
				return (0);				\
		}							\
		table.pfrt_flags &= ~PFR_TFLAG_PERSIST;			\
	} while(0)

int
pfctl_clear_tables(int opts)
{
	return pfctl_table(0, NULL, NULL, "-F", NULL, opts);
}

int
pfctl_show_tables(int opts)
{
	return pfctl_table(0, NULL, NULL, "-s", NULL, opts);
}

int
pfctl_command_tables(int argc, char *argv[], char *tname,
    char *command, char *file, int opts)
{
	if (tname == NULL || command == NULL)
		usage();
	return pfctl_table(argc, argv, tname, command, file, opts);
}

int
pfctl_table(int argc, char *argv[], char *tname, char *command,
    char *file, int opts)
{
	struct pfr_table  table;
	int		  nadd = 0, ndel = 0, nchange = 0, nzero = 0;
	int		  i, flags = 0, nmatch = 0;

	if (command == NULL)
		usage();
	if (opts & PF_OPT_NOACTION)
		flags |= PFR_FLAG_DUMMY;
	bzero(&table, sizeof(table));
	if (tname != NULL) {
		if (strlen(tname) >= PF_TABLE_NAME_SIZE)
			usage();
		if (strlcpy(table.pfrt_name, tname,
		    sizeof(table.pfrt_name)) >= sizeof(table.pfrt_name))
			errx(1, "pfctl_table: strlcpy");
	}
	if (!strcmp(command, "-F")) {
		if (argc || file != NULL)
			usage();
		RVTEST(pfr_clr_tables(&ndel, flags));
		xprintf(opts, "%d tables deleted", ndel);
	} else if (!strcmp(command, "-s")) {
		if (argc || file != NULL)
			usage();
		for (;;) {
			if (opts & PF_OPT_VERBOSE2) {
				grow_buffer(sizeof(struct pfr_tstats), size);
				size = msize;
				RVTEST(pfr_get_tstats(buffer.tstats, &size,
				    flags));
			} else {
				grow_buffer(sizeof(struct pfr_table), size);
				size = msize;
				RVTEST(pfr_get_tables(buffer.tables, &size,
				    flags));
			}
			if (size <= msize)
				break;
		}
		for (i = 0; i < size; i++)
			if (opts & PF_OPT_VERBOSE2)
				print_tstats(buffer.tstats+i,
				    opts & PF_OPT_VERBOSE);
			else
				print_table(buffer.tables+i,
				    opts & PF_OPT_VERBOSE);
	} else if (!strcmp(command, "create")) {
		if (argc || file != NULL)
			usage();
		table.pfrt_flags = PFR_TFLAG_PERSIST;
		RVTEST(pfr_add_tables(&table, 1, &nadd, flags));
		xprintf(opts, "%d table added", nadd);
	} else if (!strcmp(command, "kill")) {
		if (argc || file != NULL)
			usage();
		RVTEST(pfr_del_tables(&table, 1, &ndel, flags));
		xprintf(opts, "%d table deleted", ndel);
	} else if (!strcmp(command, "flush")) {
		if (argc || file != NULL)
			usage();
		RVTEST(pfr_clr_addrs(&table, &ndel, flags));
		xprintf(opts, "%d addresses deleted", ndel);
	} else if (!strcmp(command, "add")) {
		load_addr(argc, argv, file, 0);
		CREATE_TABLE;
		if (opts & PF_OPT_VERBOSE)
			flags |= PFR_FLAG_FEEDBACK;
		RVTEST(pfr_add_addrs(&table, buffer.addrs, size, &nadd,
		    flags));
		xprintf(opts, "%d/%d addresses added", nadd, size);
		if (opts & PF_OPT_VERBOSE)
			for (i = 0; i < size; i++)
				if ((opts & PF_OPT_VERBOSE2) ||
				    buffer.addrs[i].pfra_fback)
					print_addrx(buffer.addrs+i, NULL,
					    opts & PF_OPT_USEDNS);
	} else if (!strcmp(command, "delete")) {
		load_addr(argc, argv, file, 0);
		if (opts & PF_OPT_VERBOSE)
			flags |= PFR_FLAG_FEEDBACK;
		RVTEST(pfr_del_addrs(&table, buffer.addrs, size, &nadd,
		    flags));
		xprintf(opts, "%d/%d addresses deleted", nadd, size);
		if (opts & PF_OPT_VERBOSE)
			for (i = 0; i < size; i++)
				if ((opts & PF_OPT_VERBOSE2) ||
				    buffer.addrs[i].pfra_fback)
					print_addrx(buffer.addrs+i, NULL,
					    opts & PF_OPT_USEDNS);
	} else if (!strcmp(command, "replace")) {
		load_addr(argc, argv, file, 0);
		CREATE_TABLE;
		if (opts & PF_OPT_VERBOSE)
			flags |= PFR_FLAG_FEEDBACK;
		for (;;) {
			int size2 = msize;

			RVTEST(pfr_set_addrs(&table, buffer.addrs, size,
			    &size2, &nadd, &ndel, &nchange, flags));
			if (size2 <= msize) {
				size = size2;
				break;
			} else
				grow_buffer(sizeof(struct pfr_addr), size2);
		}
		if (nadd)
			xprintf(opts, "%d addresses added", nadd);
		if (ndel)
			xprintf(opts, "%d addresses deleted", ndel);
		if (nchange)
			xprintf(opts, "%d addresses changed", nchange);
		if (!nadd && !ndel && !nchange)
			xprintf(opts, "no changes");
		if (opts & PF_OPT_VERBOSE)
			for (i = 0; i < size; i++)
				if ((opts & PF_OPT_VERBOSE2) ||
				    buffer.addrs[i].pfra_fback)
					print_addrx(buffer.addrs+i, NULL,
					    opts & PF_OPT_USEDNS);
	} else if (!strcmp(command, "show")) {
		if (argc || file != NULL)
			usage();
		for (;;) {
			if (opts & PF_OPT_VERBOSE) {
				grow_buffer(sizeof(struct pfr_astats), size);
				size = msize;
				RVTEST(pfr_get_astats(&table, buffer.astats,
				    &size, flags));
			} else {
				grow_buffer(sizeof(struct pfr_addr), size);
				size = msize;
				RVTEST(pfr_get_addrs(&table, buffer.addrs,
				    &size, flags));
			}
			if (size <= msize)
				break;
		}
		for (i = 0; i < size; i++)
			if (opts & PF_OPT_VERBOSE) {
				print_astats(buffer.astats+i,
				    opts & PF_OPT_USEDNS);
			} else {
				print_addrx(buffer.addrs+i, NULL,
				    opts & PF_OPT_USEDNS);
			}
	} else if (!strcmp(command, "test")) {
		load_addr(argc, argv, file, 1);
		if (opts & PF_OPT_VERBOSE2) {
			flags |= PFR_FLAG_REPLACE;
			buffer2.caddr = calloc(sizeof(buffer.addrs[0]), size);
			if (buffer2.caddr == NULL)
				err(1, "calloc");
			memcpy(buffer2.addrs, buffer.addrs, size *
			    sizeof(buffer.addrs[0]));
		}
		RVTEST(pfr_tst_addrs(&table, buffer.addrs, size, &nmatch,
		    flags));
		xprintf(opts, "%d/%d addresses match", nmatch, size);
		if (opts & PF_OPT_VERBOSE && !(opts & PF_OPT_VERBOSE2))
			for (i = 0; i < size; i++)
				if (buffer.addrs[i].pfra_fback == PFR_FB_MATCH)
					print_addrx(buffer.addrs+i, NULL,
					    opts & PF_OPT_USEDNS);
		if (opts & PF_OPT_VERBOSE2)
			for (i = 0; i < size; i++)
				print_addrx(buffer2.addrs+i, buffer.addrs+i,
				    opts & PF_OPT_USEDNS);
		if (nmatch < size)
			return (2);
	} else if (!strcmp(command, "zero")) {
		if (argc || file != NULL)
			usage();
		flags |= PFR_FLAG_ADDRSTOO;
		RVTEST(pfr_clr_tstats(&table, 1, &nzero, flags));
		xprintf(opts, "%d table/stats cleared", nzero);
	} else
		assert(0);
	return (0);
}

void
grow_buffer(int bs, int minsize)
{
	assert(minsize == 0 || minsize > msize);
	if (!msize) {
		msize = minsize;
		if (msize < 64)
			msize = 64;
		buffer.caddr = calloc(bs, msize);
		if (buffer.caddr == NULL)
			err(1, "calloc");
	} else {
		int omsize = msize;
		if (minsize == 0)
			msize *= 2;
		else
			msize = minsize;
		buffer.caddr = realloc(buffer.caddr, msize * bs);
		if (buffer.caddr == NULL)
			err(1, "realloc");
		bzero(buffer.caddr + omsize * bs, (msize-omsize) * bs);
	}
}

void
print_table(struct pfr_table *ta, int all)
{
	if (!all && !(ta->pfrt_flags & PFR_TFLAG_ACTIVE))
		return;
	if (all) {
		printf("%c%c%c%c%c\t%s\n",
		    (ta->pfrt_flags & PFR_TFLAG_CONST) ? 'c' : '-',
		    (ta->pfrt_flags & PFR_TFLAG_PERSIST) ? 'p' : '-',
		    (ta->pfrt_flags & PFR_TFLAG_ACTIVE) ? 'a' : '-',
		    (ta->pfrt_flags & PFR_TFLAG_INACTIVE) ? 'i' : '-',
		    (ta->pfrt_flags & PFR_TFLAG_REFERENCED) ? 'r' : '-',
		    ta->pfrt_name);
	} else
		puts(ta->pfrt_name);
}

void
print_tstats(struct pfr_tstats *ts, int all)
{
	time_t	time = ts->pfrts_tzero;
	int	dir, op;

	if (!all && !(ts->pfrts_flags & PFR_TFLAG_ACTIVE))
		return;
	print_table(&ts->pfrts_t, all);
	printf("\tAddresses:   %d\n", ts->pfrts_cnt);
	printf("\tReferences:  %d\n", ts->pfrts_refcnt);
	printf("\tCleared:     %s", ctime(&time));
	printf("\tEvaluations: [ NoMatch: %-18llu Match: %-18llu ]\n",
	    ts->pfrts_nomatch, ts->pfrts_match);
	for (dir = 0; dir < PFR_DIR_MAX; dir++)
		for (op = 0; op < PFR_OP_TABLE_MAX; op++)
			printf("\t%-12s [ Packets: %-18llu Bytes: %-18llu ]\n",
			    stats_text[dir][op],
			    ts->pfrts_packets[dir][op],
			    ts->pfrts_bytes[dir][op]);
}

void
load_addr(int argc, char *argv[], char *file, int nonetwork)
{
	FILE	*fp;
	char	 buf[BUF_SIZE];

	while (argc--)
		append_addr(*argv++, nonetwork);
	if (file == NULL)
		return;
	if (!strcmp(file, "-"))
		fp = stdin;
	else {
		fp = fopen(file, "r");
		if (fp == NULL)
			err(1, "%s", file);
	}
	while (next_token(buf, fp))
		append_addr(buf, nonetwork);
	fclose(fp);
}

int
next_token(char buf[BUF_SIZE], FILE *fp)
{
	static char	next_ch = ' ';
	int		i = 0;

	for (;;) {
		/* skip spaces */
		while (isspace(next_ch) && !feof(fp))
			next_ch = fgetc(fp);
		/* remove from '#' until end of line */
		if (next_ch == '#')
			while (!feof(fp)) {
				next_ch = fgetc(fp);
				if (next_ch == '\n')
					break;
			}
		else
			break;
	}
	if (feof(fp)) {
		next_ch = ' ';
		return (0);
	}
	do {
		if (i < BUF_SIZE)
			buf[i++] = next_ch;
		next_ch = fgetc(fp);
	} while (!feof(fp) && !isspace(next_ch));
	if (i >= BUF_SIZE)
		errx(1, "address too long (%d bytes)", i);
	buf[i] = '\0';
	return (1);
}

void
append_addr(char *s, int test)
{
	char			 buf[BUF_SIZE], *r;
	int			 not = 0;
	struct node_host	*n, *h;

	for (r = s; *r == '!'; r++)
		not = !not;
	if (strlcpy(buf, r, sizeof(buf)) >= sizeof(buf))
		errx(1, "address too long");

	if ((n = host(buf, -1)) == NULL)
		exit (1);

	do {
		if (size >= msize)
			grow_buffer(sizeof(struct pfr_addr), 0);
		buffer.addrs[size].pfra_not = not;
		switch (n->af) {
		case AF_INET:
			buffer.addrs[size].pfra_af = AF_INET;
			buffer.addrs[size].pfra_ip4addr.s_addr =
			    n->addr.v.a.addr.addr32[0];
			buffer.addrs[size].pfra_net =
			    unmask(&n->addr.v.a.mask, AF_INET);
			if (test && (not || buffer.addrs[size].pfra_net != 32))
				errx(1, "illegal test address");
			if (buffer.addrs[size].pfra_net > 32)
				errx(1, "illegal netmask %d",
				    buffer.addrs[size].pfra_net);
			break;
		case AF_INET6:
			buffer.addrs[size].pfra_af = AF_INET6;
			memcpy(&buffer.addrs[size].pfra_ip6addr,
			    &n->addr.v.a.addr.v6, sizeof(struct in6_addr));
			buffer.addrs[size].pfra_net =
			    unmask(&n->addr.v.a.mask, AF_INET6);
			if (test && (not || buffer.addrs[size].pfra_net != 128))
				errx(1, "illegal test address");
			if (buffer.addrs[size].pfra_net > 128)
				errx(1, "illegal netmask %d",
				    buffer.addrs[size].pfra_net);
			break;
		default:
			errx(1, "unknown address family %d", n->af);
			break;
		}
		size++;
		h = n;
		n = n->next;
		free(h);
	} while (n != NULL);
}

void
print_addrx(struct pfr_addr *ad, struct pfr_addr *rad, int dns)
{
	char		ch, buf[BUF_SIZE] = "{error}";
	char		fb[] = { ' ', 'M', 'A', 'D', 'C', 'Z', 'X', ' ', 'Y' };
	unsigned int	fback, hostnet;

	fback = (rad != NULL) ? rad->pfra_fback : ad->pfra_fback;
	ch = (fback < sizeof(fb)/sizeof(*fb)) ? fb[fback] : '?';
	hostnet = (ad->pfra_af == AF_INET6) ? 128 : 32;
	inet_ntop(ad->pfra_af, &ad->pfra_u, buf, sizeof(buf));
	printf("%c %c%s", ch, (ad->pfra_not?'!':' '), buf);
	if (ad->pfra_net < hostnet)
		printf("/%d", ad->pfra_net);
	if (rad != NULL && fback != PFR_FB_NONE) {
		if (strlcpy(buf, "{error}", sizeof(buf)) >= sizeof(buf))
			errx(1, "print_addrx: strlcpy");
		inet_ntop(rad->pfra_af, &rad->pfra_u, buf, sizeof(buf));
		printf("\t%c%s", (rad->pfra_not?'!':' '), buf);
		if (rad->pfra_net < hostnet)
			printf("/%d", rad->pfra_net);
	}
	if (rad != NULL && fback == PFR_FB_NONE)
		printf("\t nomatch");
	if (dns && ad->pfra_net == hostnet) {
		char host[NI_MAXHOST];
		union sockaddr_union sa;

		strlcpy(host, "?", sizeof(host));
		bzero(&sa, sizeof(sa));
		sa.sa.sa_family = ad->pfra_af;
		if (sa.sa.sa_family == AF_INET) {
			sa.sa.sa_len = sizeof(sa.sin);
			sa.sin.sin_addr = ad->pfra_ip4addr;
		} else {
			sa.sa.sa_len = sizeof(sa.sin6);
			sa.sin6.sin6_addr = ad->pfra_ip6addr;
		}
		if (getnameinfo(&sa.sa, sa.sa.sa_len, host, sizeof(host),
		    NULL, 0, NI_NAMEREQD) == 0)
			printf("\t(%s)", host);
	}
	printf("\n");
}

void
print_astats(struct pfr_astats *as, int dns)
{
	time_t	time = as->pfras_tzero;
	int	dir, op;

	print_addrx(&as->pfras_a, NULL, dns);
	printf("\tCleared:     %s", ctime(&time));
	for (dir = 0; dir < PFR_DIR_MAX; dir++)
		for (op = 0; op < PFR_OP_ADDR_MAX; op++)
			printf("\t%-12s [ Packets: %-18llu Bytes: %-18llu ]\n",
			    stats_text[dir][op],
			    as->pfras_packets[dir][op],
			    as->pfras_bytes[dir][op]);
}

void
radix_perror(void)
{
	if (errno == ESRCH)
		fprintf(stderr, "%s: Table does not exist.\n", __progname);
	else
		perror(__progname);
}

void
pfctl_begin_table(void)
{
	static int hookreg;
	int rv;

	if ((loadopt & (PFCTL_FLAG_TABLE | PFCTL_FLAG_ALL)) == 0)
		return;
	rv = pfr_ina_begin(&ticket, NULL, 0);
	if (rv) {
		radix_perror();
		exit(1);
	}
	if (!hookreg) {
		atexit(inactive_cleanup);
		hookreg = 1;
	}
}

void
pfctl_append_addr(char *addr, int net, int neg)
{
	char *p = NULL;

	if (net < 0 && !neg) {
		append_addr(addr, 0);
		return;
	}
	if (net >= 0 && !neg)
		asprintf(&p, "%s/%d", addr, net);
	else if (net < 0)
		asprintf(&p, "!%s", addr);
	else
		asprintf(&p, "!%s/%d", addr, net);
	if (p == NULL) {
		radix_perror();
		exit(1);
	}
	append_addr(p, 0);
	free(p);
}

void
pfctl_append_file(char *file)
{
	load_addr(0, NULL, file, 0);
}

void
pfctl_define_table(char *name, int flags, int addrs, int noaction)
{
	struct pfr_table tbl;
	int rv;

	if (noaction || (loadopt & (PFCTL_FLAG_TABLE | PFCTL_FLAG_ALL)) == 0) {
		size = 0;
		return;
	}
	bzero(&tbl, sizeof(tbl));
	if (strlcpy(tbl.pfrt_name, name, sizeof(tbl.pfrt_name)) >=
	    sizeof(tbl.pfrt_name))
		errx(1, "pfctl_define_table: strlcpy");
	tbl.pfrt_flags = flags;

	inactive = 1;
	rv = pfr_ina_define(&tbl, buffer.addrs, size, NULL, NULL, ticket,
	    addrs ? PFR_FLAG_ADDRSTOO : 0);
	if (rv) {
		radix_perror();
		exit(1);
	}
	size = 0;
}

void
pfctl_commit_table()
{
	int rv;

	if ((loadopt & (PFCTL_FLAG_TABLE | PFCTL_FLAG_ALL)) == 0)
		return;
	rv = pfr_ina_commit(ticket, NULL, NULL, 0);
	if (rv) {
		radix_perror();
		exit(1);
	}
	inactive = 0;
}

void
inactive_cleanup()
{
	if (inactive)
		pfr_ina_begin(NULL, NULL, 0);
}

void
xprintf(int opts, char *fmt, ...)
{
	va_list args;

	if (opts & PF_OPT_QUIET)
		return;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	if (opts & PF_OPT_DUMMYACTION)
		fprintf(stderr, " (dummy).\n");
	else if (opts & PF_OPT_NOACTION)
		fprintf(stderr, " (syntax only).\n");
	else
		fprintf(stderr, ".\n");
}
