/*	$OpenBSD: aspa.c,v 1.24 2023/10/13 12:06:49 job Exp $ */
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
 * Types and templates for ASPA eContent draft-ietf-sidrops-aspa-profile-15
 */

typedef struct {
	ASN1_INTEGER		*version;
	ASN1_INTEGER		*customerASID;
	STACK_OF(ASN1_INTEGER)	*providers;
} ASProviderAttestation;

ASN1_SEQUENCE(ASProviderAttestation) = {
	ASN1_EXP_OPT(ASProviderAttestation, version, ASN1_INTEGER, 0),
	ASN1_SIMPLE(ASProviderAttestation, customerASID, ASN1_INTEGER),
	ASN1_SEQUENCE_OF(ASProviderAttestation, providers, ASN1_INTEGER),
} ASN1_SEQUENCE_END(ASProviderAttestation);

DECLARE_ASN1_FUNCTIONS(ASProviderAttestation);
IMPLEMENT_ASN1_FUNCTIONS(ASProviderAttestation);

/*
 * Parse the ProviderASSet sequence.
 * Return zero on failure, non-zero on success.
 */
static int
aspa_parse_providers(struct parse *p, const STACK_OF(ASN1_INTEGER) *providers)
{
	const ASN1_INTEGER	*pa;
	uint32_t		 provider;
	size_t			 providersz, i;

	if ((providersz = sk_ASN1_INTEGER_num(providers)) == 0) {
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
		pa = sk_ASN1_INTEGER_value(providers, i);

		memset(&provider, 0, sizeof(provider));

		if (!as_id_parse(pa, &provider)) {
			warnx("%s: ASPA: malformed ProviderAS", p->fn);
			return 0;
		}

		if (p->res->custasid == provider) {
			warnx("%s: ASPA: CustomerASID can't also be Provider",
			    p->fn);
			return 0;
		}

		if (i > 0) {
			if  (p->res->providers[i - 1] > provider) {
				warnx("%s: ASPA: invalid ProviderASSet order",
				    p->fn);
				return 0;
			}
			if (p->res->providers[i - 1] == provider) {
				warnx("%s: ASPA: duplicate ProviderAS", p->fn);
				return 0;
			}
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
		warnx("%s: ASPA: failed to parse ASProviderAttestation", p->fn);
		goto out;
	}

	if (!valid_econtent_version(p->fn, aspa->version, 1))
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
aspa_parse(X509 **x509, const char *fn, int talid, const unsigned char *der,
    size_t len)
{
	struct parse	 p;
	size_t		 cmsz;
	unsigned char	*cms;
	struct cert	*cert = NULL;
	time_t		 signtime = 0;
	int		 rc = 0;

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;

	cms = cms_parse_validate(x509, fn, der, len, aspa_oid, &cmsz,
	    &signtime);
	if (cms == NULL)
		return NULL;

	if ((p.res = calloc(1, sizeof(*p.res))) == NULL)
		err(1, NULL);

	p.res->signtime = signtime;

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

	if (!x509_get_notbefore(*x509, fn, &p.res->notbefore))
		goto out;
	if (!x509_get_notafter(*x509, fn, &p.res->notafter))
		goto out;

	if (x509_any_inherits(*x509)) {
		warnx("%s: inherit elements not allowed in EE cert", fn);
		goto out;
	}

	if (!aspa_parse_econtent(cms, cmsz, &p))
		goto out;

	if ((cert = cert_parse_ee_cert(fn, talid, *x509)) == NULL)
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
	io_simple_buffer(b, &p->talid, sizeof(p->talid));
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
	io_read_buf(b, &p->talid, sizeof(p->talid));
	io_read_buf(b, &p->expires, sizeof(p->expires));

	io_read_buf(b, &p->providersz, sizeof(size_t));
	if ((p->providers = calloc(p->providersz, sizeof(uint32_t))) == NULL)
		err(1, NULL);
	io_read_buf(b, p->providers, p->providersz * sizeof(p->providers[0]));

	io_read_str(b, &p->aia);
	io_read_str(b, &p->aki);
	io_read_str(b, &p->ski);
	assert(p->aia && p->aki && p->ski);

	return p;
}

/*
 * Insert a new uint32_t at index idx in the struct vap v.
 * All elements in the provider array from idx are moved up by one
 * to make space for the new element.
 */
static void
insert_vap(struct vap *v, uint32_t idx, uint32_t *p)
{
	if (idx < v->providersz)
		memmove(v->providers + idx + 1, v->providers + idx,
		    (v->providersz - idx) * sizeof(*v->providers));
	v->providers[idx] = *p;
	v->providersz++;
}

/*
 * Add each ProviderAS entry into the Validated ASPA Providers (VAP) tree.
 * Duplicated entries are merged.
 */
void
aspa_insert_vaps(struct vap_tree *tree, struct aspa *aspa, struct repo *rp)
{
	struct vap	*v, *found;
	size_t		 i, j;

	if ((v = calloc(1, sizeof(*v))) == NULL)
		err(1, NULL);
	v->custasid = aspa->custasid;
	v->talid = aspa->talid;
	if (rp != NULL)
		v->repoid = repo_id(rp);
	else
		v->repoid = 0;
	v->expires = aspa->expires;

	if ((found = RB_INSERT(vap_tree, tree, v)) != NULL) {
		if (found->expires > v->expires) {
			/* decrement found */
			repo_stat_inc(repo_byid(found->repoid), found->talid,
			    RTYPE_ASPA, STYPE_DEC_UNIQUE);
			found->expires = v->expires;
			found->talid = v->talid;
			found->repoid = v->repoid;
			repo_stat_inc(rp, v->talid, RTYPE_ASPA, STYPE_UNIQUE);
		}
		free(v);
		v = found;
	} else
		repo_stat_inc(rp, v->talid, RTYPE_ASPA, STYPE_UNIQUE);

	repo_stat_inc(rp, aspa->talid, RTYPE_ASPA, STYPE_TOTAL);

	v->providers = reallocarray(v->providers,
	    v->providersz + aspa->providersz, sizeof(*v->providers));
	if (v->providers == NULL)
		err(1, NULL);

	/*
	 * Merge all data from aspa into v: loop over all aspa providers,
	 * insert them in the right place in v->providers while keeping the
	 * order of the providers array.
	 */
	for (i = 0, j = 0; i < aspa->providersz; ) {
		if (j == v->providersz ||
		    aspa->providers[i] < v->providers[j]) {
			/* merge provider from aspa into v */
			repo_stat_inc(rp, v->talid, RTYPE_ASPA,
			    STYPE_PROVIDERS);
			insert_vap(v, j, &aspa->providers[i]);
			i++;
		} else if (aspa->providers[i] == v->providers[j])
			i++;

		if (j < v->providersz)
			j++;
	}
}

static inline int
vapcmp(struct vap *a, struct vap *b)
{
	if (a->custasid > b->custasid)
		return 1;
	if (a->custasid < b->custasid)
		return -1;

	return 0;
}

RB_GENERATE(vap_tree, vap, entry, vapcmp);
