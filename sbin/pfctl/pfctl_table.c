/*	$OpenBSD: pfctl_table.c,v 1.3 2003/01/03 21:53:35 cedric Exp $ */

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

#include <err.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>

#include "pfctl_table.h"
#include "pfctl_radix.h"
#include "pfctl_parser.h"
#include "pf_print_state.h"


#define _BUF_SIZE 256

extern void	 usage(void);
static int	_pfctl_table(int, char *[], char *, char *, char *, int);
static void	_grow_buffer(int, int);
static void	_print_table(struct pfr_table *);
static void	_print_tstats(struct pfr_tstats *);
static void	_load_addr(int, char *[], char *, int);
static int	_next_token(char [_BUF_SIZE], FILE *);
static void	_append_addr(char *, int);
static void	_print_addr(struct pfr_addr *, struct pfr_addr *, int);
static void	_print_astats(struct pfr_astats *, int);
static void	_perror(void);


static union {
	caddr_t			 caddr;
	struct pfr_table	*tables;
	struct pfr_addr		*addrs;
	struct pfr_tstats	*tstats;
	struct pfr_astats	*astats;
} buffer, buffer2;

static int	 size, msize;
extern char	*__progname;

static char	*commands[] = {
	"-F",		/* pfctl -F tables: flush all tables */
	"-s",		/* pfctl -s tables: show all tables */
	"create",	/* create a new table */
	"kill",		/* kill a table */
	"flush",	/* flush all addresses of a table */
	"add",		/* add one or more addresses in a table */
	"delete",	/* delete one or more addresses from a table */
	"replace",	/* replace the addresses of the table */
	"show",		/* show the content (addresses) of a table */
	"test",		/* test if the given addresses match a table */
	"zero",		/* clear all the statistics of a table */
	NULL
};

static char	*stats_text[PFR_DIR_MAX][PFR_OP_TABLE_MAX] = {
	{ "In/Block:",	"In/Pass:",	"In/XPass:" },
	{ "Out/Block:",	"Out/Pass:",	"Out/XPass:" }
};


#define DUMMY ((flags & PFR_FLAG_DUMMY)?" (dummy)":"")
#define RVTEST(fct)					\
	do { int rv = fct; if (rv)			\
		{ _perror(); return (1); }		\
	} while (0)

int
pfctl_clear_tables(int opts)
{
	return _pfctl_table(0, NULL, NULL, "-F", NULL, opts);
}

int
pfctl_show_tables(int opts)
{
	return _pfctl_table(0, NULL, NULL, "-s", NULL, opts);
}

int
pfctl_command_tables(int argc, char *argv[], char *tname,
	char *command, char *file, int opts)
{
	if (tname == NULL || command == NULL)
		usage();
	return _pfctl_table(argc, argv, tname, command, file, opts);
}

int
_pfctl_table(int argc, char *argv[], char *tname, char *command,
		char *file, int opts)
{
	struct pfr_table  table;
	char		**p;
	int		  nadd = 0, ndel = 0, nchange = 0, nzero = 0;
	int		  i, flags = 0, nmatch = 0;

	for (p = commands; *p != NULL; p++)
		if (!strncmp(command, *p, strlen(command)))
			break;
	if (*p == NULL)
		usage();
	if (opts & PF_OPT_NOACTION)
		flags |= PFR_FLAG_DUMMY;
	bzero(&table, sizeof(table));
	if (tname != NULL) {
		if (strlen(tname) >= PF_TABLE_NAME_SIZE)
			usage();
		strlcpy(table.pfrt_name, tname, PF_TABLE_NAME_SIZE);
	}
	if (!strcmp(*p, "-F")) {
		if (argc || file != NULL)
			usage();
		RVTEST(pfr_clr_tables(&ndel, flags));
		if (!(opts & PF_OPT_QUIET))
			fprintf(stderr, "%d tables deleted%s.\n", ndel,
			    DUMMY);
	}
	if (!strcmp(*p, "-s")) {
		if (argc || file != NULL)
			usage();
		for (;;) {
			if (opts & PF_OPT_VERBOSE) {
				_grow_buffer(sizeof(struct pfr_tstats), size);
				size = msize;
				RVTEST(pfr_get_tstats(buffer.tstats, &size,
				    flags));
			} else {
				_grow_buffer(sizeof(struct pfr_table), size);
				size = msize;
				RVTEST(pfr_get_tables(buffer.tables, &size,
				    flags));
			}
			if (size <= msize)
				break;
		}
		for (i = 0; i < size; i++)
			if (opts & PF_OPT_VERBOSE)
				_print_tstats(buffer.tstats+i);
			else
				_print_table(buffer.tables+i);
	}
	if (!strcmp(*p, "create")) {
		if (argc || file != NULL)
			usage();
		RVTEST(pfr_add_tables(&table, 1, &nadd, flags));
		if (!(opts & PF_OPT_QUIET))
			fprintf(stderr, "%d table added%s.\n", nadd, DUMMY);
	}
	if (!strcmp(*p, "kill")) {
		if (argc || file != NULL)
			usage();
		RVTEST(pfr_del_tables(&table, 1, &ndel, flags));
		if (!(opts & PF_OPT_QUIET))
			fprintf(stderr, "%d table deleted%s.\n", ndel, DUMMY);
	}
	if (!strcmp(*p, "flush")) {
		if (argc || file != NULL)
			usage();
		RVTEST(pfr_clr_addrs(&table, &ndel, flags));
		if (!(opts & PF_OPT_QUIET))
			fprintf(stderr, "%d addresses deleted%s.\n", ndel,
				DUMMY);
	}
	if (!strcmp(*p, "add")) {
		_load_addr(argc, argv, file, 0);
		if (opts & PF_OPT_VERBOSE)
			flags |= PFR_FLAG_FEEDBACK;
		RVTEST(pfr_add_addrs(&table, buffer.addrs, size, &nadd,
		    flags));
		if (!(opts & PF_OPT_QUIET))
			fprintf(stderr, "%d/%d addresses added%s.\n", nadd,
			    size, DUMMY);
		if (opts & PF_OPT_VERBOSE)
			for (i = 0; i < size; i++)
				if ((opts & PF_OPT_VERBOSE2) ||
				    buffer.addrs[i].pfra_fback)
					_print_addr(buffer.addrs+i, NULL,
					    opts & PF_OPT_USEDNS);
	}
	if (!strcmp(*p, "delete")) {
		_load_addr(argc, argv, file, 0);
		if (opts & PF_OPT_VERBOSE)
			flags |= PFR_FLAG_FEEDBACK;
		RVTEST(pfr_del_addrs(&table, buffer.addrs, size, &nadd,
		    flags));
		if (!(opts & PF_OPT_QUIET))
			fprintf(stderr, "%d/%d addresses deleted%s.\n", nadd,
			    size, DUMMY);
		if (opts & PF_OPT_VERBOSE)
			for (i = 0; i < size; i++)
				if ((opts & PF_OPT_VERBOSE2) ||
				    buffer.addrs[i].pfra_fback)
					_print_addr(buffer.addrs+i, NULL,
					    opts & PF_OPT_USEDNS);
	}
	if (!strcmp(*p, "replace")) {
		_load_addr(argc, argv, file, 0);
		if (opts & PF_OPT_VERBOSE)
			flags |= PFR_FLAG_FEEDBACK;
		for(;;) {
			int size2 = msize;

			RVTEST(pfr_set_addrs(&table, buffer.addrs, size,
			    &size2, &nadd, &ndel, &nchange, flags));
			if (size2 <= msize) {
				size = size2;
				break;
			} else
				_grow_buffer(sizeof(struct pfr_addr), size2);
		}
		if (!(opts & PF_OPT_QUIET)) {
			if (nadd)
				fprintf(stderr, "%d addresses added%s.\n",
				    nadd, DUMMY);
			if (ndel)
				fprintf(stderr, "%d addresses deleted%s.\n",
				    ndel, DUMMY);
			if (nchange)
				fprintf(stderr, "%d addresses changed%s.\n",
				    nchange, DUMMY);
			if (!nadd && !ndel && !nchange)
				fprintf(stderr, "no changes%s.\n", DUMMY);
		}
		if (opts & PF_OPT_VERBOSE)
			for (i = 0; i < size; i++)
				if ((opts & PF_OPT_VERBOSE2) ||
				    buffer.addrs[i].pfra_fback)
					_print_addr(buffer.addrs+i, NULL,
					    opts & PF_OPT_USEDNS);
	}
	if (!strcmp(*p, "show")) {
		if (argc || file != NULL)
			usage();
		for (;;) {
			if (opts & PF_OPT_VERBOSE) {
				_grow_buffer(sizeof(struct pfr_astats), size);
				size = msize;
				RVTEST(pfr_get_astats(&table, buffer.astats,
				    &size, flags));
			} else {
				_grow_buffer(sizeof(struct pfr_addr), size);
				size = msize;
				RVTEST(pfr_get_addrs(&table, buffer.addrs,
				    &size, flags));
			}
			if (size <= msize)
				break;
		}
		for (i = 0; i < size; i++)
			if (opts & PF_OPT_VERBOSE) {
				_print_astats(buffer.astats+i,
				    opts & PF_OPT_USEDNS);
			} else {
				_print_addr(buffer.addrs+i, NULL,
				    opts & PF_OPT_USEDNS);
			}
	}
	if (!strcmp(*p, "test")) {
		_load_addr(argc, argv, file, 1);
		if (opts & PF_OPT_VERBOSE2) {
			flags |= PFR_FLAG_REPLACE;
			buffer2.caddr = calloc(sizeof(buffer.addrs[0]), size);
			if (buffer2.caddr == NULL) {
				_perror();
				return 1;
			}
			memcpy(buffer2.addrs, buffer.addrs, size *
			    sizeof(buffer.addrs[0]));
		}
		RVTEST(pfr_tst_addrs(&table, buffer.addrs, size, &nmatch,
			flags));
		if (!(opts & PF_OPT_QUIET))
			printf("%d/%d addresses match.\n", nmatch, size);
		if (opts & PF_OPT_VERBOSE && !(opts & PF_OPT_VERBOSE2))
			for (i = 0; i < size; i++)
				if (buffer.addrs[i].pfra_fback == PFR_FB_MATCH)
					_print_addr(buffer.addrs+i, NULL,
					    opts & PF_OPT_USEDNS);
		if (opts & PF_OPT_VERBOSE2)
			for (i = 0; i < size; i++)
				_print_addr(buffer2.addrs+i, buffer.addrs+i,
				    opts & PF_OPT_USEDNS);
		if (nmatch < size)
			return (2);
	}
	if (!strcmp(*p, "zero")) {
		if (argc || file != NULL)
			usage();
		flags |= PFR_FLAG_RECURSE;
		RVTEST(pfr_clr_tstats(&table, 1, &nzero, flags));
		if (!(opts & PF_OPT_QUIET))
			fprintf(stderr, "%d table/stats cleared%s.\n", nzero,
			    DUMMY);
	}
	return (0);
}

void
_grow_buffer(int bs, int minsize)
{
	assert(minsize == 0 || minsize > msize);
	if (!msize) {
		msize = minsize;
		if (msize < 64)
			msize = 64;
		buffer.caddr = calloc(bs, msize);
	} else {
		int omsize = msize;
		if (minsize == 0)
			msize *= 2;
		else
			msize = minsize;
		buffer.caddr = realloc(buffer.caddr, msize * bs);
		if (buffer.caddr)
			bzero(buffer.caddr + omsize * bs, (msize-omsize) * bs);
	}
	if (!buffer.caddr) {
		perror(__progname);
		exit(1);
	}
}

void
_print_table(struct pfr_table *ta)
{
	printf("%s\n", ta->pfrt_name);
}

void
_print_tstats(struct pfr_tstats *ts)
{
	time_t	time = ts->pfrts_tzero;
	int	dir, op;

	printf("%s\n", ts->pfrts_name);
	printf("\tAddresses:   %d\n", ts->pfrts_cnt);
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
_load_addr(int argc, char *argv[], char *file, int nonetwork)
{
	FILE	*fp;
	char	 buf[_BUF_SIZE];

	while (argc--)
		_append_addr(*argv++, nonetwork);
	if (file == NULL)
		return;
	if (!strcmp(file, "-"))
		fp = stdin;
	else {
		fp = fopen(file, "r");
		if (fp == NULL) {
			perror(__progname);
			exit(1);
		}
	}
	while (_next_token(buf, fp))
		_append_addr(buf, nonetwork);
	fclose(fp);
}

int
_next_token(char buf[_BUF_SIZE], FILE *fp)
{
	static char	_next_ch = ' ';
	int		i = 0;

	for (;;) {
		/* skip spaces */
		while (isspace(_next_ch) && !feof(fp))
			_next_ch = fgetc(fp);
		/* remove from '#' until end of line */
		if (_next_ch == '#')
			while (!feof(fp)) {
				_next_ch = fgetc(fp);
				if (_next_ch == '\n')
					break;
			}
		else
			break;
	}
	if (feof(fp)) {
		_next_ch = ' ';
		return (0);
	}
	do {
		if (i < _BUF_SIZE)
			buf[i++] = _next_ch;
		_next_ch = fgetc(fp);
	} while (!feof(fp) && !isspace(_next_ch));
	if (i >= _BUF_SIZE) {
		fprintf(stderr, "%s: address too long (%d bytes)\n",
		    __progname, i);
		exit(1);
	}
	buf[i] = '\0';
	return (1);
}

void
_append_addr(char *s, int test)
{
	char		 buf[_BUF_SIZE], *p, *q;
	struct addrinfo *res, *ai, hints;
	int		 not = (*s == '!'), net = -1, rv;

	bzero(&hints, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM;
	if (strlen(s) >= _BUF_SIZE) {
		fprintf(stderr, "%s: address too long (%ld bytes)\n",
		    __progname, (long)strlen(s));
		exit(1);
	}
	strlcpy(buf, s+not, sizeof(buf));
	p = strrchr(buf, '/');
	if (test && (not || p))
		fprintf(stderr, "%s: illegal test address: \"%s\"\n",
		    __progname, s);
	if (p) {
		net = strtol(p+1, &q, 0);
		if (!q || *q) {
			fprintf(stderr, "%s: illegal network: \"%s\"\n",
			    __progname, p+1);
			exit(1);
		}
		*p++ = '\0';
	}
	rv = getaddrinfo(buf, NULL, &hints, &res);
	if (rv) {
		fprintf(stderr, "%s: illegal address: \"%s\"\n", __progname,
		    buf);
		exit(1);
	}
	for (ai = res; ai; ai = ai->ai_next) {
		switch (ai->ai_family) {
		case AF_INET:
			if (net > 32) {
				fprintf(stderr, "%s: network too big: %d\n",
				    __progname, net);
				exit(1);
			}
			if (size >= msize)
				_grow_buffer(sizeof(struct pfr_addr), 0);
			buffer.addrs[size].pfra_ip4addr =
			    ((struct sockaddr_in *)ai->ai_addr)->sin_addr;
			buffer.addrs[size].pfra_not = not;
			buffer.addrs[size].pfra_net = (net >= 0) ? net : 32;
			buffer.addrs[size].pfra_af = AF_INET;
			size++;
			break;
		case AF_INET6:
			if (net > 128) {
				fprintf(stderr, "%s: network too big: %d\n",
				    __progname, net);
				exit(1);
			}
			if (size >= msize)
				_grow_buffer(sizeof(struct pfr_addr), 0);
			buffer.addrs[size].pfra_ip6addr =
				((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
			buffer.addrs[size].pfra_not = not;
			buffer.addrs[size].pfra_net = (net >= 0) ? net : 128;
			buffer.addrs[size].pfra_af = AF_INET6;
			size++;
			break;
		}
	}
	freeaddrinfo(res);
}

void
_print_addr(struct pfr_addr *ad, struct pfr_addr *rad, int dns)
{
	char		buf[_BUF_SIZE] = "{error}";
	const char	fb[] = { ' ', 'M', 'A', 'D', 'C', 'Z', 'X', ' ' };
	int		fback, hostnet;

	fback = (rad != NULL) ? rad->pfra_fback : ad->pfra_fback;
	hostnet = (ad->pfra_af == AF_INET6) ? 128 : 32;
	inet_ntop(ad->pfra_af, &ad->pfra_u, buf, sizeof(buf));
	printf("%c %c%s", fb[fback], (ad->pfra_not?'!':' '), buf);
	if (ad->pfra_net < hostnet)
		printf("/%d", ad->pfra_net);
	if (rad != NULL && fback != PFR_FB_NONE) {
		strcpy(buf, "{error}");
		inet_ntop(rad->pfra_af, &rad->pfra_u, buf, sizeof(buf));
		printf("\t%c%s", (rad->pfra_not?'!':' '), buf);
		if (rad->pfra_net < hostnet)
			printf("/%d", rad->pfra_net);
	}
	if (rad != NULL && fback == PFR_FB_NONE)
		printf("\t nomatch");
	if (dns && ad->pfra_net == hostnet) {
		char host[NI_MAXHOST] = "?";
		union sockaddr_union sa;
		int rv;

		sa.sa.sa_len = (ad->pfra_af == AF_INET) ?
		    sizeof(sa.sin) : sizeof(sa.sin6);
		sa.sa.sa_family = ad->pfra_af;
		if (ad->pfra_af == AF_INET)
			sa.sin.sin_addr = ad->pfra_ip4addr;
		else
			sa.sin6.sin6_addr = ad->pfra_ip6addr;
		rv = getnameinfo(&sa.sa, sa.sa.sa_len, host, sizeof(host),
		    NULL, 0, NI_NAMEREQD);
		if (!rv)
			printf("\t(%s)", host);
	}
	printf("\n");
}

void
_print_astats(struct pfr_astats *as, int dns)
{
	time_t	time = as->pfras_tzero;
	int	dir, op;

	_print_addr(&as->pfras_a, NULL, dns);
	printf("\tCleared:     %s", ctime(&time));
	for (dir = 0; dir < PFR_DIR_MAX; dir++)
		for (op = 0; op < PFR_OP_ADDR_MAX; op++)
			printf("\t%-12s [ Packets: %-18llu Bytes: %-18llu ]\n",
			    stats_text[dir][op],
			    as->pfras_packets[dir][op],
			    as->pfras_bytes[dir][op]);
}

void
_perror(void)
{
	if (errno == ESRCH)
		fprintf(stderr, "%s: Table does not exist.\n", __progname);
	else
		perror(__progname);
}
