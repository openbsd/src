/*
 * Copyright (c) 2017 Gilles Chehade <gilles@poolp.org>
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
#include <sys/tree.h>

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <netdb.h>

#include <asr.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define LINE_MAX 1024
#include "smtpd-defines.h"
#include "smtpd-api.h"
#include "unpack_dns.h"
#include "parser.h"

int spfwalk(int, struct parameter *);

static void	dispatch_txt(struct dns_rr *);
static void	dispatch_mx(struct dns_rr *);
static void	dispatch_a(struct dns_rr *);
static void	dispatch_aaaa(struct dns_rr *);
static void	lookup_record(int, const char *, void (*)(struct dns_rr *));
static void	dispatch_record(struct asr_result *, void *);
static ssize_t	parse_txt(const char *, size_t, char *, size_t);

int     ip_v4 = 0;
int     ip_v6 = 0;
int     ip_both = 1;

struct dict seen;

int
spfwalk(int argc, struct parameter *argv)
{
	const char	*ip_family = NULL;
	char		*line = NULL;
	size_t		 linesize = 0;
	ssize_t		 linelen;

	if (argv)
		ip_family = argv[0].u.u_str;

	if (ip_family) {
		if (strcmp(ip_family, "-4") == 0) {
			ip_both = 0;
			ip_v4 = 1;
		} else if (strcmp(ip_family, "-6") == 0) {
			ip_both = 0;
			ip_v6 = 1;
		} else
			errx(1, "invalid ip_family");
	}

	argv += optind;
	argc -= optind;

	dict_init(&seen);
  	event_init();

	while ((linelen = getline(&line, &linesize, stdin)) != -1) {
		while (linelen-- > 0 && isspace(line[linelen]))
			line[linelen] = '\0';

		if (linelen > 0)
			lookup_record(T_TXT, line, dispatch_txt);
	}

	free(line);

	if (pledge("dns stdio", NULL) == -1)
		err(1, "pledge");

  	event_dispatch();

	return 0;
}

void
lookup_record(int type, const char *record, void (*cb)(struct dns_rr *))
{
	struct asr_query *as;

	as = res_query_async(record, C_IN, type, NULL);
	if (as == NULL)
		err(1, "res_query_async");
	event_asr_run(as, dispatch_record, cb);
}

void
dispatch_record(struct asr_result *ar, void *arg)
{
	void (*cb)(struct dns_rr *) = arg;
	struct unpack pack;
	struct dns_header h;
	struct dns_query q;
	struct dns_rr rr;

	/* best effort */
	if (ar->ar_h_errno && ar->ar_h_errno != NO_DATA)
		return;

	unpack_init(&pack, ar->ar_data, ar->ar_datalen);
	unpack_header(&pack, &h);
	unpack_query(&pack, &q);

	for (; h.ancount; h.ancount--) {
		unpack_rr(&pack, &rr);
		/**/
		cb(&rr);
	}
}

void
dispatch_txt(struct dns_rr *rr)
{
	struct in6_addr ina;
        char buf[4096];
        char buf2[512];
        char *in = buf;
        char *argv[512];
        char **ap = argv;
	char *end;
	ssize_t n;

	if (rr->rr_type != T_TXT)
		return;
	n = parse_txt(rr->rr.other.rdata, rr->rr.other.rdlen, buf, sizeof(buf));
	if (n == -1 || n == sizeof(buf))
		return;
	buf[n] = '\0';

	if (strncasecmp("v=spf1 ", buf, 7))
		return;

	while ((*ap = strsep(&in, " ")) != NULL) {
		if (strcasecmp(*ap, "v=spf1") == 0)
			continue;

		end = *ap + strlen(*ap)-1;
		if (*end == '.')
			*end = '\0';

		if (dict_set(&seen, *ap, &seen))
			continue;

		if (**ap == '-' || **ap == '~')
			continue;

		if (**ap == '+' || **ap == '?')
			(*ap)++;

		if (strncasecmp("ip4:", *ap, 4) == 0) {
			if ((ip_v4 == 1 || ip_both == 1) &&
			    inet_net_pton(AF_INET, *(ap) + 4,
			    &ina, sizeof(ina)) != -1)
				printf("%s\n", *(ap) + 4);
			continue;
		}
		if (strncasecmp("ip6:", *ap, 4) == 0) {
			if ((ip_v6 == 1 || ip_both == 1) &&
			    inet_net_pton(AF_INET6, *(ap) + 4,
			    &ina, sizeof(ina)) != -1)
				printf("%s\n", *(ap) + 4);
			continue;
		}
		if (strncasecmp("a:", *ap, 2) == 0) {
			lookup_record(T_A, *(ap) + 2, dispatch_a);
			lookup_record(T_AAAA, *(ap) + 2, dispatch_aaaa);
			continue;
		}
		if (strncasecmp("exists:", *ap, 7) == 0) {
			lookup_record(T_A, *(ap) + 7, dispatch_a);
			continue;
		}
		if (strncasecmp("include:", *ap, 8) == 0) {
			lookup_record(T_TXT, *(ap) + 8, dispatch_txt);
			continue;
		}
		if (strncasecmp("redirect=", *ap, 9) == 0) {
			lookup_record(T_TXT, *(ap) + 9, dispatch_txt);
			continue;
		}
		if (strcasecmp(*ap, "mx") == 0 || strcasecmp(*ap, "+mx") == 0) {
			print_dname(rr->rr_dname, buf2, sizeof(buf2));
			buf2[strlen(buf2) - 1] = '\0';
			lookup_record(T_MX, buf2, dispatch_mx);
			continue;
		}
		if (strcasecmp(*ap, "a") == 0 || strcasecmp(*ap, "+a") == 0) {
			print_dname(rr->rr_dname, buf2, sizeof(buf2));
			buf2[strlen(buf2) - 1] = '\0';
			lookup_record(T_A, buf2, dispatch_a);
			lookup_record(T_AAAA, buf2, dispatch_aaaa);
			continue;
		}
	}
	*ap = NULL;
}

void
dispatch_mx(struct dns_rr *rr)
{
	char buf[512];

	print_dname(rr->rr.mx.exchange, buf, sizeof(buf));
	buf[strlen(buf) - 1] = '\0';
	if (buf[strlen(buf) - 1] == '.')
		buf[strlen(buf) - 1] = '\0';
	lookup_record(T_A, buf, dispatch_a);
	lookup_record(T_AAAA, buf, dispatch_aaaa);
}

void
dispatch_a(struct dns_rr *rr)
{
	char buffer[512];
	const char *ptr;

	if ((ptr = inet_ntop(AF_INET, &rr->rr.in_a.addr,
	    buffer, sizeof buffer)))
		printf("%s\n", ptr);
}

void
dispatch_aaaa(struct dns_rr *rr)
{
	char buffer[512];
	const char *ptr;

	if ((ptr = inet_ntop(AF_INET6, &rr->rr.in_aaaa.addr6,
	    buffer, sizeof buffer)))
		printf("%s\n", ptr);
}

static ssize_t
parse_txt(const char *rdata, size_t rdatalen, char *dst, size_t dstsz)
{
	size_t len;
	ssize_t r = 0;

	while (rdatalen) {
		len = *(const unsigned char *)rdata;
		if (len >= rdatalen) {
			errno = EINVAL;
			return -1;
		}

		rdata++;
		rdatalen--;

		if (len == 0)
			continue;

		if (len >= dstsz) {
			errno = EOVERFLOW;
			return -1;
		}
		memmove(dst, rdata, len);
		dst += len;
		dstsz -= len;

		rdata += len;
		rdatalen -= len;
		r += len;
	}

	return r;
}
