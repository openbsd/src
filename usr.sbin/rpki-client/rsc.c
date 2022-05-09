/*	$OpenBSD: rsc.c,v 1.1 2022/05/09 17:02:34 job Exp $ */
/*
 * Copyright (c) 2022 Job Snijders <job@fastly.com>
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
 * Parse results and data of the Signed Checklist file.
 */
struct	parse {
	const char	*fn; /* Signed Checklist file name */
	struct rsc	*res; /* results */
};

extern ASN1_OBJECT	*rsc_oid;

/*
 * Append an AS identifier structure to our list of results.
 * Return zero on failure.
 * XXX: merge with append_as() in cert.c
 */
static int
append_as(struct parse *p, const struct cert_as *as)
{
	if (!as_check_overlap(as, p->fn, p->res->as, p->res->asz))
		return 0;
	if (p->res->asz >= MAX_AS_SIZE)
		return 0;
	p->res->as = reallocarray(p->res->as, p->res->asz + 1,
	    sizeof(struct cert_as));
	if (p->res->as == NULL)
		err(1, NULL);
	p->res->as[p->res->asz++] = *as;
	return 1;
}

/*
 * Append an IP address structure to our list of results.
 * return zero on failure.
 * XXX: merge with append_ip() in cert.c
 */
static int
append_ip(struct parse *p, const struct cert_ip *ip)
{
	struct rsc	*res = p->res;

	if (!ip_addr_check_overlap(ip, p->fn, p->res->ips, p->res->ipsz))
		return 0;
	if (res->ipsz >= MAX_IP_SIZE)
		return 0;

	res->ips = reallocarray(res->ips, res->ipsz + 1,
	    sizeof(struct cert_ip));
	if (res->ips == NULL)
		err(1, NULL);

	res->ips[res->ipsz++] = *ip;
	return 1;
}

static int
rsc_check_digesttype(struct parse *p, const unsigned char *d, size_t dsz)
{
	X509_ALGOR		*alg;
	const ASN1_OBJECT	*obj;
	int			 type, nid;
	int			 rc = 0;

	if ((alg = d2i_X509_ALGOR(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RSC DigestAlgorithmIdentifier faild to parse",
		    p->fn);
		goto out;
	}

	X509_ALGOR_get0(&obj, &type, NULL, alg);

	if (type != V_ASN1_UNDEF) {
		warnx("%s: RSC DigestAlgorithmIdentifier unexpected parameters:"
		    " %d", p->fn, type);
		goto out;
	}

	if ((nid = OBJ_obj2nid(obj)) != NID_sha256) {
		warnx("%s: RSC DigestAlgorithmIdentifier: want SHA256, have %s"
		    " (NID %d)", p->fn, ASN1_tag2str(nid), nid);
		goto out;
	}

	rc = 1;
 out:
	X509_ALGOR_free(alg);
	return rc;
}

/*
 * Parse and individual "FileNameAndHash", draft-ietf-sidrops-rpki-rsc
 * section 4.1.
 * Return zero on failure, non-zero on success.
 */
static int
rsc_parse_filenamehash(struct parse *p, const ASN1_OCTET_STRING *os)
{
	ASN1_SEQUENCE_ANY	*seq;
	const ASN1_TYPE		*file, *hash;
	char			*fn = NULL;
	const unsigned char	*d = os->data;
	size_t			 dsz = os->length;
	int			 i = 0, rc = 0, elemsz;
	struct rscfile		*rent;

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RSC FileNameAndHash: failed ASN.1 sequence "
		    "parse", p->fn);
		goto out;
	}

	elemsz = sk_ASN1_TYPE_num(seq);
	if (elemsz != 1 && elemsz != 2) {
		warnx("%s: RSC FileNameAndHash: want 1 or 2 elements, have %d",
		    p->fn, elemsz);
		goto out;
	}

	if (elemsz == 2) {
		file = sk_ASN1_TYPE_value(seq, i++);
		if (file->type != V_ASN1_IA5STRING) {
			warnx("%s: RSC FileNameAndHash: want ASN.1 IA5 string,"
			    " have %s (NID %d)", p->fn,
			    ASN1_tag2str(file->type), file->type);
			goto out;
		}
		fn = strndup((const char *)file->value.ia5string->data,
		    file->value.ia5string->length);
		if (fn == NULL)
			err(1, NULL);

		/*
		 * filename must confirm to portable file name character set
		 * XXX: use valid_filename() instead
		 */
		if (strchr(fn, '/') != NULL) {
			warnx("%s: path components disallowed in filename: %s",
			    p->fn, fn);
			goto out;
		}
		if (strchr(fn, '\n') != NULL) {
			warnx("%s: newline disallowed in filename: %s",
			    p->fn, fn);
			goto out;
		}
	}

	/* Now hash value. */

	hash = sk_ASN1_TYPE_value(seq, i);
	if (hash->type != V_ASN1_OCTET_STRING) {
		warnx("%s: RSC FileNameAndHash: want ASN.1 OCTET string, have "
		    "%s (NID %d)", p->fn, ASN1_tag2str(hash->type), hash->type);
		goto out;
	}

	if (hash->value.octet_string->length != SHA256_DIGEST_LENGTH) {
		warnx("%s: RSC Digest: invalid SHA256 length, have %d",
		    p->fn, hash->value.octet_string->length);
		goto out;
	}

	p->res->files = recallocarray(p->res->files, p->res->filesz,
	    p->res->filesz + 1, sizeof(struct rscfile));
	if (p->res->files == NULL)
		err(1, NULL);

	rent = &p->res->files[p->res->filesz++];
	rent->filename = fn;
	fn = NULL;
	memcpy(rent->hash, hash->value.octet_string->data, SHA256_DIGEST_LENGTH);

	rc = 1;
 out:
	free(fn);
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	return rc;
}

/*
 * Parse the FileNameAndHash sequence, draft-ietf-sidrops-rpki-rsc
 * section 4.1
 * Return zero on failure, non-zero on success.
 */
static int
rsc_parse_checklist(struct parse *p, const ASN1_OCTET_STRING *os)
{
	ASN1_SEQUENCE_ANY	*seq;
	const ASN1_TYPE		*t;
	const unsigned char	*d = os->data;
	size_t			 dsz = os->length;
	int			 i, rc = 0;

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RSC checkList: failed ASN.1 sequence parse",
		    p->fn);
		goto out;
	}

	for (i = 0; i < sk_ASN1_TYPE_num(seq); i++) {
		t = sk_ASN1_TYPE_value(seq, i);
		if (t->type != V_ASN1_SEQUENCE) {
			warnx("%s: RSC checkList: want ASN.1 sequence, have %s"
			    " (NID %d)", p->fn, ASN1_tag2str(t->type), t->type);
			goto out;
		}
		if (!rsc_parse_filenamehash(p, t->value.octet_string))
			goto out;
	}

	rc = 1;
 out:
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	return rc;
}

/*
 * Convert ASN1 INTEGER and add it to parse results
 * Return zero on failure.
 * XXX: merge with sbgp_asid() in cert.c
 */
static int
rsc_parse_asid(struct parse *p, const ASN1_INTEGER *i)
{
	struct cert_as		 as;

	memset(&as, 0, sizeof(struct cert_as));
	as.type = CERT_AS_ID;

	if (!as_id_parse(i, &as.id)) {
		warnx("%s: RSC malformed AS identifier", p->fn);
		return 0;
	}
	if (as.id == 0) {
		warnx("%s: RSC AS identifier zero is reserved", p->fn);
		return 0;
	}

	return append_as(p, &as);
}

/*
 * Parse AS Range and add it to parse result
 * Return zero on failure.
 * XXX: merge with sbgp_asrange() in cert.c
 */
static int
rsc_parse_asrange(struct parse *p, const unsigned char *d, size_t dsz)
{
	struct cert_as		 as;
	ASN1_SEQUENCE_ANY	*seq;
	const ASN1_TYPE		*t;
	int			 rc = 0;

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: ASRange failed ASN.1 seq parse", p->fn);
		goto out;
	}

	if (sk_ASN1_TYPE_num(seq) != 2) {
		warnx("%s: expected 2 elements in RSC ASRange, have %d",
		    p->fn, sk_ASN1_TYPE_num(seq));
		goto out;
	}

	memset(&as, 0, sizeof(struct cert_as));
	as.type = CERT_AS_RANGE;

	t = sk_ASN1_TYPE_value(seq, 0);
	if (t->type != V_ASN1_INTEGER) {
		warnx("%s: RSC ASRange: want ASN.1 integer, have %s (NID %d)",
		    p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}
	if (!as_id_parse(t->value.integer, &as.range.min)) {
		warnx("%s: RSC malformed AS identifier", p->fn);
		goto out;
	}

	t = sk_ASN1_TYPE_value(seq, 1);
	if (t->type != V_ASN1_INTEGER) {
		warnx("%s: RSC ASRange: want ASN.1 integer, have %s (NID %d)",
		    p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}
	if (!as_id_parse(t->value.integer, &as.range.max)) {
		warnx("%s: RSC malformed AS identifier", p->fn);
		goto out;
	}

	if (as.range.max == as.range.min) {
		warnx("%s: RSC ASRange error: range is singular", p->fn);
		goto out;
	}
	if (as.range.max < as.range.min) {
		warnx("%s: RSC ASRange: range is out of order", p->fn);
		goto out;
	}

	if (!append_as(p, &as))
		goto out;

	rc = 1;
 out:
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	return rc;
}

/*
 * parse AsList (inside ResourceBlock)
 * Return 0 on failure.
 */
static int
rsc_parse_aslist(struct parse *p, const unsigned char *d, size_t dsz)
{
	ASN1_SEQUENCE_ANY	*seq;
	const ASN1_TYPE		*t;
	int			 i, rc = 0;

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RSC AsList: failed ASN.1 sequence parse",
		    p->fn);
		goto out;
	}

	for (i = 0; i < sk_ASN1_TYPE_num(seq); i++) {
		t = sk_ASN1_TYPE_value(seq, i);
		switch (t->type) {
		case V_ASN1_INTEGER:
			if (!rsc_parse_asid(p, t->value.integer))
				goto out;
			break;
		case V_ASN1_SEQUENCE:
			d = t->value.asn1_string->data;
			dsz = t->value.asn1_string->length;
			if (!rsc_parse_asrange(p, d, dsz))
				goto out;
			break;
		default:
			warnx("%s: RSC AsList expected INTEGER or SEQUENCE, "
			    "have %s (NID %d)", p->fn, ASN1_tag2str(t->type),
			    t->type);
			goto out;
		}
	}

	rc = 1;
 out:
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	return rc;
}

/*
 * parse IPAddressFamilyItem (inside IPList, inside ResourceBlock)
 * Return 0 on failure.
 */
static int
rsc_parse_ipaddrfamitem(struct parse *p, const ASN1_OCTET_STRING *os)
{
	ASN1_OCTET_STRING       *aos = NULL;
	IPAddressOrRange	*aor = NULL;
	int			 tag;
	const unsigned char     *cnt = os->data;
	long			 cntsz;
	const unsigned char     *d;
	struct cert_ip		 ip;
	int			 rc = 0;

	memset(&ip, 0, sizeof(struct cert_ip));

	/*
	 * IPAddressFamilyItem is a sequence containing an addressFamily and
	 * an IPAddressOrRange.
	 */
	if (!ASN1_frame(p->fn, os->length, &cnt, &cntsz, &tag)) {
		cryptowarnx("%s: ASN1_frame failed", p->fn);
		goto out;
	}
	if (tag != V_ASN1_SEQUENCE) {
		warnx("expected ASN.1 sequence, got %d", tag);
		goto out;
	}

	d = cnt;

	if ((aos = d2i_ASN1_OCTET_STRING(NULL, &cnt, cntsz)) == NULL) {
		cryptowarnx("%s: d2i_ASN1_OCTET_STRING failed", p->fn);
		goto out;
	}

	cntsz -= cnt - d;
	assert(cntsz >= 0);

	if (!ip_addr_afi_parse(p->fn, aos, &ip.afi)) {
		warnx("%s: RSC invalid addressFamily", p->fn);
		goto out;
	}

	d = cnt;

	if ((aor = d2i_IPAddressOrRange(NULL, &cnt, cntsz)) == NULL) {
		warnx("%s: d2i_IPAddressOrRange failed", p->fn);
		goto out;
	}

	cntsz -= cnt - d;
	assert(cntsz >= 0);

	if (cntsz > 0) {
		warnx("%s: trailing garbage in RSC IPAddressFamilyItem", p->fn);
		goto out;
	}

	switch (aor->type) {
	case IPAddressOrRange_addressPrefix:
		ip.type = CERT_IP_ADDR;
		if (!ip_addr_parse(aor->u.addressPrefix, ip.afi, p->fn, &ip.ip))
			goto out;
		break;
	case IPAddressOrRange_addressRange:
		ip.type = CERT_IP_RANGE;
		if (!ip_addr_parse(aor->u.addressRange->min, ip.afi, p->fn,
		    &ip.range.min))
			goto out;
		if (!ip_addr_parse(aor->u.addressRange->max, ip.afi, p->fn,
		    &ip.range.max))
			goto out;
		break;
	default:
		warnx("%s: unknown addressOrRange type %d\n", p->fn, aor->type);
		goto out;
	}

	if (!ip_cert_compose_ranges(&ip)) {
		warnx("%s: RSC IP address range reversed", p->fn);
		goto out;
	}

	if (!append_ip(p, &ip))
		goto out;

	rc = 1;
 out:
	ASN1_OCTET_STRING_free(aos);
	IPAddressOrRange_free(aor);
	return rc;
}

static int
rsc_parse_iplist(struct parse *p, const unsigned char *d, size_t dsz)
{
	ASN1_SEQUENCE_ANY	*seq;
	const ASN1_TYPE		*t;
	int			 i, rc = 0;

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RSC IPList: failed ASN.1 sequence parse",
		    p->fn);
		goto out;
	}

	for (i = 0; i < sk_ASN1_TYPE_num(seq); i++) {
		t = sk_ASN1_TYPE_value(seq, i);
		if (t->type != V_ASN1_SEQUENCE) {
			warnx("%s: RSC IPList: want ASN.1 sequence, have %s"
			    " (NID %d)", p->fn, ASN1_tag2str(t->type), t->type);
			goto out;
		}
		if (!rsc_parse_ipaddrfamitem(p, t->value.octet_string))
			goto out;
	}

	rc = 1;
 out:
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	return rc;
}

/*
 * Parse a ResourceBlock, draft-ietf-sidrops-rpki-rsc section 4
 * Returns zero on failure, non-zero on success.
 */
static int
rsc_parse_resourceblock(const ASN1_OCTET_STRING *os, struct parse *p)
{
	ASN1_SEQUENCE_ANY	*seq;
	const unsigned char	*d = os->data;
	size_t			 dsz = os->length;
	int			 i, ptag, rc = 0;
	const ASN1_TYPE		*t;
	long			 plen;

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RSC: ResourceBlock: failed ASN.1 sequence "
		    "parse", p->fn);
		goto out;
	}

	if (sk_ASN1_TYPE_num(seq) == 0) {
		warnx("%s: ResourceBlock, there must be at least one of asID "
		    "or ipAddrBlocks", p->fn);
		goto out;
	}

	for (i = 0; i < sk_ASN1_TYPE_num(seq); i++) {
		t = sk_ASN1_TYPE_value(seq, i);

		d = t->value.asn1_string->data;
		dsz = t->value.asn1_string->length;
		if (!ASN1_frame(p->fn, dsz, &d, &plen, &ptag))
			goto out;
		switch (ptag) {
		case RSRCBLK_TYPE_ASID:
			if (!rsc_parse_aslist(p, d, plen))
				goto out;
			break;
		case RSRCBLK_TYPE_IPADDRBLK:
			if (!rsc_parse_iplist(p, d, plen))
				goto out;
			break;
		default:
			warnx("%s: want ASN.1 context specific id, have %s"
			    " (NID %d)", p->fn, ASN1_tag2str(ptag), ptag);
				goto out;
		}
	}

	rc = 1;
 out:
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	return rc;
}

/*
 * Parses the eContent segment of a RSC file
 * draft-ietf-sidrops-rpki-rsc, section 4
 * Returns zero on failure, non-zero on success.
 */
static int
rsc_parse_econtent(const unsigned char *d, size_t dsz, struct parse *p)
{
	ASN1_SEQUENCE_ANY	*seq;
	const ASN1_TYPE		*t;
	int			 i = 0, rc = 0, sz;
	long			 rsc_version;

	/*
	 * draft-ietf-sidrops-rpki-rsc section 4
	 */

	if ((seq = d2i_ASN1_SEQUENCE_ANY(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RSC: RpkiSignedChecklist: failed ASN.1 "
		     "sequence parse", p->fn);
		goto out;
	}

	if ((sz = sk_ASN1_TYPE_num(seq)) != 3 && sz != 4) {
		warnx("%s: RSC RpkiSignedChecklist: want 3 or 4 elements, have"
		    "%d", p->fn, sk_ASN1_TYPE_num(seq));
		goto out;
	}

	/*
	 * if there are 4 elements, a version should be present: check it.
	 */
	if (sz == 4) {
		t = sk_ASN1_TYPE_value(seq, i++);
		d = t->value.asn1_string->data;
		dsz = t->value.asn1_string->length;

		if (cms_econtent_version(p->fn, &d, dsz, &rsc_version) == -1)
			goto out;

		switch (rsc_version) {
		case 0:
			warnx("%s: invalid encoding for version 0", p->fn);
			goto out;
		default:
			warnx("%s: version %ld not supported (yet)", p->fn,
			    rsc_version);
			goto out;
		}
	}

	/*
	 * The RSC's eContent ResourceBlock indicates which Internet Number
	 * Resources are associated with the signature over the checkList.
	 */
	t = sk_ASN1_TYPE_value(seq, i++);
	if (t->type != V_ASN1_SEQUENCE) {
		warnx("%s: RSC ResourceBlock: want ASN.1 sequence, have %s"
		    "(NID %d)", p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}
	if (!rsc_parse_resourceblock(t->value.octet_string, p))
		goto out;

	/* digestAlgorithm */
	t = sk_ASN1_TYPE_value(seq, i++);
	if (t->type != V_ASN1_SEQUENCE) {
		warnx("%s: RSC DigestAlgorithmIdentifier: want ASN.1 sequence,"
		    " have %s (NID %d)", p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}
	if (!rsc_check_digesttype(p, t->value.asn1_string->data,
	    t->value.asn1_string->length))
		goto out;

	/*
	 * Now a sequence of FileNameAndHash
	 */
	t = sk_ASN1_TYPE_value(seq, i++);
	if (t->type != V_ASN1_SEQUENCE) {
		warnx("%s: RSC checkList: want ASN.1 sequence, have %s "
		    "(NID %d)", p->fn, ASN1_tag2str(t->type), t->type);
		goto out;
	}
	if (!rsc_parse_checklist(p, t->value.octet_string))
		goto out;

	rc = 1;
 out:
	sk_ASN1_TYPE_pop_free(seq, ASN1_TYPE_free);
	return rc;
}

/*
 * Parse a full draft-ietf-sidrops-rpki-rsc file.
 * Returns the RSC or NULL if the object was malformed.
 */
struct rsc *
rsc_parse(X509 **x509, const char *fn, const unsigned char *der, size_t len)
{
	struct parse		 p;
	size_t			 cmsz;
	unsigned char		*cms;
	int			 rc = 0;
	const ASN1_TIME		*at;

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;

	cms = cms_parse_validate(x509, fn, der, len, rsc_oid, &cmsz);
	if (cms == NULL)
		return NULL;

	if ((p.res = calloc(1, sizeof(struct rsc))) == NULL)
		err(1, NULL);

	if (!x509_get_aia(*x509, fn, &p.res->aia))
		goto out;
	if (!x509_get_aki(*x509, fn, &p.res->aki))
		goto out;
	if (!x509_get_ski(*x509, fn, &p.res->ski))
		goto out;
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

	if (!rsc_parse_econtent(cms, cmsz, &p))
		goto out;

	rc = 1;
 out:
	if (rc == 0) {
		rsc_free(p.res);
		p.res = NULL;
		X509_free(*x509);
		*x509 = NULL;
	}
	free(cms);
	return p.res;
}

/*
 * Free an RSC pointer.
 * Safe to call with NULL.
 */
void
rsc_free(struct rsc *p)
{
	size_t	i;

	if (p == NULL)
		return;

	for (i = 0; i < p->filesz; i++)
		free(p->files[i].filename);

	free(p->aia);
	free(p->aki);
	free(p->ski);
	free(p->ips);
	free(p->as);
	free(p->files);
	free(p);
}
