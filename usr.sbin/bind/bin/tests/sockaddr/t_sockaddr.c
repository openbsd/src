/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
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

/* $ISC: t_sockaddr.c,v 1.11 2001/01/09 21:42:14 bwelling Exp $ */

#include <config.h>

#include <isc/netaddr.h>
#include <isc/result.h>
#include <isc/sockaddr.h>

#include <tests/t_api.h>

static int
test_isc_sockaddr_eqaddrprefix(void) {
	struct in_addr ina_a;
	struct in_addr ina_b;
	struct in_addr ina_c;
	isc_sockaddr_t isa_a;
	isc_sockaddr_t isa_b;
	isc_sockaddr_t isa_c;

	if (inet_pton(AF_INET, "194.100.32.87", &ina_a) < 0)
		return T_FAIL;
	if (inet_pton(AF_INET, "194.100.32.80", &ina_b) < 0)
		return T_FAIL;
	if (inet_pton(AF_INET, "194.101.32.87", &ina_c) < 0)
		return T_FAIL;
	isc_sockaddr_fromin(&isa_a, &ina_a, 0);
	isc_sockaddr_fromin(&isa_b, &ina_b, 42);
	isc_sockaddr_fromin(&isa_c, &ina_c, 0);

	if (isc_sockaddr_eqaddrprefix(&isa_a, &isa_b, 0) != ISC_TRUE)
		return T_FAIL;
	if (isc_sockaddr_eqaddrprefix(&isa_a, &isa_b, 29) != ISC_TRUE)
		return T_FAIL;
	if (isc_sockaddr_eqaddrprefix(&isa_a, &isa_b, 30) != ISC_FALSE)
		return T_FAIL;
	if (isc_sockaddr_eqaddrprefix(&isa_a, &isa_b, 32) != ISC_FALSE)
		return T_FAIL;
	if (isc_sockaddr_eqaddrprefix(&isa_a, &isa_c, 8) != ISC_TRUE)
		return T_FAIL;
	if (isc_sockaddr_eqaddrprefix(&isa_a, &isa_c, 16) != ISC_FALSE)
		return T_FAIL;

	return T_PASS;
}

static void
t1(void) {
	int result;
	t_assert("isc_sockaddr_eqaddrprefix", 1, T_REQUIRED,
		 "isc_sockaddr_eqaddrprefix() returns ISC_TRUE when "
		 "prefixes of a and b are equal, and ISC_FALSE when "
		 "they are not equal");
	result = test_isc_sockaddr_eqaddrprefix();
	t_result(result);
}

static int
test_isc_netaddr_masktoprefixlen(void) {
	struct in_addr na_a;
	struct in_addr na_b;
	struct in_addr na_c;
	struct in_addr na_d;
	isc_netaddr_t ina_a;
	isc_netaddr_t ina_b;
	isc_netaddr_t ina_c;
	isc_netaddr_t ina_d;
	unsigned int plen;

	if (inet_pton(AF_INET, "0.0.0.0", &na_a) < 0)
		return T_FAIL;
	if (inet_pton(AF_INET, "255.255.255.254", &na_b) < 0)
		return T_FAIL;
	if (inet_pton(AF_INET, "255.255.255.255", &na_c) < 0)
		return T_FAIL;
	if (inet_pton(AF_INET, "255.255.255.0", &na_d) < 0)
		return T_FAIL;
	isc_netaddr_fromin(&ina_a, &na_a);
	isc_netaddr_fromin(&ina_b, &na_b);
	isc_netaddr_fromin(&ina_c, &na_c);
	isc_netaddr_fromin(&ina_d, &na_d);

	if (isc_netaddr_masktoprefixlen(&ina_a, &plen) != ISC_R_SUCCESS)
		return T_FAIL;
	if (plen != 0)
		return T_FAIL;

	if (isc_netaddr_masktoprefixlen(&ina_b, &plen) != ISC_R_SUCCESS)
		return T_FAIL;
	if (plen != 31)
		return T_FAIL;

	if (isc_netaddr_masktoprefixlen(&ina_c, &plen) != ISC_R_SUCCESS)
		return T_FAIL;
	if (plen != 32)
		return T_FAIL;

	if (isc_netaddr_masktoprefixlen(&ina_d, &plen) != ISC_R_SUCCESS)
		return T_FAIL;
	if (plen != 24)
		return T_FAIL;

	return T_PASS;
}

static void
t2(void) {
	int result;
	t_assert("isc_netaddr_masktoprefixlen", 1, T_REQUIRED,
		 "isc_netaddr_masktoprefixlen() calculates "
		 "correct prefix lengths ");
	result = test_isc_netaddr_masktoprefixlen();
	t_result(result);
}

testspec_t	T_testlist[] = {
	{	t1,	"isc_sockaddr_eqaddrprefix"	},
	{	t2,	"isc_netaddr_masktoprefixlen"	},
	{	NULL,	NULL				}
};

