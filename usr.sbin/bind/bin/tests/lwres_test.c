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

/* $ISC: lwres_test.c,v 1.25 2001/01/09 21:41:16 bwelling Exp $ */

#include <config.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <isc/mem.h>
#include <isc/netaddr.h>
#include <isc/util.h>

#include <lwres/lwres.h>

#define USE_ISC_MEM

static inline void
CHECK(int val, const char *msg) {
	if (val != 0) {
		fprintf(stderr, "%s returned %d\n", msg, val);
		exit(1);
	}
}

static void
hexdump(const char *msg, void *base, size_t len) {
	unsigned char *p;
	unsigned int cnt;

	p = base;
	cnt = 0;

	printf("*** %s (%u bytes @ %p)\n", msg, len, base);

	while (cnt < len) {
		if (cnt % 16 == 0)
			printf("%p: ", p);
		else if (cnt % 8 == 0)
			printf(" |");
		printf(" %02x", *p++);
		cnt++;

		if (cnt % 16 == 0)
			printf("\n");
	}

	if (cnt % 16 != 0)
		printf("\n");
}

static const char *TESTSTRING = "This is a test.  This is only a test.  !!!";
static lwres_context_t *ctx;

static void
test_noop(void) {
	int ret;
	lwres_lwpacket_t pkt, pkt2;
	lwres_nooprequest_t nooprequest, *nooprequest2;
	lwres_noopresponse_t noopresponse, *noopresponse2;
	lwres_buffer_t b;

	pkt.pktflags = 0;
	pkt.serial = 0x11223344;
	pkt.recvlength = 0x55667788;
	pkt.result = 0;

	nooprequest.datalength = strlen(TESTSTRING);
	/* XXXDCL maybe "nooprequest.data" should be const. */
	DE_CONST(TESTSTRING, nooprequest.data);
	ret = lwres_nooprequest_render(ctx, &nooprequest, &pkt, &b);
	CHECK(ret, "lwres_nooprequest_render");

	hexdump("rendered noop request", b.base, b.used);

	/*
	 * Now, parse it into a new structure.
	 */
	lwres_buffer_first(&b);
	ret = lwres_lwpacket_parseheader(&b, &pkt2);
	CHECK(ret, "lwres_lwpacket_parseheader");

	hexdump("parsed pkt2", &pkt2, sizeof(pkt2));

	nooprequest2 = NULL;
	ret = lwres_nooprequest_parse(ctx, &b, &pkt2, &nooprequest2);
	CHECK(ret, "lwres_nooprequest_parse");

	assert(nooprequest.datalength == nooprequest2->datalength);
	assert(memcmp(nooprequest.data, nooprequest2->data,
		       nooprequest.datalength) == 0);

	lwres_nooprequest_free(ctx, &nooprequest2);

	lwres_context_freemem(ctx, b.base, b.length);
	b.base = NULL;
	b.length = 0;

	pkt.pktflags = 0;
	pkt.serial = 0x11223344;
	pkt.recvlength = 0x55667788;
	pkt.result = 0xdeadbeef;

	noopresponse.datalength = strlen(TESTSTRING);
	/* XXXDCL maybe "noopresponse.data" should be const. */
	DE_CONST(TESTSTRING, noopresponse.data);
	ret = lwres_noopresponse_render(ctx, &noopresponse, &pkt, &b);
	CHECK(ret, "lwres_noopresponse_render");

	hexdump("rendered noop response", b.base, b.used);

	/*
	 * Now, parse it into a new structure.
	 */
	lwres_buffer_first(&b);
	ret = lwres_lwpacket_parseheader(&b, &pkt2);
	CHECK(ret, "lwres_lwpacket_parseheader");

	hexdump("parsed pkt2", &pkt2, sizeof(pkt2));

	noopresponse2 = NULL;
	ret = lwres_noopresponse_parse(ctx, &b, &pkt2, &noopresponse2);
	CHECK(ret, "lwres_noopresponse_parse");

	assert(noopresponse.datalength == noopresponse2->datalength);
	assert(memcmp(noopresponse.data, noopresponse2->data,
		       noopresponse.datalength) == 0);

	lwres_noopresponse_free(ctx, &noopresponse2);

	lwres_context_freemem(ctx, b.base, b.length);
	b.base = NULL;
	b.length = 0;
}

static void
test_gabn(const char *target) {
	lwres_gabnresponse_t *res;
	lwres_addr_t *addr;
	int ret;
	unsigned int i;
	char outbuf[64];

	res = NULL;
	ret = lwres_getaddrsbyname(ctx, target,
				   LWRES_ADDRTYPE_V4 | LWRES_ADDRTYPE_V6,
				   &res);
	printf("gabn %s ret == %d\n", target, ret);
	if (ret != 0) {
		printf("FAILURE!\n");
		if (res != NULL)
			lwres_gabnresponse_free(ctx, &res);
		return;
	}

	printf("Returned real name: (%u, %s)\n",
	       res->realnamelen, res->realname);
	printf("%u aliases:\n", res->naliases);
	for (i = 0 ; i < res->naliases ; i++)
		printf("\t(%u, %s)\n", res->aliaslen[i], res->aliases[i]);
	printf("%u addresses:\n", res->naddrs);
	addr = LWRES_LIST_HEAD(res->addrs);
	for (i = 0 ; i < res->naddrs ; i++) {
		INSIST(addr != NULL);

		if (addr->family == LWRES_ADDRTYPE_V4)
			(void)inet_ntop(AF_INET, addr->address,
					outbuf, sizeof(outbuf));
		else
			(void)inet_ntop(AF_INET6, addr->address,
					outbuf, sizeof(outbuf));
		printf("\tAddr len %u family %08x %s\n",
		       addr->length, addr->family, outbuf);
		addr = LWRES_LIST_NEXT(addr, link);
	}

	lwres_gabnresponse_free(ctx, &res);
}

static void
test_gnba(const char *target, lwres_uint32_t af) {
	lwres_gnbaresponse_t *res;
	int ret;
	unsigned int i;
	unsigned char addrbuf[16];
	unsigned int len;

	if (af == LWRES_ADDRTYPE_V4) {
		len = 4;
		ret = inet_pton(AF_INET, target, addrbuf);
		assert(ret == 1);
	} else {
		len = 16;
		ret = inet_pton(AF_INET6, target, addrbuf);
		assert(ret == 1);
	}

	res = NULL;
	ret = lwres_getnamebyaddr(ctx, af, len, addrbuf, &res);
	printf("gnba %s ret == %d\n", target, ret);
	assert(ret == 0);
	assert(res != NULL);

	printf("Returned real name: (%u, %s)\n",
	       res->realnamelen, res->realname);
	printf("%u aliases:\n", res->naliases);
	for (i = 0 ; i < res->naliases ; i++)
		printf("\t(%u, %s)\n", res->aliaslen[i], res->aliases[i]);

	lwres_gnbaresponse_free(ctx, &res);
}

#ifdef USE_ISC_MEM
/*
 * Wrappers around our memory management stuff, for the lwres functions.
 */
static void *
mem_alloc(void *arg, size_t size) {
	return (isc_mem_get(arg, size));
}

static void
mem_free(void *arg, void *mem, size_t size) {
	isc_mem_put(arg, mem, size);
}
#endif

int
main(int argc, char *argv[]) {
	int ret;
#ifdef USE_ISC_MEM
	isc_mem_t *mem;
	isc_result_t result;
#endif

	(void)argc;
	(void)argv;

#ifdef USE_ISC_MEM
	mem = NULL;
	result = isc_mem_create(0, 0, &mem);
	INSIST(result == ISC_R_SUCCESS);
#endif

	ctx = NULL;
#ifdef USE_ISC_MEM
	ret = lwres_context_create(&ctx, mem, mem_alloc, mem_free, 0);
#else
	ret = lwres_context_create(&ctx, NULL, NULL, NULL, 0);
#endif

	CHECK(ret, "lwres_context_create");

	ret = lwres_conf_parse(ctx, "/etc/resolv.conf");
	CHECK(ret, "lwres_conf_parse");

	lwres_conf_print(ctx, stdout);

	test_noop();

	/*
	 * The following comments about tests all assume your search path is
	 *	nominum.com isc.org flame.org
	 * and ndots is the default of 1.
	 */
	test_gabn("alias-05.test"); /* exact, then search. */
	test_gabn("f.root-servers.net.");
	test_gabn("poofball.flame.org.");
	test_gabn("foo.ip6.int.");
	test_gabn("notthereatall.flame.org");  /* exact, then search (!found)*/
	test_gabn("shell"); /* search (found in nominum.com), then exact */
	test_gabn("kechara"); /* search (found in flame.org), then exact */
	test_gabn("lkasdjlaksjdlkasjdlkasjdlkasjd"); /* search, exact(!found)*/

	test_gnba("198.133.199.1", LWRES_ADDRTYPE_V4);
	test_gnba("204.152.184.79", LWRES_ADDRTYPE_V4);
	test_gnba("3ffe:8050:201:1860:42::1", LWRES_ADDRTYPE_V6);

	lwres_conf_clear(ctx);
	lwres_context_destroy(&ctx);

#ifdef USE_ISC_MEM
	isc_mem_stats(mem, stdout);
	isc_mem_destroy(&mem);
#endif

	return (0);
}
