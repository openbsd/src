/*
 * Copyright (C) 2000-2002  Internet Software Consortium.
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

/* $ISC: lwtest.c,v 1.22.2.2 2002/08/05 06:57:07 marka Exp $ */

#include <config.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <isc/net.h>

#include <lwres/lwres.h>
#include <lwres/netdb.h>
#include <lwres/net.h>

/*
 * XXX getnameinfo errors, which don't appear to be standard.
 */
#define ENI_NOSERVNAME  1
#define ENI_NOHOSTNAME  2
#define ENI_MEMORY      3
#define ENI_SYSTEM      4
#define ENI_FAMILY      5
#define ENI_SALEN       6
#define ENI_NOSOCKET    7

static int fails = 0;

static void
CHECK(lwres_result_t val, const char *msg) {
	if (val != 0) {
		printf("I:%s returned %d\n", msg, val);
		exit(1);
	}
}

static unsigned char TESTSTRING[] =
	"This is a test.  This is only a test.  !!!";

static lwres_context_t *ctx;

static void
test_noop(void) {
	lwres_result_t ret;
	lwres_lwpacket_t pkt, pkt2;
	lwres_nooprequest_t nooprequest, *nooprequest2;
	lwres_noopresponse_t noopresponse, *noopresponse2;
	lwres_buffer_t b;

	pkt.pktflags = 0;
	pkt.serial = 0x11223344;
	pkt.recvlength = 0x55667788;
	pkt.result = 0;

	nooprequest.datalength = strlen((char *)TESTSTRING);
	nooprequest.data = TESTSTRING;
	ret = lwres_nooprequest_render(ctx, &nooprequest, &pkt, &b);
	CHECK(ret, "lwres_nooprequest_render");

	/*
	 * Now, parse it into a new structure.
	 */
	lwres_buffer_first(&b);
	ret = lwres_lwpacket_parseheader(&b, &pkt2);
	CHECK(ret, "lwres_lwpacket_parseheader");

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

	noopresponse.datalength = strlen((char *)TESTSTRING);
	noopresponse.data = TESTSTRING;
	ret = lwres_noopresponse_render(ctx, &noopresponse, &pkt, &b);
	CHECK(ret, "lwres_noopresponse_render");

	/*
	 * Now, parse it into a new structure.
	 */
	lwres_buffer_first(&b);
	ret = lwres_lwpacket_parseheader(&b, &pkt2);
	CHECK(ret, "lwres_lwpacket_parseheader");

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
test_gabn(const char *target, lwres_result_t expected, const char *address,
	  lwres_uint32_t af)
{
	lwres_gabnresponse_t *res;
	unsigned char addrbuf[16];
	lwres_addr_t *addr;
	char outbuf[64];
	unsigned int len;
	lwres_result_t ret;

	res = NULL;
	ret = lwres_getaddrsbyname(ctx, target,
				   LWRES_ADDRTYPE_V4 | LWRES_ADDRTYPE_V6,
				   &res);
	if (ret != expected) {
		printf("I:gabn(%s) failed: %d\n", target, ret);
		if (res != NULL)
			lwres_gabnresponse_free(ctx, &res);
		fails++;
		return;
	}
	if (ret == LWRES_R_SUCCESS) {
		if (af == LWRES_ADDRTYPE_V4) {
			len = 4;
			ret = inet_pton(AF_INET, address, addrbuf);
			assert(ret == 1);
		} else {
			len = 16;
			ret = inet_pton(AF_INET6, address, addrbuf);
			assert(ret == 1);
		}
		addr = LWRES_LIST_HEAD(res->addrs);
		if (addr == NULL) {
			printf("I:gabn(%s) returned empty list\n", target);
			fails++;
			return;
		}
		while (addr != NULL) {
			if (addr->family != af || addr->length != len ||
			    memcmp(addr->address, addrbuf, len) == 0)
				break;
			addr = LWRES_LIST_NEXT(addr, link);
		}
		if (addr == NULL) {
			addr = LWRES_LIST_HEAD(res->addrs);
			if (addr->family == LWRES_ADDRTYPE_V4)
				(void)inet_ntop(AF_INET, addr->address,
						outbuf, sizeof(outbuf));
			else
				(void)inet_ntop(AF_INET6, addr->address,
						outbuf, sizeof(outbuf));
			printf("I:gabn(%s) returned %s, expected %s\n",
				target, outbuf, address);
			fails++;
			return;
		}
	}
	if (res != NULL)
		lwres_gabnresponse_free(ctx, &res);
}

static void
test_gnba(const char *target, lwres_uint32_t af, lwres_result_t expected,
	  const char *name)
{
	lwres_gnbaresponse_t *res;
	lwres_result_t ret;
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
	if (ret != expected) {
		printf("I:gnba(%s) failed: %d\n", target, ret);
		if (res != NULL)
			lwres_gnbaresponse_free(ctx, &res);
		fails++;
		return;
	}
	if (ret == LWRES_R_SUCCESS && strcasecmp(res->realname, name) != 0) {
		 printf("I:gnba(%s) returned %s, expected %s\n",
			target, res->realname, name);
		 fails++;
		 return;
	}
	if (res != NULL)
		lwres_gnbaresponse_free(ctx, &res);
}

static void
test_gethostbyname(const char *name, const char *address) {
	struct hostent *hp;
	unsigned char addrbuf[16];
	int ret;

	hp = gethostbyname(name);
	if (hp == NULL) {
		if (address == NULL && h_errno == HOST_NOT_FOUND)
			return;
		else if (h_errno != HOST_NOT_FOUND) {
			printf("I:gethostbyname(%s) failed: %s\n",
			       name, hstrerror(h_errno));
			fails++;
			return;
		} else {
			printf("I:gethostbyname(%s) returned not found\n",
			       name);
			fails++;
			return;
		}
	} else {
		ret = inet_pton(AF_INET, address, addrbuf);
		assert(ret == 1);
		if (memcmp(hp->h_addr_list[0], addrbuf, hp->h_length) != 0) {
			char outbuf[16];
			(void)inet_ntop(AF_INET, hp->h_addr_list[0],
					outbuf, sizeof(outbuf));
			printf("I:gethostbyname(%s) returned %s, "
			       "expected %s\n", name, outbuf, address);
			fails++;
			return;
		}
	}
}

static void
test_gethostbyname2(const char *name, const char *address, int af) {
	struct hostent *hp;
	unsigned char addrbuf[16];
	int len, ret;

	hp = gethostbyname2(name, af);
	if (hp == NULL) {
		if (address == NULL && h_errno == HOST_NOT_FOUND)
			return;
		else if (h_errno != HOST_NOT_FOUND) {
			printf("I:gethostbyname(%s) failed: %s\n",
			       name, hstrerror(h_errno));
			fails++;
			return;
		} else {
			printf("I:gethostbyname(%s) returned not found\n",
			       name);
			fails++;
			return;
		}
	} else {
		if (af == AF_INET)
			len = 4;
		else
			len = 16;
		ret = inet_pton(af, address, addrbuf);
		assert(ret == 1);
		if (hp->h_addrtype != af) {
			printf("I:gethostbyname(%s) returned wrong family\n",
			       name);
			fails++;
			return;
		}
		if (len != (int)hp->h_length ||
		    memcmp(hp->h_addr_list[0], addrbuf, hp->h_length) != 0)
		{
			char outbuf[16];
			(void)inet_ntop(af, hp->h_addr_list[0],
					outbuf, sizeof(outbuf));
			printf("I:gethostbyname(%s) returned %s, "
			       "expected %s\n", name, outbuf, address);
			fails++;
			return;
		}
	}
}

static void
test_getipnodebyname(const char *name, const char *address, int af,
		     int v4map, int all)
{
	struct hostent *hp;
	unsigned char addrbuf[16];
	int len, ret;
	int error_num;
	int flags = 0;

	if (v4map)
		flags |= AI_V4MAPPED;
	if (all)
		flags |= AI_ALL;

	hp = getipnodebyname(name, af, flags, &error_num);
	if (hp == NULL) {
		if (address == NULL && error_num == HOST_NOT_FOUND)
			return;
		else if (error_num != HOST_NOT_FOUND) {
			printf("I:getipnodebyname(%s) failed: %d\n",
			       name, error_num);
			fails++;
			return;
		} else {
			printf("I:getipnodebyname(%s) returned not found\n",
			       name);
			fails++;
			return;
		}
	} else {
		if (af == AF_INET)
			len = 4;
		else
			len = 16;
		ret = inet_pton(af, address, addrbuf);
		assert(ret == 1);
		if (hp->h_addrtype != af) {
			printf("I:getipnodebyname(%s) returned wrong family\n",
			       name);
			fails++;
			return;
		}
		if (len != (int)hp->h_length ||
		    memcmp(hp->h_addr_list[0], addrbuf, hp->h_length) != 0)
		{
			char outbuf[16];
			(void)inet_ntop(af, hp->h_addr_list[0],
					outbuf, sizeof(outbuf));
			printf("I:getipnodebyname(%s) returned %s, "
			       "expected %s\n", name, outbuf, address);
			fails++;
			return;
		}
		freehostent(hp);
	}
}

static void
test_gethostbyaddr(const char *address, int af, const char *name) {
	struct hostent *hp;
	char addrbuf[16];
	int len, ret;

	if (af == AF_INET)
		len = 4;
	else
		len = 16;
	ret = inet_pton(af, address, addrbuf);
	assert(ret == 1);

	hp = gethostbyaddr(addrbuf, len, af);

	if (hp == NULL) {
		if (name == NULL && h_errno == HOST_NOT_FOUND)
			return;
		else if (h_errno != HOST_NOT_FOUND) {
			printf("I:gethostbyaddr(%s) failed: %s\n",
			       address, hstrerror(h_errno));
			fails++;
			return;
		} else {
			printf("I:gethostbyaddr(%s) returned not found\n",
			       address);
			fails++;
			return;
		}
	} else {
		if (strcmp(hp->h_name, name) != 0) {
			printf("I:gethostbyname(%s) returned %s, "
			       "expected %s\n", address, hp->h_name, name);
			fails++;
			return;
		}
	}
}

static void
test_getipnodebyaddr(const char *address, int af, const char *name) {
	struct hostent *hp;
	char addrbuf[16];
	int len, ret;
	int error_num;

	if (af == AF_INET)
		len = 4;
	else
		len = 16;
	ret = inet_pton(af, address, addrbuf);
	assert(ret == 1);

	hp = getipnodebyaddr(addrbuf, len, af, &error_num);

	if (hp == NULL) {
		if (name == NULL && error_num == HOST_NOT_FOUND)
			return;
		else if (error_num != HOST_NOT_FOUND) {
			printf("I:gethostbyaddr(%s) failed: %d\n",
			       address, error_num);
			fails++;
			return;
		} else {
			printf("I:gethostbyaddr(%s) returned not found\n",
			       address);
			fails++;
			return;
		}
	} else {
		if (strcmp(hp->h_name, name) != 0) {
			printf("I:gethostbyname(%s) returned %s, "
			       "expected %s\n", address, hp->h_name, name);
			fails++;
			return;
		}
		freehostent(hp);
	}
}

static void
test_getaddrinfo(const char *name, int af, int v4ok, int v6ok,
		   const char *address)
{
	unsigned int len;
	int ret;
	struct addrinfo *ai;
	struct addrinfo hint;
	unsigned char addrbuf[16];

	if (v4ok == 1 && v6ok== 1) {
		ret = getaddrinfo(name, NULL, NULL, &ai);
	} else {
		memset(&hint, 0, sizeof(hint));
		if (v4ok)
			hint.ai_family = AF_INET;
		else
			hint.ai_family = AF_INET6;
		ret = getaddrinfo(name, NULL, &hint, &ai);
	}
	if (ret != 0) {
		if (address == NULL && ret == EAI_NODATA)
			return;
		else if (ret != EAI_NODATA) {
			printf("I:getaddrinfo(%s,%d,%d) failed: %s\n",
			       name, v4ok, v6ok, gai_strerror(ret));
			fails++;
			return;
		} else {
			printf("I:getaddrinfo(%s,%d,%d) returned not found\n",
			       name, v4ok, v6ok);
			fails++;
			return;
		}
	} else {
		if (af == AF_INET)
			len = sizeof(struct sockaddr_in);
		else
			len = sizeof(struct sockaddr_in6);
		ret = inet_pton(af, address, addrbuf);
		assert(ret == 1);
		if (ai->ai_family != af) {
			printf("I:getaddrinfo(%s) returned wrong family\n",
			       name);
			fails++;
			freeaddrinfo(ai);
			return;
		}
		if (len != (unsigned int) ai->ai_addrlen) {
			char outbuf[16];
			(void)inet_ntop(af, ai->ai_addr,
					outbuf, sizeof(outbuf));
			printf("I:getaddrinfo(%s) returned %lub, "
			       "expected %ub\n", name,
				(unsigned long)ai->ai_addrlen, len);
			fails++;
			freeaddrinfo(ai);
			return;
		} else if (af == AF_INET) {
			struct sockaddr_in *sin;
			sin = (struct sockaddr_in *) ai->ai_addr;
			if (memcmp(&sin->sin_addr.s_addr, addrbuf, 4) != 0) {
				char outbuf[16];
				(void)inet_ntop(af, &sin->sin_addr.s_addr,
						outbuf, sizeof(outbuf));
				printf("I:getaddrinfo(%s) returned %s, "
				       "expected %s\n", name, outbuf, address);
				fails++;
				freeaddrinfo(ai);
				return;
			}
		} else {
			struct sockaddr_in6 *sin6;
			sin6 = (struct sockaddr_in6 *) ai->ai_addr;
			if (memcmp(sin6->sin6_addr.s6_addr, addrbuf, 16) != 0)
			{
				char outbuf[16];
				(void)inet_ntop(af, &sin6->sin6_addr.s6_addr,
						outbuf, sizeof(outbuf));
				printf("I:getaddrinfo(%s) returned %s, "
				       "expected %s\n", name, outbuf, address);
				fails++;
				freeaddrinfo(ai);
				return;
			}
		}
		freeaddrinfo(ai);
	}
}

static void
test_getnameinfo(const char *address, int af, const char *name) {
	int ret;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr *sa;
	int salen;
	char host[1025];

	if (af == AF_INET) {
		memset(&sin, 0, sizeof(sin));
		ret = inet_pton(AF_INET, address, &sin.sin_addr.s_addr);
		assert(ret == 1);
		sin.sin_family = AF_INET;
#ifdef LWRES_PLATFORM_HAVESALEN
		sin.sin_len = sizeof(sin);
#endif
		sa = (struct sockaddr *) &sin;
		salen = sizeof(sin);
	} else {
		memset(&sin6, 0, sizeof(sin6));
		ret = inet_pton(AF_INET6, address, sin6.sin6_addr.s6_addr);
		assert(ret == 1);
		sin6.sin6_family = AF_INET6;
#ifdef LWRES_PLATFORM_HAVESALEN
		sin6.sin6_len = sizeof(sin6);
#endif
		sa = (struct sockaddr *) &sin6;
		salen = sizeof(sin6);
	}
	sa->sa_family = af;

	ret = getnameinfo(sa, salen, host, sizeof(host), NULL, 0, NI_NAMEREQD);

	if (ret != 0) {
		if (name == NULL && ret == ENI_NOHOSTNAME)
			return;
		else if (ret != ENI_NOHOSTNAME) {
			printf("I:getnameinfo(%s) failed: %d\n",
			       address, ret);
			fails++;
			return;
		} else {
			printf("I:getnameinfo(%s) returned not found\n",
			       address);
			fails++;
			return;
		}
	} else {
		if (name == NULL) {
			printf("I:getaddrinfo(%s) returned %s, "
			       "expected NULL\n", address, host);
			fails++;
			return;
		} else if (strcmp(host, name) != 0) {
			printf("I:getaddrinfo(%s) returned %s, expected %s\n",
			       address, host, name);
			fails++;
			return;
		}
	}
}

static void
test_getrrsetbyname(const char *name, int rdclass, int rdtype,
		    unsigned int nrdatas, unsigned int nsigs,
		    int should_pass)
{
	int ret;
	struct rrsetinfo *rrinfo = NULL;
	ret = getrrsetbyname(name, rdclass, rdtype, 0, &rrinfo);
	if (ret != 0 && should_pass == 1) {
		printf("I:getrrsetbyname(%s, %d) failed\n", name, rdtype);
		fails++;
		return;
	} else if (ret == 0 && should_pass == 0) {
		printf("I:getrrsetbyname(%s, %d) unexpectedly succeeded\n",
			name, rdtype);
		fails++;
		freerrset(rrinfo);
		return;
	} else if (ret != 0)
		return;
	if (rrinfo->rri_nrdatas != nrdatas) {
		printf("I:getrrsetbyname(%s, %d): got %d rr, expected %d\n",
			name, rdtype, rrinfo->rri_nrdatas, nrdatas);
		fails++;
	}
	if (rrinfo->rri_nsigs != nsigs) {
		printf("I:getrrsetbyname(%s, %d): got %d sig, expected %d\n",
			name, rdtype, rrinfo->rri_nsigs, nsigs);
		fails++;
	}
	freerrset(rrinfo);
	return;
}

int
main(void) {
	lwres_result_t ret;

	lwres_udp_port = 9210;
	lwres_resolv_conf = "resolv.conf";

	ret = lwres_context_create(&ctx, NULL, NULL, NULL, 0);
	CHECK(ret, "lwres_context_create");

	ret = lwres_conf_parse(ctx, "resolv.conf");
	CHECK(ret, "lwres_conf_parse");

	test_noop();

	test_gabn("a.example1", LWRES_R_SUCCESS, "10.0.1.1",
		  LWRES_ADDRTYPE_V4);
	test_gabn("a.example1.", LWRES_R_SUCCESS, "10.0.1.1",
		  LWRES_ADDRTYPE_V4);
	test_gabn("a.example2", LWRES_R_SUCCESS, "10.0.2.1",
		  LWRES_ADDRTYPE_V4);
	test_gabn("a.example2.", LWRES_R_SUCCESS, "10.0.2.1",
		  LWRES_ADDRTYPE_V4);
	test_gabn("a.example3", LWRES_R_NOTFOUND, NULL, LWRES_ADDRTYPE_V4);
	test_gabn("a.example3.", LWRES_R_NOTFOUND, NULL, LWRES_ADDRTYPE_V4);
	test_gabn("a", LWRES_R_SUCCESS, "10.0.1.1", LWRES_ADDRTYPE_V4);
	test_gabn("a.", LWRES_R_NOTFOUND, NULL, LWRES_ADDRTYPE_V4);

	test_gabn("a2", LWRES_R_SUCCESS, "10.0.1.1", LWRES_ADDRTYPE_V4);
	test_gabn("a3", LWRES_R_NOTFOUND, NULL, LWRES_ADDRTYPE_V4);

	test_gabn("b.example1", LWRES_R_SUCCESS,
		  "eeee:eeee:eeee:eeee:ffff:ffff:ffff:ffff",
		  LWRES_ADDRTYPE_V6);
	test_gabn("b.example1.", LWRES_R_SUCCESS,
		  "eeee:eeee:eeee:eeee:ffff:ffff:ffff:ffff",
		  LWRES_ADDRTYPE_V6);
	test_gabn("b.example2", LWRES_R_SUCCESS,
		  "eeee:eeee:eeee:eeee:ffff:ffff:ffff:ffff",
		  LWRES_ADDRTYPE_V6);
	test_gabn("b.example2.", LWRES_R_SUCCESS,
		  "eeee:eeee:eeee:eeee:ffff:ffff:ffff:ffff",
		  LWRES_ADDRTYPE_V6);
	test_gabn("b.example3", LWRES_R_NOTFOUND, NULL, LWRES_ADDRTYPE_V6);
	test_gabn("b.example3.", LWRES_R_NOTFOUND, NULL, LWRES_ADDRTYPE_V6);
	test_gabn("b", LWRES_R_SUCCESS,
		  "eeee:eeee:eeee:eeee:ffff:ffff:ffff:ffff",
		  LWRES_ADDRTYPE_V6);
	test_gabn("b.", LWRES_R_NOTFOUND, NULL, LWRES_ADDRTYPE_V6);

	test_gabn("d.example1", LWRES_R_NOTFOUND, NULL, LWRES_ADDRTYPE_V6);

	test_gabn("x", LWRES_R_SUCCESS, "10.1.10.1", LWRES_ADDRTYPE_V4);
	test_gabn("x.", LWRES_R_SUCCESS, "10.1.10.1", LWRES_ADDRTYPE_V4);

	test_gnba("10.10.10.1", LWRES_ADDRTYPE_V4, LWRES_R_SUCCESS,
		  "ipv4.example");
	test_gnba("10.10.10.17", LWRES_ADDRTYPE_V4, LWRES_R_NOTFOUND,
		  NULL);
	test_gnba("0123:4567:89ab:cdef:0123:4567:89ab:cdef",
		  LWRES_ADDRTYPE_V6, LWRES_R_SUCCESS, "nibble.example");
	test_gnba("0123:4567:89ab:cdef:0123:4567:89ab:cde0",
		  LWRES_ADDRTYPE_V6, LWRES_R_NOTFOUND, NULL);
	test_gnba("1123:4567:89ab:cdef:0123:4567:89ab:cdef",
		  LWRES_ADDRTYPE_V6, LWRES_R_SUCCESS, "bitstring.example");
	test_gnba("1123:4567:89ab:cdef:0123:4567:89ab:cde0",
		  LWRES_ADDRTYPE_V6, LWRES_R_NOTFOUND, NULL);

	test_gethostbyname("a.example1.", "10.0.1.1");
	test_gethostbyname("q.example1.", NULL);

	test_gethostbyname2("a.example1.", "10.0.1.1", AF_INET);
	test_gethostbyname2("b.example1.",
			    "eeee:eeee:eeee:eeee:ffff:ffff:ffff:ffff",
			    AF_INET6);
	test_gethostbyname2("q.example1.", NULL, AF_INET);

	test_getipnodebyname("a.example1.", "10.0.1.1", AF_INET, 0, 0);
	test_getipnodebyname("b.example1.",
			     "eeee:eeee:eeee:eeee:ffff:ffff:ffff:ffff",
			     AF_INET6, 0, 0);
	test_getipnodebyname("a.example1.",
			     "::ffff:10.0.1.1", AF_INET6, 1, 0);
	test_getipnodebyname("a.example1.",
			     "::ffff:10.0.1.1", AF_INET6, 1, 1);
	test_getipnodebyname("b.example1.",
			     "eeee:eeee:eeee:eeee:ffff:ffff:ffff:ffff",
			     AF_INET6, 1, 1);
	test_getipnodebyname("q.example1.", NULL, AF_INET, 0, 0);

	test_gethostbyaddr("10.10.10.1", AF_INET, "ipv4.example");
	test_gethostbyaddr("10.10.10.17", AF_INET, NULL);
	test_gethostbyaddr("0123:4567:89ab:cdef:0123:4567:89ab:cdef",
			   AF_INET6, "nibble.example");
	test_gethostbyaddr("1123:4567:89ab:cdef:0123:4567:89ab:cdef",
			   AF_INET6, "bitstring.example");

	test_getipnodebyaddr("10.10.10.1", AF_INET, "ipv4.example");
	test_getipnodebyaddr("10.10.10.17", AF_INET, NULL);
	test_getipnodebyaddr("0123:4567:89ab:cdef:0123:4567:89ab:cdef",
			     AF_INET6, "nibble.example");
	test_getipnodebyaddr("1123:4567:89ab:cdef:0123:4567:89ab:cdef",
			     AF_INET6, "bitstring.example");

	test_getaddrinfo("a.example1.", AF_INET, 1, 1, "10.0.1.1");
	test_getaddrinfo("a.example1.", AF_INET, 1, 0, "10.0.1.1");
	test_getaddrinfo("a.example1.", AF_INET, 0, 1, NULL);
	test_getaddrinfo("b.example1.", AF_INET6, 1, 1,
			 "eeee:eeee:eeee:eeee:ffff:ffff:ffff:ffff");
	test_getaddrinfo("b.example1.", AF_INET6, 1, 0, NULL);
	test_getaddrinfo("b.example1.", AF_INET6, 0, 1,
			 "eeee:eeee:eeee:eeee:ffff:ffff:ffff:ffff");

	test_getnameinfo("10.10.10.1", AF_INET, "ipv4.example");
	test_getnameinfo("10.10.10.17", AF_INET, NULL);
	test_getnameinfo("0123:4567:89ab:cdef:0123:4567:89ab:cdef",
			 AF_INET6, "nibble.example");
	test_getnameinfo("1123:4567:89ab:cdef:0123:4567:89ab:cdef",
			 AF_INET6, "bitstring.example");
	test_getnameinfo("1122:3344:5566:7788:99aa:bbcc:ddee:ff00",
			 AF_INET6, "dname.example1");

	test_getrrsetbyname("a", 1, 1, 1, 0, 1);
	test_getrrsetbyname("a.example1.", 1, 1, 1, 0, 1);
	test_getrrsetbyname("e.example1.", 1, 1, 1, 1, 1);
	test_getrrsetbyname("e.example1.", 1, 255, 1, 1, 0);
	test_getrrsetbyname("e.example1.", 1, 24, 1, 0, 1);
	test_getrrsetbyname("", 1, 1, 0, 0, 0);

	if (fails == 0)
		printf("I:ok\n");
	return (fails);
}
