/*	$OpenBSD: rsc.c,v 1.12 2022/06/10 10:41:09 tb Exp $ */
/*
 * Copyright (c) 2022 Theo Buehler <tb@openbsd.org>
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

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/safestack.h>
#include <openssl/stack.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

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
 * Types and templates for RSC eContent - draft-ietf-sidrops-rpki-rsc-08
 */

typedef struct {
	ASIdOrRanges		*asnum;
} ConstrainedASIdentifiers;

ASN1_SEQUENCE(ConstrainedASIdentifiers) = {
	ASN1_EXP_SEQUENCE_OF(ConstrainedASIdentifiers, asnum, ASIdOrRange, 0),
} ASN1_SEQUENCE_END(ConstrainedASIdentifiers);

typedef struct {
	ASN1_OCTET_STRING		*addressFamily;
	STACK_OF(IPAddressOrRange)	*addressesOrRanges;
} ConstrainedIPAddressFamily;

ASN1_SEQUENCE(ConstrainedIPAddressFamily) = {
	ASN1_SIMPLE(ConstrainedIPAddressFamily, addressFamily,
	    ASN1_OCTET_STRING),
	ASN1_SEQUENCE_OF(ConstrainedIPAddressFamily, addressesOrRanges,
	    IPAddressOrRange),
} ASN1_SEQUENCE_END(ConstrainedIPAddressFamily);

typedef STACK_OF(ConstrainedIPAddressFamily) ConstrainedIPAddrBlocks;
DECLARE_STACK_OF(ConstrainedIPAddressFamily);

ASN1_ITEM_TEMPLATE(ConstrainedIPAddrBlocks) =
	ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, ConstrainedIPAddrBlocks,
	    ConstrainedIPAddressFamily)
ASN1_ITEM_TEMPLATE_END(ConstrainedIPAddrBlocks);

typedef struct {
	ConstrainedASIdentifiers	*asID;
	ConstrainedIPAddrBlocks		*ipAddrBlocks;
} ResourceBlock;

ASN1_SEQUENCE(ResourceBlock) = {
	ASN1_EXP_OPT(ResourceBlock, asID, ConstrainedASIdentifiers, 0),
	ASN1_EXP_SEQUENCE_OF_OPT(ResourceBlock, ipAddrBlocks,
	    ConstrainedIPAddressFamily, 1)
} ASN1_SEQUENCE_END(ResourceBlock);

typedef struct {
	ASN1_IA5STRING		*fileName;
	ASN1_OCTET_STRING	*hash;
} FileNameAndHash;

DECLARE_STACK_OF(FileNameAndHash);

#ifndef DEFINE_STACK_OF
#define sk_ConstrainedIPAddressFamily_num(sk) \
    SKM_sk_num(ConstrainedIPAddressFamily, (sk))
#define sk_ConstrainedIPAddressFamily_value(sk, i) \
    SKM_sk_value(ConstrainedIPAddressFamily, (sk), (i))

#define sk_FileNameAndHash_num(sk)	SKM_sk_num(FileNameAndHash, (sk))
#define sk_FileNameAndHash_value(sk, i)	SKM_sk_value(FileNameAndHash, (sk), (i))
#endif

ASN1_SEQUENCE(FileNameAndHash) = {
	ASN1_OPT(FileNameAndHash, fileName, ASN1_IA5STRING),
	ASN1_SIMPLE(FileNameAndHash, hash, ASN1_OCTET_STRING),
} ASN1_SEQUENCE_END(FileNameAndHash);

typedef struct {
	ASN1_INTEGER			*version;
	ResourceBlock			*resources;
	X509_ALGOR			*digestAlgorithm;
	STACK_OF(FileNameAndHash)	*checkList;
} RpkiSignedChecklist;

ASN1_SEQUENCE(RpkiSignedChecklist) = {
	ASN1_IMP_OPT(RpkiSignedChecklist, version, ASN1_INTEGER, 0),
	ASN1_SIMPLE(RpkiSignedChecklist, resources, ResourceBlock),
	ASN1_SIMPLE(RpkiSignedChecklist, digestAlgorithm, X509_ALGOR),
	ASN1_SEQUENCE_OF(RpkiSignedChecklist, checkList, FileNameAndHash),
} ASN1_SEQUENCE_END(RpkiSignedChecklist);

DECLARE_ASN1_FUNCTIONS(RpkiSignedChecklist);
IMPLEMENT_ASN1_FUNCTIONS(RpkiSignedChecklist);

/*
 * Parse asID (inside ResourceBlock)
 * Return 0 on failure.
 */
static int
rsc_parse_aslist(struct parse *p, const ConstrainedASIdentifiers *asids)
{
	int	 i, asz;

	if (asids == NULL)
		return 1;

	if ((asz = sk_ASIdOrRange_num(asids->asnum)) == 0) {
		warnx("%s: RSC asID empty", p->fn);
		return 0;
	}

	if (asz >= MAX_AS_SIZE) {
		warnx("%s: too many AS number entries: limit %d",
		    p->fn, MAX_AS_SIZE);
		return 0;
	}

	p->res->as = calloc(asz, sizeof(struct cert_as));
	if (p->res->as == NULL)
		err(1, NULL);

	for (i = 0; i < asz; i++) {
		const ASIdOrRange *aor;

		aor = sk_ASIdOrRange_value(asids->asnum, i);

		switch (aor->type) {
		case ASIdOrRange_id:
			if (!sbgp_as_id(p->fn, p->res->as, &p->res->asz,
			    aor->u.id))
				return 0;
			break;
		case ASIdOrRange_range:
			if (!sbgp_as_range(p->fn, p->res->as, &p->res->asz,
			    aor->u.range))
				return 0;
			break;
		default:
			warnx("%s: RSC AsList: unknown type %d", p->fn,
			    aor->type);
			return 0;
		}
	}

	return 1;
}

static int
rsc_parse_iplist(struct parse *p, const ConstrainedIPAddrBlocks *ipAddrBlocks)
{
	const ConstrainedIPAddressFamily	*af;
	const IPAddressOrRanges			*aors;
	const IPAddressOrRange			*aor;
	size_t					 ipsz;
	enum afi				 afi;
	int					 i, j;

	if (ipAddrBlocks == NULL)
		return 1;

	if (sk_ConstrainedIPAddressFamily_num(ipAddrBlocks) == 0) {
		warnx("%s: RSC ipAddrBlocks empty", p->fn);
		return 0;
	}

	for (i = 0; i < sk_ConstrainedIPAddressFamily_num(ipAddrBlocks); i++) {
		af = sk_ConstrainedIPAddressFamily_value(ipAddrBlocks, i);
		aors = af->addressesOrRanges;

		ipsz = p->res->ipsz + sk_IPAddressOrRange_num(aors);
		if (ipsz >= MAX_IP_SIZE) {
			warnx("%s: too many IP address entries: limit %d",
			    p->fn, MAX_IP_SIZE);
			return 0;
		}

		p->res->ips = recallocarray(p->res->ips, p->res->ipsz, ipsz,
		    sizeof(struct cert_ip));
		if (p->res->ips == NULL)
			err(1, NULL);

		if (!ip_addr_afi_parse(p->fn, af->addressFamily, &afi)) {
			warnx("%s: RSC: invalid AFI", p->fn);
			return 0;
		}

		for (j = 0; j < sk_IPAddressOrRange_num(aors); j++) {
			aor = sk_IPAddressOrRange_value(aors, j);
			switch (aor->type) {
			case IPAddressOrRange_addressPrefix:
				if (!sbgp_addr(p->fn, p->res->ips,
				    &p->res->ipsz, afi, aor->u.addressPrefix))
					return 0;
				break;
			case IPAddressOrRange_addressRange:
				if (!sbgp_addr_range(p->fn, p->res->ips,
				    &p->res->ipsz, afi, aor->u.addressRange))
					return 0;
				break;
			default:
				warnx("%s: RFC 3779: IPAddressOrRange: "
				    "unknown type %d", p->fn, aor->type);
				return 0;
			}
		}
	}

	return 1;
}

static int
rsc_check_digesttype(struct parse *p, const X509_ALGOR *alg)
{
	const ASN1_OBJECT	*obj;
	int			 type, nid;

	X509_ALGOR_get0(&obj, &type, NULL, alg);

	if (type != V_ASN1_UNDEF) {
		warnx("%s: RSC DigestAlgorithmIdentifier unexpected parameters:"
		    " %d", p->fn, type);
		return 0;
	}

	if ((nid = OBJ_obj2nid(obj)) != NID_sha256) {
		warnx("%s: RSC DigestAlgorithmIdentifier: want SHA256, have %s"
		    " (NID %d)", p->fn, ASN1_tag2str(nid), nid);
		return 0;
	}

	return 1;
}

/*
 * Parse the FileNameAndHash sequence, draft-ietf-sidrops-rpki-rsc, section 4.4.
 * Return zero on failure, non-zero on success.
 */
static int
rsc_parse_checklist(struct parse *p, const STACK_OF(FileNameAndHash) *checkList)
{
	FileNameAndHash		*fh;
	ASN1_IA5STRING		*fn;
	struct rscfile		*file;
	size_t			 sz, i;

	if ((sz = sk_FileNameAndHash_num(checkList)) == 0) {
		warnx("%s: RSC checkList needs at least one entry", p->fn);
		return 0;
	}

	if (sz >= MAX_CHECKLIST_ENTRIES) {
		warnx("%s: %zu exceeds checklist entry limit (%d)", p->fn, sz,
		    MAX_CHECKLIST_ENTRIES);
		return 0;
	}

	p->res->files = calloc(sz, sizeof(struct rscfile));
	if (p->res->files == NULL)
		err(1, NULL);
	p->res->filesz = sz;

	for (i = 0; i < sz; i++) {
		fh = sk_FileNameAndHash_value(checkList, i);

		file = &p->res->files[i];

		if (fh->hash->length != SHA256_DIGEST_LENGTH) {
			warnx("%s: RSC Digest: invalid SHA256 length", p->fn);
			return 0;
		}
		memcpy(file->hash, fh->hash->data, SHA256_DIGEST_LENGTH);

		if ((fn = fh->fileName) == NULL)
			continue;

		if (!valid_filename(fn->data, fn->length)) {
			warnx("%s: RSC FileNameAndHash: bad filename", p->fn);
			return 0;
		}

		file->filename = strndup(fn->data, fn->length);
		if (file->filename == NULL)
			err(1, NULL);
	}

	return 1;
}

/*
 * Parses the eContent segment of an RSC file
 * draft-ietf-sidrops-rpki-rsc, section 4
 * Returns zero on failure, non-zero on success.
 */
static int
rsc_parse_econtent(const unsigned char *d, size_t dsz, struct parse *p)
{
	RpkiSignedChecklist	*rsc;
	ResourceBlock		*resources;
	int			 rc = 0;

	/*
	 * draft-ietf-sidrops-rpki-rsc section 4
	 */

	if ((rsc = d2i_RpkiSignedChecklist(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: RSC: failed to parse RpkiSignedChecklist",
		    p->fn);
		goto out;
	}

	if (!valid_econtent_version(p->fn, rsc->version))
		goto out;

	resources = rsc->resources;
	if (resources->asID == NULL && resources->ipAddrBlocks == NULL) {
		warnx("%s: RSC: one of asID or ipAddrBlocks must be present",
		    p->fn);
		goto out;
	}

	if (!rsc_parse_aslist(p, resources->asID))
		goto out;

	if (!rsc_parse_iplist(p, resources->ipAddrBlocks))
		goto out;

	if (!rsc_check_digesttype(p, rsc->digestAlgorithm))
		goto out;

	if (!rsc_parse_checklist(p, rsc->checkList))
		goto out;

	rc = 1;
 out:
	RpkiSignedChecklist_free(rsc);
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
	unsigned char		*cms;
	size_t			 cmsz;
	const ASN1_TIME		*at;
	int			 rc = 0;

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
