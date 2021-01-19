/*	$OpenBSD: rde_decide_test.c,v 1.1 2021/01/19 16:04:46 claudio Exp $ */

/*
 * Copyright (c) 2020 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/queue.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rde.h"

struct rde_memstats rdemem;

struct rib dummy_rib = {
	.name = "regress RIB",
	.flags = 0,
};

struct rib_entry dummy_re;

struct nexthop nh_reach = {
	.state = NEXTHOP_REACH
};
struct nexthop nh_unreach = {
	.state = NEXTHOP_UNREACH
};

struct rde_peer peer1 = {
	.conf.ebgp = 1,
	.remote_bgpid = 1,
	.remote_addr = { .aid = AID_INET, .v4.s_addr = 0xef000001 },
};
struct rde_peer peer2 = {
	.conf.ebgp = 1,
	.remote_bgpid = 2,
	.remote_addr = { .aid = AID_INET, .v4.s_addr = 0xef000002 },
};
struct rde_peer peer3 = {
	.conf.ebgp = 0,
	.remote_bgpid = 3,
	.remote_addr = { .aid = AID_INET, .v4.s_addr = 0xef000003 },
};
struct rde_peer peer4 = {
	.conf.ebgp = 1,
	.remote_bgpid = 1,
	.remote_addr = { .aid = AID_INET, .v4.s_addr = 0xef000004 },
};

struct a {
	struct aspath	a;
	uint8_t 	d[5];
} asdata[] = {
	{ .a = { .data = { 2 }, .len = 6, .ascnt = 2 }, .d = { 1, 0, 0, 0, 1 } },
	{ .a = { .data = { 2 }, .len = 6, .ascnt = 3 }, .d = { 1, 0, 0, 0, 1 } },
	{ .a = { .data = { 2 }, .len = 6, .ascnt = 2 }, .d = { 1, 0, 0, 0, 2 } },
	{ .a = { .data = { 2 }, .len = 6, .ascnt = 3 }, .d = { 1, 0, 0, 0, 2 } },
};

struct rde_aspath asp[] = {
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 100, .origin = ORIGIN_IGP, .weight = 1000 },
	/* 1 & 2: errors and loops */
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 100, .origin = ORIGIN_IGP, .flags=F_ATTR_PARSE_ERR },
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 100, .origin = ORIGIN_IGP, .flags=F_ATTR_LOOP },
	/* 3: local preference */
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 50, .origin = ORIGIN_IGP },
	/* 4: aspath count */
	{ .aspath = &asdata[1].a, .med = 100, .lpref = 100, .origin = ORIGIN_IGP },
	/* 5 & 6: origin */
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 100, .origin = ORIGIN_EGP },
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 100, .origin = ORIGIN_INCOMPLETE },
	/* 7: MED */
	{ .aspath = &asdata[0].a, .med = 200, .lpref = 100, .origin = ORIGIN_IGP },
	/* 8: Weight */
	{ .aspath = &asdata[0].a, .med = 100, .lpref = 100, .origin = ORIGIN_IGP, .weight = 100 },


};

#define T1	1610980000
#define T2	1610983600

struct test {
	char *what;
	struct prefix p;
} testpfx[] = {
	{ .what = "test prefix",
	.p = { .re = &dummy_re, .aspath = &asp[0], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* pathes with errors are not eligible */
	{ .what = "prefix with error",
	.p = { .re = &dummy_re, .aspath = &asp[1], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* only loop free pathes are eligible */
	{ .what = "prefix with loop",
	.p = { .re = &dummy_re, .aspath = &asp[2], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 1. check if prefix is eligible a.k.a reachable */
	{ .what = "prefix with unreachable nexthop",
	.p = { .re = &dummy_re, .aspath = &asp[0], .peer = &peer1, .nexthop = &nh_unreach, .lastchange = T1, } },
	/* 2. local preference of prefix, bigger is better */
	{ .what = "local preference check",
	.p = { .re = &dummy_re, .aspath = &asp[3], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 3. aspath count, the shorter the better */
	{ .what = "aspath count check",
	.p = { .re = &dummy_re, .aspath = &asp[4], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 4. origin, the lower the better */
	{ .what = "origin EGP",
	.p = { .re = &dummy_re, .aspath = &asp[5], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	{ .what = "origin INCOMPLETE",
	.p = { .re = &dummy_re, .aspath = &asp[6], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 5. MED decision */
	{ .what = "MED",
	.p = { .re = &dummy_re, .aspath = &asp[7], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 6. EBGP is cooler than IBGP */
	{ .what = "EBGP vs IBGP",
	.p = { .re = &dummy_re, .aspath = &asp[0], .peer = &peer3, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 7. weight */
	{ .what = "local weight",
	.p = { .re = &dummy_re, .aspath = &asp[8], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 8. nexthop cost not implemented */
	/* 9. route age */
	{ .what = "route age",
	.p = { .re = &dummy_re, .aspath = &asp[0], .peer = &peer1, .nexthop = &nh_reach, .lastchange = T2, } },
	/* 10. BGP Id or ORIGINATOR_ID if present */
	{ .what = "BGP ID",
	.p = { .re = &dummy_re, .aspath = &asp[0], .peer = &peer2, .nexthop = &nh_reach, .lastchange = T1, } },
	/* 11. CLUSTER_LIST length, TODO */
	/* 12. lowest peer address wins */
	{ .what = "remote peer address",
	.p = { .re = &dummy_re, .aspath = &asp[0], .peer = &peer4, .nexthop = &nh_reach, .lastchange = T1, } },
};

int     prefix_cmp(struct prefix *, struct prefix *);

int
main(int argc, char **argv)
{
	size_t i, ntest;;

	ntest = sizeof(testpfx) / sizeof(*testpfx);
	for (i = 1; i < ntest; i++) {
		if (prefix_cmp(&testpfx[0].p, &testpfx[i].p) < 0)
			errx(1, "prefix_cmp check #%zu failed: %s", i, testpfx[i].what);
		if (prefix_cmp(&testpfx[i].p, &testpfx[0].p) > 0)
			errx(1, "reverse prefix_cmp check #%zu failed: %s", i, testpfx[i].what);
		printf("test %zu: %s OK\n", i, testpfx[i].what);
	}

	printf("test NULL element in prefix_cmp\n");
	if (prefix_cmp(&testpfx[0].p, NULL) < 0)
		errx(1, "NULL check #1 failed");
	if (prefix_cmp(NULL, &testpfx[0].p) > 0)
		errx(1, "NULL check #2 failed");

	printf("OK\n");
	exit(0);
}

int
rde_decisionflags(void)
{
	return BGPD_FLAG_DECISION_ROUTEAGE;
}

u_int32_t
rde_local_as(void)
{
	return 65000;
}

int
as_set_match(const struct as_set *aset, u_int32_t asnum)
{
	errx(1, __func__);
}

struct rib *
rib_byid(u_int16_t id)
{
	return &dummy_rib;
}

void
rde_generate_updates(struct rib *rib, struct prefix *new, struct prefix *old)
{
	/* maybe we want to do something here */
}

__dead void
fatalx(const char *emsg, ...)
{
	va_list ap;
	va_start(ap, emsg);
	verrx(2, emsg, ap);
}

__dead void
fatal(const char *emsg, ...)
{
	va_list ap;
	va_start(ap, emsg);
	verr(2, emsg, ap);
}

void
log_warnx(const char *emsg, ...)
{
	va_list  ap;
	va_start(ap, emsg);
	vwarnx(emsg, ap);
	va_end(ap);
}

