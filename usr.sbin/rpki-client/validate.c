/*	$OpenBSD: validate.c,v 1.7 2019/11/27 17:08:12 benno Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/socket.h>

#include <arpa/inet.h>
#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>

#include "extern.h"

static void
tracewarn(size_t idx, const struct auth *auths, size_t authsz)
{

	for (;; idx = auths[idx].parent) {
		warnx(" ...inheriting from: %s", auths[idx].fn);
		if (auths[idx].parent == (size_t)idx)
			break;
	}
}

/*
 * Walk up the chain of certificates trying to match our AS number to
 * one of the allocations in that chain.
 * Returns the index of the certificate in auths or -1 on error.
 */
static ssize_t
valid_as(uint32_t min, uint32_t max,
    size_t idx, const struct auth *as, size_t asz)
{
	int	 c;

	assert(idx < asz);

	/* Does this certificate cover our AS number? */

	if (as[idx].cert->asz) {
		c = as_check_covered(min, max,
		    as[idx].cert->as, as[idx].cert->asz);
		if (c > 0)
			return idx;
		/*else if (c < 0)
			return -1;*/
	}

	/* If it doesn't, walk up the chain. */

	if (as[idx].parent == as[idx].id)
		return -1;
	return valid_as(min, max, as[idx].parent, as, asz);
}

/*
 * Walk up the chain of certificates (really just the last one, but in
 * the case of inheritence, the ones before) making sure that our IP
 * prefix is covered in the first non-inheriting specification.
 * Returns the index of the certificate in auths or -1 on error.
 */
static ssize_t
valid_ip(size_t idx, enum afi afi,
    const unsigned char *min, const unsigned char *max,
    const struct auth *as, size_t asz)
{
	int	 c;

	assert(idx < asz);

	/* Does this certificate cover our IP prefix? */

	c = ip_addr_check_covered(afi, min, max,
	    as[idx].cert->ips, as[idx].cert->ipsz);
	if (c > 0)
		return idx;
	else if (c < 0)
		return -1;

	/* If it doesn't, walk up the chain. */

	if (as[idx].parent == as[idx].id)
		return -1;
	return valid_ip(as[idx].parent, afi, min, max, as, asz);
}

/*
 * Authenticate a trust anchor by making sure its resources are not
 * inheriting and that the SKI is unique.
 * Returns *authsz or -1 on failure.
 */
ssize_t
valid_ta(const char *fn, const struct auth *auths,
    size_t authsz, const struct cert *cert)
{
	size_t	 i;

	/* AS and IP resources must not inherit. */

	if (cert->asz && cert->as[0].type == CERT_AS_INHERIT) {
		warnx("%s: RFC 6487 (trust anchor): "
		    "inheriting AS resources", fn);
		return -1;
	}
	for (i = 0; i < cert->ipsz; i++)
		if (cert->ips[i].type == CERT_IP_INHERIT) {
			warnx("%s: RFC 6487 (trust anchor): "
			    "inheriting IP resources", fn);
			return -1;
		}

	/* SKI must not be a dupe. */

	for (i = 0; i < authsz; i++)
		if (strcmp(auths[i].cert->ski, cert->ski) == 0) {
			warnx("%s: RFC 6487: duplicate SKI", fn);
			return -1;
		}

	return authsz;
}

/*
 * Make sure that the SKI doesn't already exist and return the parent by
 * its AKI.
 * Returns the parent index or -1 on failure.
 */
ssize_t
valid_ski_aki(const char *fn, const struct auth *auths,
    size_t authsz, const char *ski, const char *aki)
{
	size_t		 i;

	for (i = 0; i < authsz; i++)
		if (strcmp(auths[i].cert->ski, ski) == 0) {
			warnx("%s: RFC 6487: duplicate SKI", fn);
			return -1;
		}

	for (i = 0; i < authsz; i++)
		if (strcmp(auths[i].cert->ski, aki) == 0)
			return i;

	warnx("%s: RFC 6487: unknown AKI", fn);
	return -1;
}

/*
 * Validate a non-TA certificate: make sure its IP and AS resources are
 * fully covered by those in the authority key (which must exist).
 * Returns the parent index or -1 on failure.
 */
ssize_t
valid_cert(const char *fn, const struct auth *auths,
    size_t authsz, const struct cert *cert)
{
	ssize_t		 c, pp;
	size_t		 i;
	uint32_t	 min, max;
	char		 buf1[64], buf2[64];

	c = valid_ski_aki(fn, auths, authsz, cert->ski, cert->aki);
	if (c < 0)
		return -1;

	for (i = 0; i < cert->asz; i++) {
		if (cert->as[i].type == CERT_AS_INHERIT)
			continue;
		min = cert->as[i].type == CERT_AS_ID ?
		    cert->as[i].id : cert->as[i].range.min;
		max = cert->as[i].type == CERT_AS_ID ?
		    cert->as[i].id : cert->as[i].range.max;
		pp = valid_as(min, max, c, auths, authsz);
		if (pp >= 0)
			continue;
		warnx("%s: RFC 6487: uncovered AS: "
		    "%u--%u", fn, min, max);
		tracewarn(c, auths, authsz);
		return -1;
	}

	for (i = 0; i < cert->ipsz; i++) {
		pp = valid_ip(c, cert->ips[i].afi, cert->ips[i].min,
		    cert->ips[i].max, auths, authsz);
		if (pp >= 0)
			continue;
		switch (cert->ips[i].type) {
		case CERT_IP_RANGE:
			ip_addr_print(&cert->ips[i].range.min,
			    cert->ips[i].afi, buf1, sizeof(buf1));
			ip_addr_print(&cert->ips[i].range.max,
			    cert->ips[i].afi, buf2, sizeof(buf2));
			warnx("%s: RFC 6487: uncovered IP: "
			    "%s--%s", fn, buf1, buf2);
			break;
		case CERT_IP_ADDR:
			ip_addr_print(&cert->ips[i].ip,
			    cert->ips[i].afi, buf1, sizeof(buf1));
			warnx("%s: RFC 6487: uncovered IP: "
			    "%s", fn, buf1);
		case CERT_IP_INHERIT:
			warnx("%s: RFC 6487: uncovered IP: "
			    "(inherit)", fn);
			break;
		}
		tracewarn(c, auths, authsz);
		return -1;
	}

	return c;
}

/*
 * Validate our ROA: check that the SKI is unique, the AKI exists, and
 * the IP prefix is also contained.
 * Returns the parent index or -1 on failure.
 */
ssize_t
valid_roa(const char *fn, const struct auth *auths,
    size_t authsz, const struct roa *roa)
{
	ssize_t	 c, pp;
	size_t	 i;
	char	 buf[64];

	c = valid_ski_aki(fn, auths, authsz, roa->ski, roa->aki);
	if (c < 0)
		return -1;

	for (i = 0; i < roa->ipsz; i++) {
		pp = valid_ip(c, roa->ips[i].afi, roa->ips[i].min,
		    roa->ips[i].max, auths, authsz);
		if (pp >= 0)
			continue;
		ip_addr_print(&roa->ips[i].addr,
		    roa->ips[i].afi, buf, sizeof(buf));
		warnx("%s: RFC 6482: uncovered IP: "
		    "%s", fn, buf);
		tracewarn(c, auths, authsz);
		return -1;
	}

	return c;
}
