/*	$OpenBSD: roa.c,v 1.8 2019/11/29 05:14:11 benno Exp $ */
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

#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>

#include "extern.h"

/*
 * Parse results and data of the manifest file.
 */
struct	parse {
	const char	 *fn; /* manifest file name */
	struct roa	 *res; /* results */
};

/*
 * Parse IP address (ROAIPAddress), RFC 6482, section 3.3.
 * Returns zero on failure, non-zero on success.
 */
static int
roa_parse_addr(const ASN1_OCTET_STRING *os, enum afi afi, struct parse *p)
{
	ASN1_SEQUENCE_ANY	*seq;
	const unsigned char	*d = os->data;
	size_t			 dsz = os->length;
	int			 rc = 0;
	const ASN1_TYPE		*t;
	const ASN1_INTEGER	*maxlength = NULL;
	struct ip_addr		 addr;
	struct roa_ip		*res;

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RFC 6482 section 3.3: address: "
		    "failed ASN.1 sequence parse", p->fn);
		goto out;
	}

	/* ROAIPAddress has the address and optional maxlength. */

	if (sk_ASN1_TYPE_num(seq) != 1 &&
	    sk_ASN1_TYPE_num(seq) != 2) {
		warnx("%s: RFC 6482 section 3.3: adddress: "
		    "want 1 or 2 elements, have %d",
		    p->fn, sk_ASN1_TYPE_num(seq));
		goto out;
	}

	t = sk_ASN1_TYPE_value(seq, 0);
	if (t->type != V_ASN1_BIT_STRING) {
		warnx("%s: RFC 6482 section 3.3: address: "
		    "want ASN.1 bit string, have %s (NID %d)",
		    p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}
	if (!ip_addr_parse(t->value.bit_string, afi, p->fn, &addr)) {
		warnx("%s: RFC 6482 section 3.3: address: "
		    "invalid IP address", p->fn);
		goto out;
	}

	/*
	 * RFC 6482, section 3.3 doesn't ever actually state that the
	 * maximum length can't be negative, but it needs to be >=0.
	 */

	if (sk_ASN1_TYPE_num(seq) == 2) {
		t = sk_ASN1_TYPE_value(seq, 1);
		if (t->type != V_ASN1_INTEGER) {
			warnx("%s: RFC 6482 section 3.1: maxLength: "
			    "want ASN.1 integer, have %s (NID %d)",
			    p->fn, ASN1_tag2str(t->type), t->type);
			goto out;
		}
		maxlength = t->value.integer;

		/*
		 * It's safe to use ASN1_INTEGER_get() here
		 * because we're not going to have more than signed 32
		 * bit maximum of length.
		 */

		if (ASN1_INTEGER_get(maxlength) < 0) {
			warnx("%s: RFC 6482 section 3.2: maxLength: "
			    "want positive integer, have %ld",
			    p->fn, ASN1_INTEGER_get(maxlength));
			goto out;
		}
		/* FIXME: maximum check. */
	}

	p->res->ips = reallocarray(p->res->ips,
		p->res->ipsz + 1, sizeof(struct roa_ip));
	if (p->res->ips == NULL)
		err(1, NULL);
	res = &p->res->ips[p->res->ipsz++];
	memset(res, 0, sizeof(struct roa_ip));

	res->addr = addr;
	res->afi = afi;
	res->maxlength = (maxlength == NULL) ? addr.prefixlen :
	    ASN1_INTEGER_get(maxlength);
	ip_roa_compose_ranges(res);

	rc = 1;
out:
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	return rc;
}

/*
 * Parse IP address family, RFC 6482, section 3.3.
 * Returns zero on failure, non-zero on success.
 */
static int
roa_parse_ipfam(const ASN1_OCTET_STRING *os, struct parse *p)
{
	ASN1_SEQUENCE_ANY	*seq, *sseq = NULL;
	const unsigned char	*d = os->data;
	size_t			 dsz = os->length;
	int			 i, rc = 0;
	const ASN1_TYPE		*t;
	enum afi		 afi;

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RFC 6482 section 3.3: ROAIPAddressFamily: "
		    "failed ASN.1 sequence parse", p->fn);
		goto out;
	} else if (sk_ASN1_TYPE_num(seq) != 2) {
		warnx("%s: RFC 6482 section 3.3: ROAIPAddressFamily: "
		    "want 2 elements, have %d",
		    p->fn, sk_ASN1_TYPE_num(seq));
		goto out;
	}

	t = sk_ASN1_TYPE_value(seq, 0);
	if (t->type != V_ASN1_OCTET_STRING) {
		warnx("%s: RFC 6482 section 3.3: addressFamily: "
		    "want ASN.1 octet string, have %s (NID %d)",
		    p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}
	if (!ip_addr_afi_parse(p->fn, t->value.octet_string, &afi)) {
		warnx("%s: RFC 6482 section 3.3: addressFamily: "
		    "invalid", p->fn);
		goto out;
	}

	t = sk_ASN1_TYPE_value(seq, 1);
	if (t->type != V_ASN1_SEQUENCE) {
		warnx("%s: RFC 6482 section 3.3: addresses: "
		    "want ASN.1 sequence, have %s (NID %d)",
		    p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}

	d = t->value.octet_string->data;
	dsz = t->value.octet_string->length;

	if ((sseq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RFC 6482 section 3.3: addresses: "
		    "failed ASN.1 sequence parse", p->fn);
		goto out;
	}

	for (i = 0; i < sk_ASN1_TYPE_num(sseq); i++) {
		t = sk_ASN1_TYPE_value(sseq, i);
		if (t->type != V_ASN1_SEQUENCE) {
			warnx("%s: RFC 6482 section 3.3: ROAIPAddress: "
			    "want ASN.1 sequence, have %s (NID %d)",
			    p->fn, ASN1_tag2str(t->type), t->type);
			goto out;
		}
		if (!roa_parse_addr(t->value.octet_string, afi, p))
			goto out;
	}

	rc = 1;
out:
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	sk_ASN1_TYPE_pop_free(sseq, ASN1_TYPE_free);
	return rc;
}

/*
 * Parse IP blocks, RFC 6482, section 3.3.
 * Returns zero on failure, non-zero on success.
 */
static int
roa_parse_ipblocks(const ASN1_OCTET_STRING *os, struct parse *p)
{
	ASN1_SEQUENCE_ANY	*seq;
	const unsigned char	*d = os->data;
	size_t			 dsz = os->length;
	int			 i, rc = 0;
	const ASN1_TYPE		*t;

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RFC 6482 section 3.3: ipAddrBlocks: "
		    "failed ASN.1 sequence parse", p->fn);
		goto out;
	}

	for (i = 0; i < sk_ASN1_TYPE_num(seq); i++) {
		t = sk_ASN1_TYPE_value(seq, i);
		if (t->type != V_ASN1_SEQUENCE) {
			warnx("%s: RFC 6482 section 3.3: ROAIPAddressFamily: "
			    "want ASN.1 sequence, have %s (NID %d)",
			    p->fn, ASN1_tag2str(t->type), t->type);
			goto out;
		} else if (!roa_parse_ipfam(t->value.octet_string, p))
			goto out;
	}

	rc = 1;
out:
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	return rc;
}

/*
 * Parses the eContent section of an ROA file, RFC 6482, section 3.
 * Returns zero on failure, non-zero on success.
 */
static int
roa_parse_econtent(const unsigned char *d, size_t dsz, struct parse *p)
{
	ASN1_SEQUENCE_ANY	*seq;
	int			 i = 0, rc = 0, sz;
	const ASN1_TYPE		*t;

	/* RFC 6482, section 3. */

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RFC 6482 section 3: RouteOriginAttestation: "
		    "failed ASN.1 sequence parse", p->fn);
		goto out;
	}

	if ((sz = sk_ASN1_TYPE_num(seq)) != 2 && sz != 3) {
		warnx("%s: RFC 6482 section 3: RouteOriginAttestation: "
		    "want 2 or 3 elements, have %d",
		    p->fn, sk_ASN1_TYPE_num(seq));
		goto out;
	}

	/* RFC 6482, section 3.1. */

	if (sz == 3) {
		t = sk_ASN1_TYPE_value(seq, i++);

		/*
		 * This check with ASN1_INTEGER_get() is fine since
		 * we're looking for a value of zero anyway, so any
		 * overflowing number will be definition be wrong.
		 */

		if (t->type != V_ASN1_INTEGER) {
			warnx("%s: RFC 6482 section 3.1: version: "
			    "want ASN.1 integer, have %s (NID %d)",
			    p->fn, ASN1_tag2str(t->type), t->type);
			goto out;
		} else if (ASN1_INTEGER_get(t->value.integer) != 0) {
			warnx("%s: RFC 6482 section 3.1: version: "
			    "want version 0, have %ld",
			    p->fn, ASN1_INTEGER_get(t->value.integer));
			goto out;
		}
	}

	/*
	 * RFC 6482, section 3.2.
	 * It doesn't ever actually state that AS numbers can't be
	 * negative, but...?
	 */

	t = sk_ASN1_TYPE_value(seq, i++);
	if (t->type != V_ASN1_INTEGER) {
		warnx("%s: RFC 6482 section 3.2: asID: "
		    "want ASN.1 integer, have %s (NID %d)",
		    p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	} else if (!as_id_parse(t->value.integer, &p->res->asid)) {
		warnx("%s: RFC 6482 section 3.2: asID: "
		    "malformed AS identifier", p->fn);
		goto out;
	}

	/* RFC 6482, section 3.3. */

	t = sk_ASN1_TYPE_value(seq, i++);
	if (t->type != V_ASN1_SEQUENCE) {
		warnx("%s: RFC 6482 section 3.3: ipAddrBlocks: "
		    "want ASN.1 sequence, have %s (NID %d)",
		    p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	} else if (!roa_parse_ipblocks(t->value.octet_string, p))
		goto out;

	rc = 1;
out:
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	return rc;
}

/*
 * Parse a full RFC 6482 file with a SHA256 digest "dgst" and signed by
 * the certificate "cacert" (the latter two are optional and may be
 * passed as NULL to disable).
 * Returns the ROA or NULL if the document was malformed.
 */
struct roa *
roa_parse(X509 **x509, const char *fn, const unsigned char *dgst)
{
	struct parse	 p;
	size_t		 cmsz;
	unsigned char	*cms;
	int		 rc = 0;

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;

	/* OID from section 2, RFC 6482. */

	cms = cms_parse_validate(x509, fn,
	    "1.2.840.113549.1.9.16.1.24", dgst, &cmsz);
	if (cms == NULL)
		return NULL;

	if ((p.res = calloc(1, sizeof(struct roa))) == NULL)
		err(1, NULL);
	if (!x509_get_ski_aki(*x509, fn, &p.res->ski, &p.res->aki))
		goto out;
	if (!roa_parse_econtent(cms, cmsz, &p))
		goto out;

	rc = 1;
out:
	if (rc == 0) {
		roa_free(p.res);
		p.res = NULL;
		X509_free(*x509);
		*x509 = NULL;
	}
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
	free(p->aki);
	free(p->ski);
	free(p->ips);
	free(p->tal);
	free(p);
}

/*
 * Serialise parsed ROA content.
 * See roa_read() for reader.
 */
void
roa_buffer(char **b, size_t *bsz, size_t *bmax, const struct roa *p)
{
	size_t	 i;

	io_simple_buffer(b, bsz, bmax, &p->valid, sizeof(int));
	io_simple_buffer(b, bsz, bmax, &p->asid, sizeof(uint32_t));
	io_simple_buffer(b, bsz, bmax, &p->ipsz, sizeof(size_t));

	for (i = 0; i < p->ipsz; i++) {
		io_simple_buffer(b, bsz, bmax,
		    &p->ips[i].afi, sizeof(enum afi));
		io_simple_buffer(b, bsz, bmax,
		    &p->ips[i].maxlength, sizeof(size_t));
		io_simple_buffer(b, bsz, bmax,
		    p->ips[i].min, sizeof(p->ips[i].min));
		io_simple_buffer(b, bsz, bmax,
		    p->ips[i].max, sizeof(p->ips[i].max));
		ip_addr_buffer(b, bsz, bmax, &p->ips[i].addr);
	}

	io_str_buffer(b, bsz, bmax, p->aki);
	io_str_buffer(b, bsz, bmax, p->ski);
	io_str_buffer(b, bsz, bmax, p->tal);
}

/*
 * Read parsed ROA content from descriptor.
 * See roa_buffer() for writer.
 * Result must be passed to roa_free().
 */
struct roa *
roa_read(int fd)
{
	struct roa	*p;
	size_t		 i;

	if ((p = calloc(1, sizeof(struct roa))) == NULL)
		err(1, NULL);

	io_simple_read(fd, &p->valid, sizeof(int));
	io_simple_read(fd, &p->asid, sizeof(uint32_t));
	io_simple_read(fd, &p->ipsz, sizeof(size_t));

	if ((p->ips = calloc(p->ipsz, sizeof(struct roa_ip))) == NULL)
		err(1, NULL);

	for (i = 0; i < p->ipsz; i++) {
		io_simple_read(fd, &p->ips[i].afi, sizeof(enum afi));
		io_simple_read(fd, &p->ips[i].maxlength, sizeof(size_t));
		io_simple_read(fd, &p->ips[i].min, sizeof(p->ips[i].min));
		io_simple_read(fd, &p->ips[i].max, sizeof(p->ips[i].max));
		ip_addr_read(fd, &p->ips[i].addr);
	}

	io_str_read(fd, &p->aki);
	io_str_read(fd, &p->ski);
	io_str_read(fd, &p->tal);
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
	struct vrp *v;
	size_t i;

	for (i = 0; i < roa->ipsz; i++) {
		if ((v = malloc(sizeof(*v))) == NULL)
			err(1, NULL);
		v->afi = roa->ips[i].afi;
		v->addr = roa->ips[i].addr;
		v->maxlength = roa->ips[i].maxlength;
		v->asid = roa->asid;
		if ((v->tal = strdup(roa->tal)) == NULL)
			err(1, NULL);
		if (RB_INSERT(vrp_tree, tree, v) == NULL)
			(*uniqs)++;
		else /* already exists */
			free(v);
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
