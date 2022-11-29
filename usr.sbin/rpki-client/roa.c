/*	$OpenBSD: roa.c,v 1.58 2022/11/29 20:41:32 job Exp $ */
/*
 * Copyright (c) 2022 Theo Buehler <tb@openbsd.org>
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

#include <assert.h>
#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/stack.h>
#include <openssl/safestack.h>
#include <openssl/x509.h>

#include "extern.h"

/*
 * Parse results and data of the manifest file.
 */
struct	parse {
	const char	 *fn; /* manifest file name */
	struct roa	 *res; /* results */
};

extern ASN1_OBJECT	*roa_oid;

/*
 * Types and templates for the ROA eContent, RFC 6482, section 3.
 */

typedef struct {
	ASN1_BIT_STRING		*address;
	ASN1_INTEGER		*maxLength;
} ROAIPAddress;

DECLARE_STACK_OF(ROAIPAddress);

typedef struct {
	ASN1_OCTET_STRING	*addressFamily;
	STACK_OF(ROAIPAddress)	*addresses;
} ROAIPAddressFamily;

DECLARE_STACK_OF(ROAIPAddressFamily);

#ifndef DEFINE_STACK_OF
#define sk_ROAIPAddress_num(st)		SKM_sk_num(ROAIPAddress, (st))
#define sk_ROAIPAddress_value(st, i)	SKM_sk_value(ROAIPAddress, (st), (i))

#define sk_ROAIPAddressFamily_num(st)	SKM_sk_num(ROAIPAddressFamily, (st))
#define sk_ROAIPAddressFamily_value(st, i) \
    SKM_sk_value(ROAIPAddressFamily, (st), (i))
#endif

typedef struct {
	ASN1_INTEGER			*version;
	ASN1_INTEGER			*asid;
	STACK_OF(ROAIPAddressFamily)	*ipAddrBlocks;
} RouteOriginAttestation;

ASN1_SEQUENCE(ROAIPAddress) = {
	ASN1_SIMPLE(ROAIPAddress, address, ASN1_BIT_STRING),
	ASN1_OPT(ROAIPAddress, maxLength, ASN1_INTEGER),
} ASN1_SEQUENCE_END(ROAIPAddress);

ASN1_SEQUENCE(ROAIPAddressFamily) = {
	ASN1_SIMPLE(ROAIPAddressFamily, addressFamily, ASN1_OCTET_STRING),
	ASN1_SEQUENCE_OF(ROAIPAddressFamily, addresses, ROAIPAddress),
} ASN1_SEQUENCE_END(ROAIPAddressFamily);

ASN1_SEQUENCE(RouteOriginAttestation) = {
	ASN1_EXP_OPT(RouteOriginAttestation, version, ASN1_INTEGER, 0),
	ASN1_SIMPLE(RouteOriginAttestation, asid, ASN1_INTEGER),
	ASN1_SEQUENCE_OF(RouteOriginAttestation, ipAddrBlocks,
	    ROAIPAddressFamily),
} ASN1_SEQUENCE_END(RouteOriginAttestation);

DECLARE_ASN1_FUNCTIONS(RouteOriginAttestation);
IMPLEMENT_ASN1_FUNCTIONS(RouteOriginAttestation);

/*
 * Parses the eContent section of an ROA file, RFC 6482, section 3.
 * Returns zero on failure, non-zero on success.
 */
static int
roa_parse_econtent(const unsigned char *d, size_t dsz, struct parse *p)
{
	RouteOriginAttestation		*roa;
	const ROAIPAddressFamily	*addrfam;
	const STACK_OF(ROAIPAddress)	*addrs;
	int				 addrsz;
	enum afi			 afi;
	const ROAIPAddress		*addr;
	long				 maxlen;
	struct ip_addr			 ipaddr;
	struct roa_ip			*res;
	int				 ipaddrblocksz;
	int				 i, j, rc = 0;

	if ((roa = d2i_RouteOriginAttestation(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RFC 6482 section 3: failed to parse "
		    "RouteOriginAttestation", p->fn);
		goto out;
	}

	if (!valid_econtent_version(p->fn, roa->version))
		goto out;

	if (!as_id_parse(roa->asid, &p->res->asid)) {
		warnx("%s: RFC 6482 section 3.2: asID: "
		    "malformed AS identifier", p->fn);
		goto out;
	}

	ipaddrblocksz = sk_ROAIPAddressFamily_num(roa->ipAddrBlocks);
	if (ipaddrblocksz > 2) {
		warnx("%s: draft-rfc6482bis: too many ipAddrBlocks "
		    "(got %d, expected 1 or 2)", p->fn, ipaddrblocksz);
		goto out;
	}

	for (i = 0; i < ipaddrblocksz; i++) {
		addrfam = sk_ROAIPAddressFamily_value(roa->ipAddrBlocks, i);
		addrs = addrfam->addresses;
		addrsz = sk_ROAIPAddress_num(addrs);

		if (!ip_addr_afi_parse(p->fn, addrfam->addressFamily, &afi)) {
			warnx("%s: RFC 6482 section 3.3: addressFamily: "
			    "invalid", p->fn);
			goto out;
		}

		if (p->res->ipsz + addrsz >= MAX_IP_SIZE) {
			warnx("%s: too many ROAIPAddress entries: limit %d",
			    p->fn, MAX_IP_SIZE);
			goto out;
		}
		p->res->ips = recallocarray(p->res->ips, p->res->ipsz,
		    p->res->ipsz + addrsz, sizeof(struct roa_ip));
		if (p->res->ips == NULL)
			err(1, NULL);

		for (j = 0; j < addrsz; j++) {
			addr = sk_ROAIPAddress_value(addrs, j);

			if (!ip_addr_parse(addr->address, afi, p->fn,
			    &ipaddr)) {
				warnx("%s: RFC 6482 section 3.3: address: "
				    "invalid IP address", p->fn);
				goto out;
			}
			maxlen = ipaddr.prefixlen;

			if (addr->maxLength != NULL) {
				maxlen = ASN1_INTEGER_get(addr->maxLength);
				if (maxlen < 0) {
					warnx("%s: RFC 6482 section 3.2: "
					    "ASN1_INTEGER_get failed", p->fn);
					goto out;
				}
				if (ipaddr.prefixlen > maxlen) {
					warnx("%s: prefixlen (%d) larger than "
					    "maxLength (%ld)", p->fn,
					    ipaddr.prefixlen, maxlen);
					goto out;
				}
				if (maxlen > ((afi == AFI_IPV4) ? 32 : 128)) {
					warnx("%s: maxLength (%ld) too large",
					    p->fn, maxlen);
					goto out;
				}
			}

			res = &p->res->ips[p->res->ipsz++];
			res->addr = ipaddr;
			res->afi = afi;
			res->maxlength = maxlen;
			ip_roa_compose_ranges(res);
		}
	}

	rc = 1;
 out:
	RouteOriginAttestation_free(roa);
	return rc;
}

/*
 * Parse a full RFC 6482 file.
 * Returns the ROA or NULL if the document was malformed.
 */
struct roa *
roa_parse(X509 **x509, const char *fn, const unsigned char *der, size_t len)
{
	struct parse	 p;
	size_t		 cmsz;
	unsigned char	*cms;
	const ASN1_TIME	*at;
	struct cert	*cert = NULL;
	int		 rc = 0;

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;

	cms = cms_parse_validate(x509, fn, der, len, roa_oid, &cmsz);
	if (cms == NULL)
		return NULL;

	if ((p.res = calloc(1, sizeof(struct roa))) == NULL)
		err(1, NULL);

	if (!x509_get_aia(*x509, fn, &p.res->aia))
		goto out;
	if (!x509_get_aki(*x509, fn, &p.res->aki))
		goto out;
	if (!x509_get_sia(*x509, fn, &p.res->sia))
		goto out;
	if (!x509_get_ski(*x509, fn, &p.res->ski))
		goto out;
	if (p.res->aia == NULL || p.res->aki == NULL || p.res->sia == NULL ||
	    p.res->ski == NULL) {
		warnx("%s: RFC 6487 section 4.8: "
		    "missing AIA, AKI, SIA, or SKI X509 extension", fn);
		goto out;
	}

	at = X509_get0_notAfter(*x509);
	if (at == NULL) {
		warnx("%s: X509_get0_notAfter failed", fn);
		goto out;
	}
	if (!x509_get_time(at, &p.res->expires)) {
		warnx("%s: ASN1_time_parse failed", fn);
		goto out;
	}

	if (!roa_parse_econtent(cms, cmsz, &p))
		goto out;

	if (x509_any_inherits(*x509)) {
		warnx("%s: inherit elements not allowed in EE cert", fn);
		goto out;
	}

	if ((cert = cert_parse_ee_cert(fn, *x509)) == NULL)
		goto out;

	if (cert->asz > 0) {
		warnx("%s: superfluous AS Resources extension present", fn);
		goto out;
	}

	/*
	 * If the ROA isn't valid, we accept it anyway and depend upon
	 * the code around roa_read() to check the "valid" field itself.
	 */
	p.res->valid = valid_roa(fn, cert, p.res);

	rc = 1;
out:
	if (rc == 0) {
		roa_free(p.res);
		p.res = NULL;
		X509_free(*x509);
		*x509 = NULL;
	}
	cert_free(cert);
	free(cms);
	return p.res;
}

/*
 * Free an ROA pointer.
 * Safe to call with NULL.
 */
void
roa_free(struct roa *p)
{

	if (p == NULL)
		return;
	free(p->aia);
	free(p->aki);
	free(p->sia);
	free(p->ski);
	free(p->ips);
	free(p);
}

/*
 * Serialise parsed ROA content.
 * See roa_read() for reader.
 */
void
roa_buffer(struct ibuf *b, const struct roa *p)
{
	io_simple_buffer(b, &p->valid, sizeof(p->valid));
	io_simple_buffer(b, &p->asid, sizeof(p->asid));
	io_simple_buffer(b, &p->talid, sizeof(p->talid));
	io_simple_buffer(b, &p->ipsz, sizeof(p->ipsz));
	io_simple_buffer(b, &p->expires, sizeof(p->expires));

	io_simple_buffer(b, p->ips, p->ipsz * sizeof(p->ips[0]));

	io_str_buffer(b, p->aia);
	io_str_buffer(b, p->aki);
	io_str_buffer(b, p->ski);
}

/*
 * Read parsed ROA content from descriptor.
 * See roa_buffer() for writer.
 * Result must be passed to roa_free().
 */
struct roa *
roa_read(struct ibuf *b)
{
	struct roa	*p;

	if ((p = calloc(1, sizeof(struct roa))) == NULL)
		err(1, NULL);

	io_read_buf(b, &p->valid, sizeof(p->valid));
	io_read_buf(b, &p->asid, sizeof(p->asid));
	io_read_buf(b, &p->talid, sizeof(p->talid));
	io_read_buf(b, &p->ipsz, sizeof(p->ipsz));
	io_read_buf(b, &p->expires, sizeof(p->expires));

	if ((p->ips = calloc(p->ipsz, sizeof(struct roa_ip))) == NULL)
		err(1, NULL);
	io_read_buf(b, p->ips, p->ipsz * sizeof(p->ips[0]));

	io_read_str(b, &p->aia);
	io_read_str(b, &p->aki);
	io_read_str(b, &p->ski);
	assert(p->aia && p->aki && p->ski);

	return p;
}

/*
 * Add each IP address in the ROA into the VRP tree.
 * Updates "vrps" to be the number of VRPs and "uniqs" to be the unique
 * number of addresses.
 */
void
roa_insert_vrps(struct vrp_tree *tree, struct roa *roa, size_t *vrps,
    size_t *uniqs)
{
	struct vrp	*v, *found;
	size_t		 i;

	for (i = 0; i < roa->ipsz; i++) {
		if ((v = malloc(sizeof(*v))) == NULL)
			err(1, NULL);
		v->afi = roa->ips[i].afi;
		v->addr = roa->ips[i].addr;
		v->maxlength = roa->ips[i].maxlength;
		v->asid = roa->asid;
		v->talid = roa->talid;
		v->expires = roa->expires;

		/*
		 * Check if a similar VRP already exists in the tree.
		 * If the found VRP expires sooner, update it to this
		 * ROAs later expiry moment.
		 */
		if ((found = RB_INSERT(vrp_tree, tree, v)) != NULL) {
			/* already exists */
			if (found->expires < v->expires) {
				/* update found with preferred data */
				found->talid = v->talid;
				found->expires = v->expires;
			}
			free(v);
		} else
			(*uniqs)++;

		(*vrps)++;
	}
}

static inline int
vrpcmp(struct vrp *a, struct vrp *b)
{
	int rv;

	if (a->afi > b->afi)
		return 1;
	if (a->afi < b->afi)
		return -1;
	switch (a->afi) {
	case AFI_IPV4:
		rv = memcmp(&a->addr.addr, &b->addr.addr, 4);
		if (rv)
			return rv;
		break;
	case AFI_IPV6:
		rv = memcmp(&a->addr.addr, &b->addr.addr, 16);
		if (rv)
			return rv;
		break;
	default:
		break;
	}
	/* a smaller prefixlen is considered bigger, e.g. /8 vs /10 */
	if (a->addr.prefixlen < b->addr.prefixlen)
		return 1;
	if (a->addr.prefixlen > b->addr.prefixlen)
		return -1;
	if (a->maxlength < b->maxlength)
		return 1;
	if (a->maxlength > b->maxlength)
		return -1;

	if (a->asid > b->asid)
		return 1;
	if (a->asid < b->asid)
		return -1;

	return 0;
}

RB_GENERATE(vrp_tree, vrp, entry, vrpcmp);
