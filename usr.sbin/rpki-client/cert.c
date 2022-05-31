/*	$OpenBSD: cert.c,v 1.84 2022/05/31 18:51:35 tb Exp $ */
/*
 * Copyright (c) 2022 Theo Buehler <tb@openbsd.org>
 * Copyright (c) 2021 Job Snijders <job@openbsd.org>
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

#include <openssl/asn1.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "extern.h"

/*
 * A parsing sequence of a file (which may just be <stdin>).
 */
struct	parse {
	struct cert	*res; /* result */
	const char	*fn; /* currently-parsed file */
};

extern ASN1_OBJECT	*certpol_oid;	/* id-cp-ipAddr-asNumber cert policy */
extern ASN1_OBJECT	*carepo_oid;	/* 1.3.6.1.5.5.7.48.5 (caRepository) */
extern ASN1_OBJECT	*manifest_oid;	/* 1.3.6.1.5.5.7.48.10 (rpkiManifest) */
extern ASN1_OBJECT	*notify_oid;	/* 1.3.6.1.5.5.7.48.13 (rpkiNotify) */

/*
 * Append an IP address structure to our list of results.
 * This will also constrain us to having at most one inheritance
 * statement per AFI and also not have overlapping ranges (as prohibited
 * in section 2.2.3.6).
 * It does not make sure that ranges can't coalesce, that is, that any
 * two ranges abut each other.
 * This is warned against in section 2.2.3.6, but doesn't change the
 * semantics of the system.
 * Returns zero on failure (IP overlap) non-zero on success.
 */
static int
append_ip(const char *fn, struct cert_ip *ips, size_t *ipsz,
    const struct cert_ip *ip)
{
	if (!ip_addr_check_overlap(ip, fn, ips, *ipsz))
		return 0;
	ips[(*ipsz)++] = *ip;
	return 1;
}

/*
 * Append an AS identifier structure to our list of results.
 * Makes sure that the identifiers do not overlap or improperly inherit
 * as defined by RFC 3779 section 3.3.
 */
static int
append_as(const char *fn, struct cert_as *ases, size_t *asz,
    const struct cert_as *as)
{
	if (!as_check_overlap(as, fn, ases, *asz))
		return 0;
	ases[(*asz)++] = *as;
	return 1;
}

/*
 * Parse a range of AS identifiers as in 3.2.3.8.
 * Returns zero on failure, non-zero on success.
 */
int
sbgp_as_range(const char *fn, struct cert_as *ases, size_t *asz,
    const ASRange *range)
{
	struct cert_as		 as;

	memset(&as, 0, sizeof(struct cert_as));
	as.type = CERT_AS_RANGE;

	if (!as_id_parse(range->min, &as.range.min)) {
		warnx("%s: RFC 3779 section 3.2.3.8 (via RFC 1930): "
		    "malformed AS identifier", fn);
		return 0;
	}

	if (!as_id_parse(range->max, &as.range.max)) {
		warnx("%s: RFC 3779 section 3.2.3.8 (via RFC 1930): "
		    "malformed AS identifier", fn);
		return 0;
	}

	if (as.range.max == as.range.min) {
		warnx("%s: RFC 3379 section 3.2.3.8: ASRange: "
		    "range is singular", fn);
		return 0;
	} else if (as.range.max < as.range.min) {
		warnx("%s: RFC 3379 section 3.2.3.8: ASRange: "
		    "range is out of order", fn);
		return 0;
	}

	return append_as(fn, ases, asz, &as);
}

/*
 * Parse an entire 3.2.3.10 integer type.
 */
int
sbgp_as_id(const char *fn, struct cert_as *ases, size_t *asz,
    const ASN1_INTEGER *i)
{
	struct cert_as	 as;

	memset(&as, 0, sizeof(struct cert_as));
	as.type = CERT_AS_ID;

	if (!as_id_parse(i, &as.id)) {
		warnx("%s: RFC 3779 section 3.2.3.10 (via RFC 1930): "
		    "malformed AS identifier", fn);
		return 0;
	}
	if (as.id == 0) {
		warnx("%s: RFC 3779 section 3.2.3.10 (via RFC 1930): "
		    "AS identifier zero is reserved", fn);
		return 0;
	}

	return append_as(fn, ases, asz, &as);
}

static int
sbgp_as_inherit(const char *fn, struct cert_as *ases, size_t *asz)
{
	struct cert_as as;

	memset(&as, 0, sizeof(struct cert_as));
	as.type = CERT_AS_INHERIT;

	return append_as(fn, ases, asz, &as);
}

/*
 * Parse RFC 6487 4.8.11 X509v3 extension, with syntax documented in RFC
 * 3779 starting in section 3.2.
 * Returns zero on failure, non-zero on success.
 */
static int
sbgp_assysnum(struct parse *p, X509_EXTENSION *ext)
{
	ASIdentifiers		*asidentifiers = NULL;
	const ASIdOrRanges	*aors = NULL;
	size_t			 asz;
	int			 i, rc = 0;

	if (!X509_EXTENSION_get_critical(ext)) {
		cryptowarnx("%s: RFC 6487 section 4.8.11: autonomousSysNum: "
		    "extension not critical", p->fn);
		goto out;
	}

	if ((asidentifiers = X509V3_EXT_d2i(ext)) == NULL) {
		cryptowarnx("%s: RFC 6487 section 4.8.11: autonomousSysNum: "
		    "failed extension parse", p->fn);
		goto out;
	}

	if (asidentifiers->rdi != NULL) {
		warnx("%s: RFC 6487 section 4.8.11: autonomousSysNum: "
		    "should not have RDI values", p->fn);
		goto out;
	}

	if (asidentifiers->asnum == NULL) {
		warnx("%s: RFC 6487 section 4.8.11: autonomousSysNum: "
		    "no AS number resource set", p->fn);
		goto out;
	}

	switch (asidentifiers->asnum->type) {
	case ASIdentifierChoice_inherit:
		asz = 1;
		break;
	case ASIdentifierChoice_asIdsOrRanges:
		aors = asidentifiers->asnum->u.asIdsOrRanges;
		asz = sk_ASIdOrRange_num(aors);
		break;
	default:
		warnx("%s: RFC 3779 section 3.2.3.2: ASIdentifierChoice: "
		    "unknown type %d", p->fn, asidentifiers->asnum->type);
		goto out;
	}

	if (asz >= MAX_AS_SIZE) {
		warnx("%s: too many AS number entries: limit %d",
		    p->fn, MAX_AS_SIZE);
		goto out;
	}
	p->res->as = calloc(asz, sizeof(struct cert_as));
	if (p->res->as == NULL)
		err(1, NULL);

	if (aors == NULL) {
		if (!sbgp_as_inherit(p->fn, p->res->as, &p->res->asz))
			goto out;
	}

	for (i = 0; i < sk_ASIdOrRange_num(aors); i++) {
		const ASIdOrRange *aor;

		aor = sk_ASIdOrRange_value(aors, i);
		switch (aor->type) {
		case ASIdOrRange_id:
			if (!sbgp_as_id(p->fn, p->res->as, &p->res->asz,
			    aor->u.id))
				goto out;
			break;
		case ASIdOrRange_range:
			if (!sbgp_as_range(p->fn, p->res->as, &p->res->asz,
			    aor->u.range))
				goto out;
			break;
		default:
			warnx("%s: RFC 3779 section 3.2.3.5: ASIdOrRange: "
			    "unknown type %d", p->fn, aor->type);
			goto out;
		}
	}

	rc = 1;
 out:
	ASIdentifiers_free(asidentifiers);
	return rc;
}

/*
 * Construct a RFC 3779 2.2.3.8 range from its bit string.
 * Returns zero on failure, non-zero on success.
 */
int
sbgp_addr(const char *fn, struct cert_ip *ips, size_t *ipsz, enum afi afi,
    const ASN1_BIT_STRING *bs)
{
	struct cert_ip	ip;

	memset(&ip, 0, sizeof(struct cert_ip));

	ip.afi = afi;
	ip.type = CERT_IP_ADDR;

	if (!ip_addr_parse(bs, afi, fn, &ip.ip)) {
		warnx("%s: RFC 3779 section 2.2.3.8: IPAddress: "
		    "invalid IP address", fn);
		return 0;
	}

	if (!ip_cert_compose_ranges(&ip)) {
		warnx("%s: RFC 3779 section 2.2.3.8: IPAddress: "
		    "IP address range reversed", fn);
		return 0;
	}

	return append_ip(fn, ips, ipsz, &ip);
}

/*
 * Parse RFC 3779 2.2.3.9 range of addresses.
 * Returns zero on failure, non-zero on success.
 */
int
sbgp_addr_range(const char *fn, struct cert_ip *ips, size_t *ipsz,
    enum afi afi, const IPAddressRange *range)
{
	struct cert_ip	ip;

	memset(&ip, 0, sizeof(struct cert_ip));

	ip.afi = afi;
	ip.type = CERT_IP_RANGE;

	if (!ip_addr_parse(range->min, afi, fn, &ip.range.min)) {
		warnx("%s: RFC 3779 section 2.2.3.9: IPAddressRange: "
		    "invalid IP address", fn);
		return 0;
	}

	if (!ip_addr_parse(range->max, afi, fn, &ip.range.max)) {
		warnx("%s: RFC 3779 section 2.2.3.9: IPAddressRange: "
		    "invalid IP address", fn);
		return 0;
	}

	if (!ip_cert_compose_ranges(&ip)) {
		warnx("%s: RFC 3779 section 2.2.3.9: IPAddressRange: "
		    "IP address range reversed", fn);
		return 0;
	}

	return append_ip(fn, ips, ipsz, &ip);
}

static int
sbgp_addr_inherit(const char *fn, struct cert_ip *ips, size_t *ipsz,
    enum afi afi)
{
	struct cert_ip	ip;

	memset(&ip, 0, sizeof(struct cert_ip));

	ip.afi = afi;
	ip.type = CERT_IP_INHERIT;

	return append_ip(fn, ips, ipsz, &ip);
}

/*
 * Parse an sbgp-ipAddrBlock X509 extension, RFC 6487 4.8.10, with
 * syntax documented in RFC 3779 starting in section 2.2.
 * Returns zero on failure, non-zero on success.
 */
static int
sbgp_ipaddrblk(struct parse *p, X509_EXTENSION *ext)
{
	STACK_OF(IPAddressFamily)	*addrblk = NULL;
	const IPAddressFamily		*af;
	const IPAddressOrRanges		*aors;
	const IPAddressOrRange		*aor;
	enum afi			 afi;
	size_t				 ipsz;
	int				 i, j, rc = 0;

	if (!X509_EXTENSION_get_critical(ext)) {
		cryptowarnx("%s: RFC 6487 section 4.8.10: sbgp-ipAddrBlock: "
		    "extension not critical", p->fn);
		goto out;
	}

	if ((addrblk = X509V3_EXT_d2i(ext)) == NULL) {
		cryptowarnx("%s: RFC 6487 section 4.8.10: sbgp-ipAddrBlock: "
		    "failed extension parse", p->fn);
		goto out;
	}

	for (i = 0; i < sk_IPAddressFamily_num(addrblk); i++) {
		af = sk_IPAddressFamily_value(addrblk, i);

		switch (af->ipAddressChoice->type) {
		case IPAddressChoice_inherit:
			aors = NULL;
			ipsz = p->res->ipsz + 1;
			break;
		case IPAddressChoice_addressesOrRanges:
			aors = af->ipAddressChoice->u.addressesOrRanges;
			ipsz = p->res->ipsz + sk_IPAddressOrRange_num(aors);
			break;
		default:
			warnx("%s: RFC 3779: IPAddressChoice: unknown type %d",
			    p->fn, af->ipAddressChoice->type);
			goto out;
		}

		if (ipsz >= MAX_IP_SIZE)
			goto out;
		p->res->ips = recallocarray(p->res->ips, p->res->ipsz, ipsz,
		    sizeof(struct cert_ip));
		if (p->res->ips == NULL)
			err(1, NULL);

		if (!ip_addr_afi_parse(p->fn, af->addressFamily, &afi)) {
			warnx("%s: RFC 3779: invalid AFI", p->fn);
			goto out;
		}

		if (aors == NULL) {
			if (!sbgp_addr_inherit(p->fn, p->res->ips,
			    &p->res->ipsz, afi))
				goto out;
			continue;
		}

		for (j = 0; j < sk_IPAddressOrRange_num(aors); j++) {
			aor = sk_IPAddressOrRange_value(aors, j);
			switch (aor->type) {
			case IPAddressOrRange_addressPrefix:
				if (!sbgp_addr(p->fn, p->res->ips,
				    &p->res->ipsz, afi, aor->u.addressPrefix))
					goto out;
				break;
			case IPAddressOrRange_addressRange:
				if (!sbgp_addr_range(p->fn, p->res->ips,
				    &p->res->ipsz, afi, aor->u.addressRange))
					goto out;
				break;
			default:
				warnx("%s: RFC 3779: IPAddressOrRange: "
				    "unknown type %d", p->fn, aor->type);
				goto out;
			}
		}
	}

	rc = 1;
 out:
	sk_IPAddressFamily_pop_free(addrblk, IPAddressFamily_free);
	return rc;
}

/*
 * Parse "Subject Information Access" extension, RFC 6487 4.8.8.
 * Returns zero on failure, non-zero on success.
 */
static int
sbgp_sia(struct parse *p, X509_EXTENSION *ext)
{
	AUTHORITY_INFO_ACCESS	*sia = NULL;
	ACCESS_DESCRIPTION	*ad;
	ASN1_OBJECT		*oid;
	int			 i, rc = 0;

	if (X509_EXTENSION_get_critical(ext)) {
		warnx("%s: RFC 6487 section 4.8.8: SIA: "
		    "extension not non-critical", p->fn);
		goto out;
	}

	if ((sia = X509V3_EXT_d2i(ext)) == NULL) {
		cryptowarnx("%s: RFC 6487 section 4.8.8: SIA: "
		    "failed extension parse", p->fn);
		goto out;
	}

	for (i = 0; i < sk_ACCESS_DESCRIPTION_num(sia); i++) {
		ad = sk_ACCESS_DESCRIPTION_value(sia, i);

		oid = ad->method;

		if (OBJ_cmp(oid, carepo_oid) == 0) {
			if (!x509_location(p->fn, "SIA: caRepository",
			    "rsync://", ad->location, &p->res->repo))
				goto out;
		} else if (OBJ_cmp(oid, manifest_oid) == 0) {
			if (!x509_location(p->fn, "SIA: rpkiManifest",
			    "rsync://", ad->location, &p->res->mft))
				goto out;
		} else if (OBJ_cmp(oid, notify_oid) == 0) {
			if (!x509_location(p->fn, "SIA: rpkiNotify",
			    "https://", ad->location, &p->res->notify))
				goto out;
		}
	}

	if (p->res->mft == NULL || p->res->repo == NULL) {
		warnx("%s: RFC 6487 section 4.8.8: SIA: missing caRepository "
		    "or rpkiManifest", p->fn);
		goto out;
	}

	if (strstr(p->res->mft, p->res->repo) != p->res->mft) {
		warnx("%s: RFC 6487 section 4.8.8: SIA: "
		    "conflicting URIs for caRepository and rpkiManifest",
		    p->fn);
		goto out;
	}

	if (rtype_from_file_extension(p->res->mft) != RTYPE_MFT) {
		warnx("%s: RFC 6487 section 4.8.8: SIA: "
		    "not an MFT file", p->fn);
		goto out;
	}

	rc = 1;
 out:
	AUTHORITY_INFO_ACCESS_free(sia);
	return rc;
}

/*
 * Parse the certificate policies extension and check that it follows RFC 7318.
 * Returns zero on failure, non-zero on success.
 */
static int
certificate_policies(struct parse *p, X509_EXTENSION *ext)
{
	STACK_OF(POLICYINFO)		*policies = NULL;
	POLICYINFO			*policy;
	STACK_OF(POLICYQUALINFO)	*qualifiers;
	POLICYQUALINFO			*qualifier;
	int				 nid;
	int				 rc = 0;

	if (!X509_EXTENSION_get_critical(ext)) {
		cryptowarnx("%s: RFC 6487 section 4.8.9: certificatePolicies: "
		    "extension not critical", p->fn);
		goto out;
	}

	if ((policies = X509V3_EXT_d2i(ext)) == NULL) {
		cryptowarnx("%s: RFC 6487 section 4.8.9: certificatePolicies: "
		    "failed extension parse", p->fn);
		goto out;
	}

	if (sk_POLICYINFO_num(policies) != 1) {
		warnx("%s: RFC 6487 section 4.8.9: certificatePolicies: "
		    "want 1 policy, got %d", p->fn,
		    sk_POLICYINFO_num(policies));
		goto out;
	}

	policy = sk_POLICYINFO_value(policies, 0);
	assert(policy != NULL && policy->policyid != NULL);

	if (OBJ_cmp(policy->policyid, certpol_oid) != 0) {
		char pbuf[128], cbuf[128];

		OBJ_obj2txt(pbuf, sizeof(pbuf), policy->policyid, 1);
		OBJ_obj2txt(cbuf, sizeof(cbuf), certpol_oid, 1);
		warnx("%s: RFC 7318 section 2: certificatePolicies: "
		    "unexpected OID: %s, want %s", p->fn, pbuf, cbuf);
		goto out;
	}

	/* Policy qualifiers are optional. If they're absent, we're done. */
	if ((qualifiers = policy->qualifiers) == NULL) {
		rc = 1;
		goto out;
	}

	if (sk_POLICYQUALINFO_num(qualifiers) != 1) {
		warnx("%s: RFC 7318 section 2: certificatePolicies: "
		    "want 1 policy qualifier, got %d", p->fn,
		    sk_POLICYQUALINFO_num(qualifiers));
		goto out;
	}

	qualifier = sk_POLICYQUALINFO_value(qualifiers, 0);
	assert(qualifier != NULL && qualifier->pqualid != NULL);

	if ((nid = OBJ_obj2nid(qualifier->pqualid)) != NID_id_qt_cps) {
		warnx("%s: RFC 7318 section 2: certificatePolicies: "
		    "want CPS, got %d (%s)", p->fn, nid, OBJ_nid2sn(nid));
		goto out;
	}

	if (verbose > 1)
		warnx("%s: CPS %.*s", p->fn, qualifier->d.cpsuri->length,
		    qualifier->d.cpsuri->data);

	rc = 1;
 out:
	sk_POLICYINFO_pop_free(policies, POLICYINFO_free);
	return rc;
}

/*
 * Parse and partially validate an RPKI X509 certificate (either a trust
 * anchor or a certificate) as defined in RFC 6487.
 * Returns the parse results or NULL on failure.
 */
struct cert *
cert_parse_pre(const char *fn, const unsigned char *der, size_t len)
{
	int		 extsz;
	int		 sia_present = 0;
	size_t		 i;
	X509		*x = NULL;
	X509_EXTENSION	*ext = NULL;
	ASN1_OBJECT	*obj;
	struct parse	 p;

	/* just fail for empty buffers, the warning was printed elsewhere */
	if (der == NULL)
		return NULL;

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;
	if ((p.res = calloc(1, sizeof(struct cert))) == NULL)
		err(1, NULL);

	if ((x = d2i_X509(NULL, &der, len)) == NULL) {
		cryptowarnx("%s: d2i_X509", p.fn);
		goto out;
	}

	/* Cache X509v3 extensions, see X509_check_ca(3). */
	if (X509_check_purpose(x, -1, -1) <= 0) {
		cryptowarnx("%s: could not cache X509v3 extensions", p.fn);
		goto out;
	}

	/* Look for X509v3 extensions. */

	if ((extsz = X509_get_ext_count(x)) < 0)
		cryptoerrx("X509_get_ext_count");

	for (i = 0; i < (size_t)extsz; i++) {
		ext = X509_get_ext(x, i);
		assert(ext != NULL);
		obj = X509_EXTENSION_get_object(ext);
		assert(obj != NULL);

		switch (OBJ_obj2nid(obj)) {
		case NID_sbgp_ipAddrBlock:
			if (!sbgp_ipaddrblk(&p, ext))
				goto out;
			break;
		case NID_sbgp_autonomousSysNum:
			if (!sbgp_assysnum(&p, ext))
				goto out;
			break;
		case NID_sinfo_access:
			sia_present = 1;
			if (!sbgp_sia(&p, ext))
				goto out;
			break;
		case NID_certificate_policies:
			if (!certificate_policies(&p, ext))
				goto out;
			break;
		case NID_crl_distribution_points:
			/* ignored here, handled later */
			break;
		case NID_info_access:
			break;
		case NID_authority_key_identifier:
			break;
		case NID_subject_key_identifier:
			break;
		case NID_ext_key_usage:
			break;
		default:
			/* {
				char objn[64];
				OBJ_obj2txt(objn, sizeof(objn), obj, 0);
				warnx("%s: ignoring %s (NID %d)",
					p.fn, objn, OBJ_obj2nid(obj));
			} */
			break;
		}
	}

	if (!x509_get_aki(x, p.fn, &p.res->aki))
		goto out;
	if (!x509_get_ski(x, p.fn, &p.res->ski))
		goto out;
	if (!x509_get_aia(x, p.fn, &p.res->aia))
		goto out;
	if (!x509_get_crl(x, p.fn, &p.res->crl))
		goto out;
	if (!x509_get_expire(x, p.fn, &p.res->expires))
		goto out;
	p.res->purpose = x509_get_purpose(x, p.fn);

	/* Validation on required fields. */

	switch (p.res->purpose) {
	case CERT_PURPOSE_CA:
		if (p.res->mft == NULL) {
			warnx("%s: RFC 6487 section 4.8.8: missing SIA", p.fn);
			goto out;
		}
		if (p.res->asz == 0 && p.res->ipsz == 0) {
			warnx("%s: missing IP or AS resources", p.fn);
			goto out;
		}
		break;
	case CERT_PURPOSE_BGPSEC_ROUTER:
		p.res->pubkey = x509_get_pubkey(x, p.fn);
		if (p.res->pubkey == NULL) {
			warnx("%s: x509_get_pubkey failed", p.fn);
			goto out;
		}
		if (p.res->ipsz > 0) {
			warnx("%s: unexpected IP resources in BGPsec cert",
			    p.fn);
			goto out;
		}
		if (sia_present) {
			warnx("%s: unexpected SIA extension in BGPsec cert",
			    p.fn);
			goto out;
		}
		break;
	default:
		warnx("%s: x509_get_purpose failed in %s", p.fn, __func__);
		goto out;
	}

	if (p.res->ski == NULL) {
		warnx("%s: RFC 6487 section 8.4.2: missing SKI", p.fn);
		goto out;
	}

	p.res->x509 = x;
	return p.res;

out:
	cert_free(p.res);
	X509_free(x);
	return NULL;
}

struct cert *
cert_parse(const char *fn, struct cert *p)
{
	if (p == NULL)
		return NULL;

	if (p->aki == NULL) {
		warnx("%s: RFC 6487 section 8.4.2: "
		    "non-trust anchor missing AKI", fn);
		goto badcert;
	}
	if (strcmp(p->aki, p->ski) == 0) {
		warnx("%s: RFC 6487 section 8.4.2: "
		    "non-trust anchor AKI may not match SKI", fn);
		goto badcert;
	}
	if (p->aia == NULL) {
		warnx("%s: RFC 6487 section 8.4.7: AIA: extension missing", fn);
		goto badcert;
	}
	if (p->crl == NULL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "no CRL distribution point extension", fn);
		goto badcert;
	}
	return p;

badcert:
	cert_free(p);
	return NULL;
}

struct cert *
ta_parse(const char *fn, struct cert *p, const unsigned char *pkey,
    size_t pkeysz)
{
	ASN1_TIME	*notBefore, *notAfter;
	EVP_PKEY	*pk, *opk;

	if (p == NULL)
		return NULL;

	/* first check pubkey against the one from the TAL */
	pk = d2i_PUBKEY(NULL, &pkey, pkeysz);
	if (pk == NULL) {
		cryptowarnx("%s: RFC 6487 (trust anchor): bad TAL pubkey", fn);
		goto badcert;
	}
	if ((opk = X509_get0_pubkey(p->x509)) == NULL) {
		cryptowarnx("%s: RFC 6487 (trust anchor): missing pubkey", fn);
		goto badcert;
	}
	if (EVP_PKEY_cmp(pk, opk) != 1) {
		cryptowarnx("%s: RFC 6487 (trust anchor): "
		    "pubkey does not match TAL pubkey", fn);
		goto badcert;
	}

	if ((notBefore = X509_get_notBefore(p->x509)) == NULL) {
		warnx("%s: certificate has invalid notBefore", fn);
		goto badcert;
	}
	if ((notAfter = X509_get_notAfter(p->x509)) == NULL) {
		warnx("%s: certificate has invalid notAfter", fn);
		goto badcert;
	}
	if (X509_cmp_current_time(notBefore) != -1) {
		warnx("%s: certificate not yet valid", fn);
		goto badcert;
	}
	if (X509_cmp_current_time(notAfter) != 1) {
		warnx("%s: certificate has expired", fn);
		goto badcert;
	}
	if (p->aki != NULL && strcmp(p->aki, p->ski)) {
		warnx("%s: RFC 6487 section 8.4.2: "
		    "trust anchor AKI, if specified, must match SKI", fn);
		goto badcert;
	}
	if (p->aia != NULL) {
		warnx("%s: RFC 6487 section 8.4.7: "
		    "trust anchor must not have AIA", fn);
		goto badcert;
	}
	if (p->crl != NULL) {
		warnx("%s: RFC 6487 section 8.4.2: "
		    "trust anchor may not specify CRL resource", fn);
		goto badcert;
	}
	if (p->purpose == CERT_PURPOSE_BGPSEC_ROUTER) {
		warnx("%s: BGPsec cert cannot be a trust anchor", fn);
		goto badcert;
	}

	EVP_PKEY_free(pk);
	return p;

badcert:
	EVP_PKEY_free(pk);
	cert_free(p);
	return NULL;
}

/*
 * Free parsed certificate contents.
 * Passing NULL is a noop.
 */
void
cert_free(struct cert *p)
{
	if (p == NULL)
		return;

	free(p->crl);
	free(p->repo);
	free(p->mft);
	free(p->notify);
	free(p->ips);
	free(p->as);
	free(p->aia);
	free(p->aki);
	free(p->ski);
	free(p->pubkey);
	X509_free(p->x509);
	free(p);
}

/*
 * Write certificate parsed content into buffer.
 * See cert_read() for the other side of the pipe.
 */
void
cert_buffer(struct ibuf *b, const struct cert *p)
{
	io_simple_buffer(b, &p->expires, sizeof(p->expires));
	io_simple_buffer(b, &p->purpose, sizeof(p->purpose));
	io_simple_buffer(b, &p->talid, sizeof(p->talid));
	io_simple_buffer(b, &p->ipsz, sizeof(p->ipsz));
	io_simple_buffer(b, &p->asz, sizeof(p->asz));

	io_simple_buffer(b, p->ips, p->ipsz * sizeof(p->ips[0]));
	io_simple_buffer(b, p->as, p->asz * sizeof(p->as[0]));

	io_str_buffer(b, p->mft);
	io_str_buffer(b, p->notify);
	io_str_buffer(b, p->repo);
	io_str_buffer(b, p->crl);
	io_str_buffer(b, p->aia);
	io_str_buffer(b, p->aki);
	io_str_buffer(b, p->ski);
	io_str_buffer(b, p->pubkey);
}

/*
 * Allocate and read parsed certificate content from descriptor.
 * The pointer must be freed with cert_free().
 * Always returns a valid pointer.
 */
struct cert *
cert_read(struct ibuf *b)
{
	struct cert	*p;

	if ((p = calloc(1, sizeof(struct cert))) == NULL)
		err(1, NULL);

	io_read_buf(b, &p->expires, sizeof(p->expires));
	io_read_buf(b, &p->purpose, sizeof(p->purpose));
	io_read_buf(b, &p->talid, sizeof(p->talid));
	io_read_buf(b, &p->ipsz, sizeof(p->ipsz));
	io_read_buf(b, &p->asz, sizeof(p->asz));

	p->ips = calloc(p->ipsz, sizeof(struct cert_ip));
	if (p->ips == NULL)
		err(1, NULL);
	io_read_buf(b, p->ips, p->ipsz * sizeof(p->ips[0]));

	p->as = calloc(p->asz, sizeof(struct cert_as));
	if (p->as == NULL)
		err(1, NULL);
	io_read_buf(b, p->as, p->asz * sizeof(p->as[0]));

	io_read_str(b, &p->mft);
	io_read_str(b, &p->notify);
	io_read_str(b, &p->repo);
	io_read_str(b, &p->crl);
	io_read_str(b, &p->aia);
	io_read_str(b, &p->aki);
	io_read_str(b, &p->ski);
	io_read_str(b, &p->pubkey);

	assert(p->mft != NULL || p->purpose == CERT_PURPOSE_BGPSEC_ROUTER);
	assert(p->ski);
	return p;
}

static inline int
authcmp(struct auth *a, struct auth *b)
{
	return strcmp(a->cert->ski, b->cert->ski);
}

RB_GENERATE_STATIC(auth_tree, auth, entry, authcmp);

struct auth *
auth_find(struct auth_tree *auths, const char *aki)
{
	struct auth a;
	struct cert c;

	/* we look up the cert where the ski == aki */
	c.ski = (char *)aki;
	a.cert = &c;

	return RB_FIND(auth_tree, auths, &a);
}

struct auth *
auth_insert(struct auth_tree *auths, struct cert *cert, struct auth *parent)
{
	struct auth *na;

	na = malloc(sizeof(*na));
	if (na == NULL)
		err(1, NULL);

	na->parent = parent;
	na->cert = cert;

	if (RB_INSERT(auth_tree, auths, na) != NULL)
		err(1, "auth tree corrupted");

	return na;
}

static void
insert_brk(struct brk_tree *tree, struct cert *cert, int asid)
{
	struct brk	*b, *found;

	if ((b = calloc(1, sizeof(*b))) == NULL)
		err(1, NULL);

	b->asid = asid;
	b->expires = cert->expires;
	b->talid = cert->talid;
	if ((b->ski = strdup(cert->ski)) == NULL)
		err(1, NULL);
	if ((b->pubkey = strdup(cert->pubkey)) == NULL)
		err(1, NULL);

	/*
	 * Check if a similar BRK already exists in the tree. If the found BRK
	 * expires sooner, update it to this BRK's later expiry moment.
	 */
	if ((found = RB_INSERT(brk_tree, tree, b)) != NULL) {
		if (found->expires < b->expires) {
			found->expires = b->expires;
			found->talid = b->talid;
		}
		free(b->ski);
		free(b->pubkey);
		free(b);
	}
}

/*
 * Add each BGPsec Router Key into the BRK tree.
 */
void
cert_insert_brks(struct brk_tree *tree, struct cert *cert)
{
	size_t		 i, asid;

	for (i = 0; i < cert->asz; i++) {
		switch (cert->as[i].type) {
		case CERT_AS_ID:
			insert_brk(tree, cert, cert->as[i].id);
			break;
		case CERT_AS_RANGE:
			for (asid = cert->as[i].range.min;
			    asid <= cert->as[i].range.max; asid++)
				insert_brk(tree, cert, asid);
			break;
		default:
			warnx("invalid AS identifier type");
			continue;
		}
	}
}

static inline int
brkcmp(struct brk *a, struct brk *b)
{
	int rv;

	if (a->asid > b->asid)
		return 1;
	if (a->asid < b->asid)
		return -1;

	rv = strcmp(a->ski, b->ski);
	if (rv > 0)
		return 1;
	if (rv < 0)
		return -1;

	return strcmp(a->pubkey, b->pubkey);
}

RB_GENERATE(brk_tree, brk, entry, brkcmp);
