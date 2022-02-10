/*	$OpenBSD: roa.c,v 1.38 2022/02/10 15:33:47 claudio Exp $ */
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

#include <openssl/asn1.h>
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
	const ASN1_INTEGER	*maxlength;
	long			 maxlen;
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
	maxlen = addr.prefixlen;

	if (sk_ASN1_TYPE_num(seq) == 2) {
		t = sk_ASN1_TYPE_value(seq, 1);
		if (t->type != V_ASN1_INTEGER) {
			warnx("%s: RFC 6482 section 3.1: maxLength: "
			    "want ASN.1 integer, have %s (NID %d)",
			    p->fn, ASN1_tag2str(t->type), t->type);
			goto out;
		}

		maxlength = t->value.integer;
		maxlen = ASN1_INTEGER_get(maxlength);
		if (maxlen < 0) {
			warnx("%s: RFC 6482 section 3.2: maxLength: "
			    "want positive integer, have %ld", p->fn, maxlen);
			goto out;
		}
		if (addr.prefixlen > maxlen) {
			warnx("%s: prefixlen (%d) larger than maxLength (%ld)",
			    p->fn, addr.prefixlen, maxlen);
			goto out;
		}
		if (maxlen > ((afi == AFI_IPV4) ? 32 : 128)) {
			warnx("%s: maxLength (%ld) too large", p->fn, maxlen);
			goto out;
		}
	}

	res = &p->res->ips[p->res->ipsz++];

	res->addr = addr;
	res->afi = afi;
	res->maxlength = maxlen;
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

	/* will be called multiple times so use recallocarray */
	if (p->res->ipsz + sk_ASN1_TYPE_num(sseq) >= MAX_IP_SIZE) {
		warnx("%s: too many IPAddress entries: limit %d",
		    p->fn, MAX_IP_SIZE);
		goto out;
	}
	p->res->ips = recallocarray(p->res->ips, p->res->ipsz,
	    p->res->ipsz + sk_ASN1_TYPE_num(sseq), sizeof(struct roa_ip));
	if (p->res->ips == NULL)
		err(1, NULL);

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
	long			 roa_version;

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

	/* Parse the optional version field */
	if (sz == 3) {
		t = sk_ASN1_TYPE_value(seq, i++);
		d = t->value.asn1_string->data;
		dsz = t->value.asn1_string->length;

		if (cms_econtent_version(p->fn, &d, dsz, &roa_version) == -1)
			goto out;

		switch (roa_version) {
		case 0:
			warnx("%s: incorrect encoding for version 0", p->fn);
			goto out;
		default:
			warnx("%s: version %ld not supported (yet)", p->fn,
			    roa_version);
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
 * Parse a full RFC 6482 file.
 * Returns the ROA or NULL if the document was malformed.
 */
struct roa *
roa_parse(X509 **x509, const char *fn, const unsigned char *der, size_t len)
{
	struct parse	 p;
	size_t		 cmsz;
	unsigned char	*cms;
	int		 rc = 0;
	const ASN1_TIME	*at;

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;

	cms = cms_parse_validate(x509, fn, der, len, roa_oid, &cmsz);
	if (cms == NULL)
		return NULL;

	if ((p.res = calloc(1, sizeof(struct roa))) == NULL)
		err(1, NULL);

	p.res->aia = x509_get_aia(*x509, fn);
	p.res->aki = x509_get_aki(*x509, 0, fn);
	p.res->ski = x509_get_ski(*x509, fn);
	if (p.res->aia == NULL || p.res->aki == NULL || p.res->ski == NULL) {
		warnx("%s: RFC 6487 section 4.8: "
		    "missing AIA, AKI or SKI X509 extension", fn);
		goto out;
	}

	at = X509_get0_notAfter(*x509);
	if (at == NULL) {
		warnx("%s: X509_get0_notAfter failed", fn);
		goto out;
	}
	if (x509_get_time(at, &p.res->expires) == -1) {
		warnx("%s: ASN1_time_parse failed", fn);
		goto out;
	}

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
	free(p->aia);
	free(p->aki);
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
