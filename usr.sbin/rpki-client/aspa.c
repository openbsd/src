/*	$OpenBSD: aspa.c,v 1.9 2022/11/29 20:41:32 job Exp $ */
/*
 * Copyright (c) 2022 Job Snijders <job@fastly.com>
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
 * Parse results and data of the ASPA object.
 */
struct	parse {
	const char	 *fn; /* ASPA file name */
	struct aspa	 *res; /* results */
};

extern ASN1_OBJECT	*aspa_oid;

/*
 * Types and templates for ASPA eContent draft-ietf-sidrops-aspa-profile-08
 */

typedef struct {
	ASN1_INTEGER		*providerASID;
	ASN1_OCTET_STRING	*afiLimit;
} ProviderAS;

DECLARE_STACK_OF(ProviderAS);

#ifndef DEFINE_STACK_OF
#define sk_ProviderAS_num(sk)		SKM_sk_num(ProviderAS, (sk))
#define sk_ProviderAS_value(sk, i)	SKM_sk_value(ProviderAS, (sk), (i))
#endif

ASN1_SEQUENCE(ProviderAS) = {
	ASN1_SIMPLE(ProviderAS, providerASID, ASN1_INTEGER),
	ASN1_OPT(ProviderAS, afiLimit, ASN1_OCTET_STRING),
} ASN1_SEQUENCE_END(ProviderAS);

typedef struct {
	ASN1_INTEGER		*version;
	ASN1_INTEGER		*customerASID;
	STACK_OF(ProviderAS)	*providers;
} ASProviderAttestation;

ASN1_SEQUENCE(ASProviderAttestation) = {
	ASN1_EXP_OPT(ASProviderAttestation, version, ASN1_INTEGER, 0),
	ASN1_SIMPLE(ASProviderAttestation, customerASID, ASN1_INTEGER),
	ASN1_SEQUENCE_OF(ASProviderAttestation, providers, ProviderAS),
} ASN1_SEQUENCE_END(ASProviderAttestation);

DECLARE_ASN1_FUNCTIONS(ASProviderAttestation);
IMPLEMENT_ASN1_FUNCTIONS(ASProviderAttestation);

/*
 * Parse the ProviderASSet sequence.
 * Return zero on failure, non-zero on success.
 */
static int
aspa_parse_providers(struct parse *p, const STACK_OF(ProviderAS) *providers)
{
	ProviderAS		*pa;
	struct aspa_provider	 provider;
	size_t			 providersz, i;

	if ((providersz = sk_ProviderAS_num(providers)) == 0) {
		warnx("%s: ASPA: ProviderASSet needs at least one entry",
		    p->fn);
		return 0;
	}

	if (providersz >= MAX_ASPA_PROVIDERS) {
		warnx("%s: ASPA: too many providers (more than %d)", p->fn,
		    MAX_ASPA_PROVIDERS);
		return 0;
	}

	p->res->providers = calloc(providersz, sizeof(provider));
	if (p->res->providers == NULL)
		err(1, NULL);

	for (i = 0; i < providersz; i++) {
		pa = sk_ProviderAS_value(providers, i);

		memset(&provider, 0, sizeof(provider));

		if (!as_id_parse(pa->providerASID, &provider.as)) {
			warnx("%s: ASPA: malformed ProviderAS", p->fn);
			return 0;
		}

		if (p->res->custasid == provider.as) {
			warnx("%s: ASPA: CustomerASID can't also be Provider",
			    p->fn);
			return 0;
		}

		if (i > 0) {
			if  (p->res->providers[i - 1].as > provider.as) {
				warnx("%s: ASPA: invalid ProviderASSet order",
				    p->fn);
				return 0;
			}
			if (p->res->providers[i - 1].as == provider.as) {
				warnx("%s: ASPA: duplicate ProviderAS", p->fn);
				return 0;
			}
		}

		if (pa->afiLimit != NULL && !ip_addr_afi_parse(p->fn,
		    pa->afiLimit, &provider.afi)) {
			warnx("%s: ASPA: invalid afiLimit", p->fn);
			return 0;
		}

		p->res->providers[p->res->providersz++] = provider;
	}

	return 1;
}

/*
 * Parse the eContent of an ASPA file.
 * Returns zero on failure, non-zero on success.
 */
static int
aspa_parse_econtent(const unsigned char *d, size_t dsz, struct parse *p)
{
	ASProviderAttestation	*aspa;
	int			 rc = 0;

	if ((aspa = d2i_ASProviderAttestation(NULL, &d, dsz)) == NULL) {
		cryptowarnx("%s: ASPA: failed to parse ASProviderAttestation",
		    p->fn);
		goto out;
	}

	if (!valid_econtent_version(p->fn, aspa->version))
		goto out;

	if (!as_id_parse(aspa->customerASID, &p->res->custasid)) {
		warnx("%s: malformed CustomerASID", p->fn);
		goto out;
	}

	if (!aspa_parse_providers(p, aspa->providers))
		goto out;

	rc = 1;
 out:
	ASProviderAttestation_free(aspa);
	return rc;
}

/*
 * Parse a full ASPA file.
 * Returns the payload or NULL if the file was malformed.
 */
struct aspa *
aspa_parse(X509 **x509, const char *fn, const unsigned char *der, size_t len)
{
	struct parse	 p;
	size_t		 cmsz;
	unsigned char	*cms;
	const ASN1_TIME	*at;
	struct cert	*cert = NULL;
	int		 rc = 0;

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;

	cms = cms_parse_validate(x509, fn, der, len, aspa_oid, &cmsz);
	if (cms == NULL)
		return NULL;

	if ((p.res = calloc(1, sizeof(*p.res))) == NULL)
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

	if (X509_get_ext_by_NID(*x509, NID_sbgp_ipAddrBlock, -1) != -1) {
		warnx("%s: superfluous IP Resources extension present", fn);
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

	if (x509_any_inherits(*x509)) {
		warnx("%s: inherit elements not allowed in EE cert", fn);
		goto out;
	}

	if (!aspa_parse_econtent(cms, cmsz, &p))
		goto out;

	if ((cert = cert_parse_ee_cert(fn, *x509)) == NULL)
		goto out;

	p.res->valid = valid_aspa(fn, cert, p.res);

	rc = 1;
 out:
	if (rc == 0) {
		aspa_free(p.res);
		p.res = NULL;
		X509_free(*x509);
		*x509 = NULL;
	}
	cert_free(cert);
	free(cms);
	return p.res;
}

/*
 * Free an ASPA pointer.
 * Safe to call with NULL.
 */
void
aspa_free(struct aspa *p)
{
	if (p == NULL)
		return;

	free(p->aia);
	free(p->aki);
	free(p->sia);
	free(p->ski);
	free(p->providers);
	free(p);
}

/*
 * Serialise parsed ASPA content.
 * See aspa_read() for the reader on the other side.
 */
void
aspa_buffer(struct ibuf *b, const struct aspa *p)
{
	io_simple_buffer(b, &p->valid, sizeof(p->valid));
	io_simple_buffer(b, &p->custasid, sizeof(p->custasid));
	io_simple_buffer(b, &p->expires, sizeof(p->expires));

	io_simple_buffer(b, &p->providersz, sizeof(size_t));
	io_simple_buffer(b, p->providers,
	    p->providersz * sizeof(p->providers[0]));

	io_str_buffer(b, p->aia);
	io_str_buffer(b, p->aki);
	io_str_buffer(b, p->ski);
}

/*
 * Read parsed ASPA content from descriptor.
 * See aspa_buffer() for writer.
 * Result must be passed to aspa_free().
 */
struct aspa *
aspa_read(struct ibuf *b)
{
	struct aspa	*p;

	if ((p = calloc(1, sizeof(struct aspa))) == NULL)
		err(1, NULL);

	io_read_buf(b, &p->valid, sizeof(p->valid));
	io_read_buf(b, &p->custasid, sizeof(p->custasid));
	io_read_buf(b, &p->expires, sizeof(p->expires));

	io_read_buf(b, &p->providersz, sizeof(size_t));
	if ((p->providers = calloc(p->providersz,
	    sizeof(struct aspa_provider))) == NULL)
		err(1, NULL);
	io_read_buf(b, p->providers, p->providersz * sizeof(p->providers[0]));

	io_read_str(b, &p->aia);
	io_read_str(b, &p->aki);
	io_read_str(b, &p->ski);
	assert(p->aia && p->aki && p->ski);

	return p;
}

/*
 * draft-ietf-sidrops-8210bis section 5.12 states:
 *
 *     "The router MUST see at most one ASPA for a given AFI from a cache for
 *      a particular Customer ASID active at any time. As a number of conditions
 *      in the global RPKI may present multiple valid ASPA RPKI records for a
 *      single customer to a particular RP cache, this places a burden on the
 *      cache to form the union of multiple ASPA records it has received from
 *      the global RPKI into one RPKI-To-Router (RTR) ASPA PDU."
 *
 * The above described 'burden' (which is specific to RTR) is resolved in
 * insert_vap() and aspa_insert_vaps() functions below.
 *
 * XXX: for bgpd(8), ASPA config injection (via /var/db/rpki-client/openbgpd)
 * we probably want to undo the 'burden solving' and compress into implicit
 * AFIs.
 */

/*
 * If the CustomerASID (CAS) showed up before, append the ProviderAS (PAS);
 * otherwise create a new entry in the RB tree.
 * Ensure there are no duplicates in the 'providers' array.
 * Always compare 'expires': use the soonest expiration moment.
 */
static void
insert_vap(struct vap_tree *tree, uint32_t cas, uint32_t pas, time_t expires,
    enum afi afi)
{
	struct vap	*v, *found;
	size_t		 i;

	if ((v = malloc(sizeof(*v))) == NULL)
		err(1, NULL);
	v->afi = afi;
	v->custasid = cas;
	v->expires = expires;

	if ((found = RB_INSERT(vap_tree, tree, v)) == NULL) {
		if ((v->providers = malloc(sizeof(uint32_t))) == NULL)
			err(1, NULL);

		v->providers[0] = pas;
		v->providersz = 1;

		return;
	}

	free(v);

	if (found->expires > expires)
		found->expires = expires;

	for (i = 0; i < found->providersz; i++) {
		if (found->providers[i] == pas)
			return;
	}

	found->providers = reallocarray(found->providers,
	    found->providersz + 1, sizeof(uint32_t));
	if (found->providers == NULL)
		err(1, NULL);
	found->providers[found->providersz++] = pas;
}

/*
 * Add each ProviderAS entry into the Validated ASPA Providers (VAP) tree.
 * Updates "vaps" to be the total number of VAPs, and "uniqs" to be the
 * pre-'AFI explosion' deduplicated count.
 */
void
aspa_insert_vaps(struct vap_tree *tree, struct aspa *aspa, size_t *vaps,
    size_t *uniqs)
{
	size_t		 i;
	uint32_t	 cas, pas;
	time_t		 expires;

	cas = aspa->custasid;
	expires = aspa->expires;

	*uniqs += aspa->providersz;

	for (i = 0; i < aspa->providersz; i++) {
		pas = aspa->providers[i].as;

		switch (aspa->providers[i].afi) {
		case AFI_IPV4:
			insert_vap(tree, cas, pas, expires, AFI_IPV4);
			(*vaps)++;
			break;
		case AFI_IPV6:
			insert_vap(tree, cas, pas, expires, AFI_IPV6);
			(*vaps)++;
			break;
		default:
			insert_vap(tree, cas, pas, expires, AFI_IPV4);
			insert_vap(tree, cas, pas, expires, AFI_IPV6);
			*vaps += 2;
			break;
		}
	}
}

static inline int
vapcmp(struct vap *a, struct vap *b)
{
	if (a->afi > b->afi)
		return 1;
	if (a->afi < b->afi)
		return -1;

	if (a->custasid > b->custasid)
		return 1;
	if (a->custasid < b->custasid)
		return -1;

	return 0;
}

RB_GENERATE(vap_tree, vap, entry, vapcmp);
