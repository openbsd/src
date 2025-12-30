/*	$OpenBSD: ccr.c,v 1.32 2025/12/30 09:04:09 job Exp $ */
/*
 * Copyright (c) 2025 Job Snijders <job@openbsd.org>
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

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/tree.h>

#include <assert.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/stack.h>
#include <openssl/safestack.h>
#include <openssl/x509.h>

#include "extern.h"
#include "rpki-asn1.h"

/*
 * CCR definition in draft-ietf-sidrops-rpki-ccr-01, section 3.
 */

ASN1_ITEM_EXP ContentInfo_it;
ASN1_ITEM_EXP CanonicalCacheRepresentation_it;
ASN1_ITEM_EXP ManifestInstances_it;
ASN1_ITEM_EXP ManifestInstance_it;
ASN1_ITEM_EXP ROAPayloadSets_it;
ASN1_ITEM_EXP ROAPayloadSet_it;
ASN1_ITEM_EXP ASPAPayloadSets_it;
ASN1_ITEM_EXP ASPAPayloadSet_it;
ASN1_ITEM_EXP SubjectKeyIdentifiers_it;
ASN1_ITEM_EXP SubjectKeyIdentifier_it;
ASN1_ITEM_EXP RouterKeySets_it;
ASN1_ITEM_EXP RouterKeySet_it;
ASN1_ITEM_EXP RouterKey_it;

ASN1_SEQUENCE(ContentInfo) = {
	ASN1_SIMPLE(ContentInfo, contentType, ASN1_OBJECT),
	ASN1_EXP(ContentInfo, content, CanonicalCacheRepresentation, 0),
} ASN1_SEQUENCE_END(ContentInfo);

IMPLEMENT_ASN1_FUNCTIONS(ContentInfo);

ASN1_SEQUENCE(CanonicalCacheRepresentation) = {
	ASN1_EXP_OPT(CanonicalCacheRepresentation, version, ASN1_INTEGER, 0),
	ASN1_SIMPLE(CanonicalCacheRepresentation, hashAlg, X509_ALGOR),
	ASN1_SIMPLE(CanonicalCacheRepresentation, producedAt,
	    ASN1_GENERALIZEDTIME),
	ASN1_EXP_OPT(CanonicalCacheRepresentation, mfts, ManifestState, 1),
	ASN1_EXP_OPT(CanonicalCacheRepresentation, vrps, ROAPayloadState, 2),
	ASN1_EXP_OPT(CanonicalCacheRepresentation, vaps, ASPAPayloadState, 3),
	ASN1_EXP_OPT(CanonicalCacheRepresentation, tas, TrustAnchorState, 4),
	ASN1_EXP_OPT(CanonicalCacheRepresentation, rks, RouterKeyState, 5),
} ASN1_SEQUENCE_END(CanonicalCacheRepresentation);

IMPLEMENT_ASN1_FUNCTIONS(CanonicalCacheRepresentation);

ASN1_SEQUENCE(ManifestState) = {
	ASN1_SEQUENCE_OF(ManifestState, mis, ManifestInstance),
	ASN1_SIMPLE(ManifestState, mostRecentUpdate, ASN1_GENERALIZEDTIME),
	ASN1_SIMPLE(ManifestState, hash, ASN1_OCTET_STRING),
} ASN1_SEQUENCE_END(ManifestState);

IMPLEMENT_ASN1_FUNCTIONS(ManifestState);

ASN1_ITEM_TEMPLATE(ManifestInstances) =
    ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, mis, ManifestInstance)
ASN1_ITEM_TEMPLATE_END(ManifestInstances);

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_fname(ManifestInstances, ManifestInstances,
    ManifestInstances);

ASN1_SEQUENCE(ManifestInstance) = {
	ASN1_SIMPLE(ManifestInstance, hash, ASN1_OCTET_STRING),
	ASN1_SIMPLE(ManifestInstance, size, ASN1_INTEGER),
	ASN1_SIMPLE(ManifestInstance, aki, ASN1_OCTET_STRING),
	ASN1_SIMPLE(ManifestInstance, manifestNumber, ASN1_INTEGER),
	ASN1_SIMPLE(ManifestInstance, thisUpdate, ASN1_GENERALIZEDTIME),
	ASN1_SEQUENCE_OF(ManifestInstance, locations, ACCESS_DESCRIPTION),
	ASN1_SEQUENCE_OF_OPT(ManifestInstance, subordinates,
	    SubjectKeyIdentifier),
} ASN1_SEQUENCE_END(ManifestInstance);

IMPLEMENT_ASN1_FUNCTIONS(ManifestInstance);

ASN1_SEQUENCE(ROAPayloadState) = {
	ASN1_SEQUENCE_OF(ROAPayloadState, rps, ROAPayloadSet),
	ASN1_SIMPLE(ROAPayloadState, hash, ASN1_OCTET_STRING),
} ASN1_SEQUENCE_END(ROAPayloadState);

IMPLEMENT_ASN1_FUNCTIONS(ROAPayloadState);

ASN1_ITEM_TEMPLATE(ROAPayloadSets) =
    ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, rps, ROAPayloadSet)
ASN1_ITEM_TEMPLATE_END(ROAPayloadSets);

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_fname(ROAPayloadSets, ROAPayloadSets,
    ROAPayloadSets);

ASN1_SEQUENCE(ROAPayloadSet) = {
	ASN1_SIMPLE(ROAPayloadSet, asID, ASN1_INTEGER),
	ASN1_SEQUENCE_OF(ROAPayloadSet, ipAddrBlocks, ROAIPAddressFamily),
} ASN1_SEQUENCE_END(ROAPayloadSet);

IMPLEMENT_ASN1_FUNCTIONS(ROAPayloadSet);
IMPLEMENT_ASN1_FUNCTIONS(ROAIPAddressFamily);
IMPLEMENT_ASN1_FUNCTIONS(ROAIPAddress);

ASN1_SEQUENCE(ASPAPayloadState) = {
	ASN1_SEQUENCE_OF(ASPAPayloadState, aps, ASPAPayloadSet),
	ASN1_SIMPLE(ASPAPayloadState, hash, ASN1_OCTET_STRING),
} ASN1_SEQUENCE_END(ASPAPayloadState);

IMPLEMENT_ASN1_FUNCTIONS(ASPAPayloadState);

ASN1_ITEM_TEMPLATE(ASPAPayloadSets) =
    ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, aps, ASPAPayloadSet)
ASN1_ITEM_TEMPLATE_END(ASPAPayloadSets)

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_fname(ASPAPayloadSets, ASPAPayloadSets,
    ASPAPayloadSets);

ASN1_SEQUENCE(ASPAPayloadSet) = {
	ASN1_SIMPLE(ASPAPayloadSet, asID, ASN1_INTEGER),
	ASN1_SEQUENCE_OF(ASPAPayloadSet, providers, ASN1_INTEGER),
} ASN1_SEQUENCE_END(ASPAPayloadSet);

IMPLEMENT_ASN1_FUNCTIONS(ASPAPayloadSet);

IMPLEMENT_ASN1_TYPE_ex(SubjectKeyIdentifier, ASN1_OCTET_STRING, 0)
IMPLEMENT_ASN1_FUNCTIONS(SubjectKeyIdentifier);

ASN1_SEQUENCE(TrustAnchorState) = {
	ASN1_SEQUENCE_OF(TrustAnchorState, skis, SubjectKeyIdentifier),
	ASN1_SIMPLE(TrustAnchorState, hash, ASN1_OCTET_STRING),
} ASN1_SEQUENCE_END(TrustAnchorState);

IMPLEMENT_ASN1_FUNCTIONS(TrustAnchorState);

ASN1_ITEM_TEMPLATE(SubjectKeyIdentifiers) =
	ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, tas,
	    SubjectKeyIdentifier)
ASN1_ITEM_TEMPLATE_END(SubjectKeyIdentifiers);

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_fname(SubjectKeyIdentifiers,
    SubjectKeyIdentifiers, SubjectKeyIdentifiers);

ASN1_SEQUENCE(RouterKeyState) = {
	ASN1_SEQUENCE_OF(RouterKeyState, rksets, RouterKeySet),
	ASN1_SIMPLE(RouterKeyState, hash, ASN1_OCTET_STRING),
} ASN1_SEQUENCE_END(RouterKeyState);

IMPLEMENT_ASN1_FUNCTIONS(RouterKeyState);

ASN1_ITEM_TEMPLATE(RouterKeySets) =
    ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, rks, RouterKeySet)
ASN1_ITEM_TEMPLATE_END(RouterKeySets);

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_fname(RouterKeySets, RouterKeySets,
    RouterKeySets);

ASN1_SEQUENCE(RouterKeySet) = {
	ASN1_SIMPLE(RouterKeySet, asID, ASN1_INTEGER),
	ASN1_SEQUENCE_OF(RouterKeySet, routerKeys, RouterKey),
} ASN1_SEQUENCE_END(RouterKeySet);

IMPLEMENT_ASN1_FUNCTIONS(RouterKeySet);

ASN1_SEQUENCE(RouterKey) = {
	ASN1_SIMPLE(RouterKey, ski, SubjectKeyIdentifier),
	ASN1_SIMPLE(RouterKey, spki, X509_PUBKEY),
} ASN1_SEQUENCE_END(RouterKey);

IMPLEMENT_ASN1_FUNCTIONS(RouterKey);

static char *
hex_encode_asn1_string(const ASN1_STRING *str)
{
	return hex_encode(ASN1_STRING_get0_data(str), ASN1_STRING_length(str));
}

static int
base64_encode_asn1_string(const ASN1_OCTET_STRING *astr, char **out)
{
	const unsigned char *data = ASN1_STRING_get0_data(astr);
	int length = ASN1_STRING_length(astr);

	return base64_encode(data, length, out) == 0;
}

static int
copy_asn1_string(const ASN1_STRING *astr, unsigned char *buf, size_t len)
{
	const unsigned char *data = ASN1_STRING_get0_data(astr);
	int length = ASN1_STRING_length(astr);

	if (length < 0 || (size_t)length != len)
		return 0;

	memcpy(buf, data, length);

	return 1;
}

static void
hash_asn1_item(ASN1_OCTET_STRING *astr, const ASN1_ITEM *it, void *val)
{
	unsigned char hash[SHA256_DIGEST_LENGTH];

	if (!ASN1_item_digest(it, EVP_sha256(), val, hash, NULL))
		errx(1, "ASN1_item_digest");

	if (!ASN1_OCTET_STRING_set(astr, hash, sizeof(hash)))
		errx(1, "ASN1_OCTET_STRING_set");
}

static char *
validate_asn1_hash(const char *fn, const char *descr,
    const ASN1_OCTET_STRING *hash, const ASN1_ITEM *it, void *val)
{
	ASN1_OCTET_STRING *astr = NULL;
	char *b64 = NULL;

	if ((astr = ASN1_OCTET_STRING_new()) == NULL)
		errx(1, "ASN1_OCTET_STRING_new");

	hash_asn1_item(astr, it, val);

	if (ASN1_OCTET_STRING_cmp(hash, astr) != 0) {
		warnx("%s: corrupted %s state", fn, descr);
		goto out;
	}

	if (!base64_encode_asn1_string(astr, &b64))
		errx(1, "base64_encode_asn1_string");

 out:
	ASN1_OCTET_STRING_free(astr);
	return b64;
}

static void
asn1int_set_seqnum(ASN1_INTEGER *aint, const char *seqnum)
{
	BIGNUM *bn = NULL;

	if (!BN_hex2bn(&bn, seqnum))
		errx(1, "BN_hex2bn");

	if (BN_to_ASN1_INTEGER(bn, aint) == NULL)
		errx(1, "BN_to_ASN1_INTEGER");

	BN_free(bn);
}

static void
location_add_sia(STACK_OF(ACCESS_DESCRIPTION) *sad, const char *sia)
{
	ACCESS_DESCRIPTION *ad = NULL;

	if ((ad = ACCESS_DESCRIPTION_new()) == NULL)
		errx(1, "ACCESS_DESCRIPTION_new");

	ASN1_OBJECT_free(ad->method);
	if ((ad->method = OBJ_dup(signedobj_oid)) == NULL)
		errx(1, "OBJ_dup");

	GENERAL_NAME_free(ad->location);
	ad->location = a2i_GENERAL_NAME(NULL, NULL, NULL, GEN_URI, sia, 0);
	if (ad->location == NULL)
		errx(1, "a2i_GENERAL_NAME");

	if (sk_ACCESS_DESCRIPTION_push(sad, ad) <= 0)
		errx(1, "sk_ACCESS_DESCRIPTION_push");
}

static int
ski_cmp(const SubjectKeyIdentifier *const *a, const SubjectKeyIdentifier *const *b)
{
	return ASN1_OCTET_STRING_cmp(*a, *b);
}

static void
append_cached_manifest(STACK_OF(ManifestInstance) *mis, struct ccr_mft *cm)
{
	ManifestInstance *mi;
	struct ccr_mft_sub_ski *sub;
	SubjectKeyIdentifier *ski;

	if ((mi = ManifestInstance_new()) == NULL)
		errx(1, "ManifestInstance_new");

	if (!ASN1_OCTET_STRING_set(mi->hash, cm->hash, sizeof(cm->hash)))
		errx(1, "ASN1_OCTET_STRING_set");

	if (!ASN1_OCTET_STRING_set(mi->aki, cm->aki, sizeof(cm->aki)))
		errx(1, "ASN1_OCTET_STRING_set");

	if (!ASN1_INTEGER_set_uint64(mi->size, cm->size))
		errx(1, "ASN1_INTEGER_set_uint64");

	asn1int_set_seqnum(mi->manifestNumber, cm->seqnum);

	if (ASN1_GENERALIZEDTIME_set(mi->thisUpdate, cm->thisupdate) == NULL)
		errx(1, "ASN1_GENERALIZEDTIME_set");

	location_add_sia(mi->locations, cm->sia);

	if (SLIST_EMPTY(&cm->subordinates))
		goto done;

	if ((mi->subordinates = sk_SubjectKeyIdentifier_new(ski_cmp)) == NULL)
		err(1, NULL);

	SLIST_FOREACH(sub, &cm->subordinates, entry) {
		if ((ski = SubjectKeyIdentifier_new()) == NULL)
			err(1, NULL);

		if (!ASN1_OCTET_STRING_set(ski, sub->ski, sizeof(sub->ski)))
			errx(1, "ASN1_OCTET_STRING_set");

		if (sk_SubjectKeyIdentifier_push(mi->subordinates, ski) <= 0)
			errx(1, "sk_SubjectKeyIdentifier_push");
	}

	sk_SubjectKeyIdentifier_sort(mi->subordinates);

 done:
	if (sk_ManifestInstance_push(mis, mi) <= 0)
		errx(1, "sk_ManifestInstance_push");
}

static ManifestState *
generate_manifeststate(struct validation_data *vd)
{
	struct ccr *ccr = &vd->ccr;
	ManifestState *ms;
	struct ccr_mft *cm;
	time_t most_recent_update = 0;

	if ((ms = ManifestState_new()) == NULL)
		errx(1, "ManifestState_new");

	RB_FOREACH(cm, ccr_mft_tree, &ccr->mfts) {
		append_cached_manifest(ms->mis, cm);

		if (cm->thisupdate > most_recent_update)
			most_recent_update = cm->thisupdate;
	}

	if (ASN1_GENERALIZEDTIME_set(ms->mostRecentUpdate,
	    most_recent_update) == NULL)
		errx(1, "ASN1_GENERALIZEDTIME_set");

	hash_asn1_item(ms->hash, ASN1_ITEM_rptr(ManifestInstances), ms->mis);

	if (!base64_encode_asn1_string(ms->hash, &ccr->mfts_hash))
		errx(1, "base64_encode_asn1_string");

	return ms;
}

static void
append_cached_vrp(STACK_OF(ROAIPAddress) *addresses, struct vrp *vrp)
{
	ROAIPAddress *ripa;
	int num_bits, num_bytes;
	uint8_t unused_bits;

	if ((ripa = ROAIPAddress_new()) == NULL)
		errx(1, "ROAIPAddress_new");

	num_bytes = (vrp->addr.prefixlen + 7) / 8;
	num_bits = vrp->addr.prefixlen % 8;

	unused_bits = 0;
	if (num_bits > 0)
		unused_bits = 8 - num_bits;

	if (!ASN1_BIT_STRING_set(ripa->address, vrp->addr.addr, num_bytes))
		errx(1, "ASN1_BIT_STRING_set");

	/* ip_addr_parse() handles unused bits, no need to clear them here. */
	ripa->address->flags |= ASN1_STRING_FLAG_BITS_LEFT | unused_bits;

	/* XXX - assert that unused bits are zero */

	if (vrp->maxlength > vrp->addr.prefixlen) {
		if ((ripa->maxLength = ASN1_INTEGER_new()) == NULL)
			errx(1, "ASN1_INTEGER_new");

		if (!ASN1_INTEGER_set_uint64(ripa->maxLength, vrp->maxlength))
			errx(1, "ASN1_INTEGER_set_uint64");
	}

	if (sk_ROAIPAddress_push(addresses, ripa) <= 0)
		errx(1, "sk_ROAIPAddress_push");
}

static ROAPayloadState *
generate_roapayloadstate(struct validation_data *vd)
{
	struct ccr *ccr = &vd->ccr;
	ROAPayloadState *vrps;
	struct vrp *prev, *vrp;
	ROAPayloadSet *rp = NULL;
	ROAIPAddressFamily *ripaf = NULL;
	unsigned char afibuf[2];

	if ((vrps = ROAPayloadState_new()) == NULL)
		errx(1, "ROAPayloadState_new");

	prev = NULL;
	RB_FOREACH(vrp, ccr_vrp_tree, &ccr->vrps) {
		if (prev == NULL || prev->asid != vrp->asid) {
			if ((rp = ROAPayloadSet_new()) == NULL)
				errx(1, "ROAPayloadSet_new");

			if (!ASN1_INTEGER_set_uint64(rp->asID, vrp->asid))
				errx(1, "ASN1_INTEGER_set_uint64");

			if (sk_ROAPayloadSet_push(vrps->rps, rp) <= 0)
				errx(1, "sk_ROAPayloadSet_push");
		}

		if (prev == NULL || prev->asid != vrp->asid ||
		    prev->afi != vrp->afi) {
			if ((ripaf = ROAIPAddressFamily_new()) == NULL)
				errx(1, "ROAIPAddressFamily_new");

			/* vrp->afi is 1 or 2. */
			assert(vrp->afi == AFI_IPV4 || vrp->afi == AFI_IPV6);

			afibuf[0] = 0;
			afibuf[1] = vrp->afi;
			if (!ASN1_OCTET_STRING_set(ripaf->addressFamily, afibuf,
			    sizeof(afibuf)))
				errx(1, "ASN1_OCTET_STRING_set");

			if (sk_ROAIPAddressFamily_push(rp->ipAddrBlocks,
			    ripaf) <= 0)
				errx(1, "sk_ROAIPAddressFamily_push");
		}

		append_cached_vrp(ripaf->addresses, vrp);
		prev = vrp;
	}

	hash_asn1_item(vrps->hash, ASN1_ITEM_rptr(ROAPayloadSets), vrps->rps);

	if (!base64_encode_asn1_string(vrps->hash, &ccr->vrps_hash))
		errx(1, "base64_encode_asn1_string");

	return vrps;
}

static void
append_cached_aspa(STACK_OF(ASPAPayloadSet) *aps, struct vap *vap)
{
	ASPAPayloadSet *ap;
	size_t i;

	if ((ap = ASPAPayloadSet_new()) == NULL)
		errx(1, "ASPAPayloadSet_new");

	if (!ASN1_INTEGER_set_uint64(ap->asID, vap->custasid))
		errx(1, "ASN1_INTEGER_set_uint64");

	for (i = 0; i < vap->num_providers; i++) {
		ASN1_INTEGER *ai;

		if ((ai = ASN1_INTEGER_new()) == NULL)
			errx(1, "ASN1_INTEGER_new");

		if (!ASN1_INTEGER_set_uint64(ai, vap->providers[i]))
			errx(1, "ASN1_INTEGER_set_uint64");

		if ((sk_ASN1_INTEGER_push(ap->providers, ai)) <= 0)
			errx(1, "sk_ASN1_INTEGER_push");
	}

	if ((sk_ASPAPayloadSet_push(aps, ap)) <= 0)
		errx(1, "sk_ASPAPayloadSet_push");
}

static ASPAPayloadState *
generate_aspapayloadstate(struct validation_data *vd)
{
	struct ccr *ccr = &vd->ccr;
	ASPAPayloadState *vaps;
	struct vap *vap;

	if ((vaps = ASPAPayloadState_new()) == NULL)
		errx(1, "ASPAPayloadState_new");

	RB_FOREACH(vap, vap_tree, &vd->vaps) {
		append_cached_aspa(vaps->aps, vap);
	}

	hash_asn1_item(vaps->hash, ASN1_ITEM_rptr(ASPAPayloadSets), vaps->aps);

	if (!base64_encode_asn1_string(vaps->hash, &ccr->vaps_hash))
		errx(1, "base64_encode_asn1_string");

	return vaps;
}

static void
append_tas_ski(STACK_OF(SubjectKeyIdentifier) *skis, struct ccr_tas_ski *cts)
{
	SubjectKeyIdentifier *ski;

	if ((ski = SubjectKeyIdentifier_new()) == NULL)
		errx(1, "SubjectKeyIdentifier_new");

	if (!ASN1_OCTET_STRING_set(ski, cts->keyid, sizeof(cts->keyid)))
		errx(1, "ASN1_OCTET_STRING_set");

	if ((sk_SubjectKeyIdentifier_push(skis, ski)) <= 0)
		errx(1, "sk_SubjectKeyIdentifier_push");
}

static TrustAnchorState *
generate_trustanchorstate(struct validation_data *vd)
{
	TrustAnchorState *tas;
	struct ccr_tas_ski *cts;

	if ((tas = TrustAnchorState_new()) == NULL)
		errx(1, "TrustAnchorState_new");

	RB_FOREACH(cts, ccr_tas_tree, &vd->ccr.tas) {
		append_tas_ski(tas->skis, cts);
	}

	hash_asn1_item(tas->hash, ASN1_ITEM_rptr(SubjectKeyIdentifiers),
	    tas->skis);

	if (!base64_encode_asn1_string(tas->hash, &vd->ccr.tas_hash))
		errx(1, "base64_encode_asn1_string");

	return tas;
}

static RouterKeyState *
generate_routerkeystate(struct validation_data *vd)
{
	RouterKeyState *rks;
	RouterKeySet *rkset;
	RouterKey *rk;
	struct brk *brk, *prev = NULL;
	unsigned char *pk_der = NULL;
	size_t pk_len;
	const unsigned char *der;

	if ((rks = RouterKeyState_new()) == NULL)
		errx(1, "RouterKeyState_new");

	prev = NULL;
	RB_FOREACH(brk, brk_tree, &vd->brks) {
		static char hbuf[SHA_DIGEST_LENGTH] = { 0 };

		if (prev == NULL || prev->asid != brk->asid) {
			if ((rkset = RouterKeySet_new()) == NULL)
				errx(1, "RouterKeySet_new");

			if (!ASN1_INTEGER_set_uint64(rkset->asID, brk->asid))
				errx(1, "ASN1_INTEGER_set_uint64");

			if (sk_RouterKeySet_push(rks->rksets, rkset) <= 0)
				errx(1, "sk_RouterKeySet_push");
		}

		if ((rk = RouterKey_new()) == NULL)
			errx(1, "RouterKey_new");

		if (hex_decode(brk->ski, hbuf, sizeof(hbuf)) == -1)
			errx(1, "hex_decode brk pubkey corrupted");

		if (!ASN1_OCTET_STRING_set(rk->ski, hbuf, sizeof(hbuf)))
			errx(1, "ASN1_OCTET_STRING_set");

		if (base64_decode(brk->pubkey, strlen(brk->pubkey), &pk_der,
		    &pk_len) == -1)
			errx(1, "base64_decode");

		X509_PUBKEY_free(rk->spki);
		der = pk_der;
		if ((rk->spki = d2i_X509_PUBKEY(NULL, &der, pk_len)) == NULL)
			errx(1, "d2i_X509_PUBKEY");
		if (der != pk_der + pk_len)
			errx(1, "brk pubkey corrupted");
		free(pk_der);
		pk_der = NULL;

		if ((sk_RouterKey_push(rkset->routerKeys, rk)) <= 0)
			errx(1, "sk_RouterKey_push");

		prev = brk;
	}

	hash_asn1_item(rks->hash, ASN1_ITEM_rptr(RouterKeySets), rks->rksets);

	if (!base64_encode_asn1_string(rks->hash, &vd->ccr.brks_hash))
		errx(1, "base64_encode_asn1_string");

	return rks;
}

static CanonicalCacheRepresentation *
generate_ccr(struct validation_data *vd)
{
	CanonicalCacheRepresentation *ccr = NULL;
	ASN1_OBJECT *oid;

	if ((ccr = CanonicalCacheRepresentation_new()) == NULL)
		errx(1, "CanonicalCacheRepresentation_new");

	if ((oid = OBJ_nid2obj(NID_sha256)) == NULL)
		errx(1, "OBJ_nid2obj");

	if (!X509_ALGOR_set0(ccr->hashAlg, oid, V_ASN1_UNDEF, NULL))
		errx(1, "X509_ALGOR_set0");

	if (ASN1_GENERALIZEDTIME_set(ccr->producedAt, vd->buildtime) == NULL)
		errx(1, "ASN1_GENERALIZEDTIME_set");

	if ((ccr->mfts = generate_manifeststate(vd)) == NULL)
		errx(1, "generate_manifeststate");

	if ((ccr->vrps = generate_roapayloadstate(vd)) == NULL)
		errx(1, "generate_roapayloadstate");

	if ((ccr->vaps = generate_aspapayloadstate(vd)) == NULL)
		errx(1, "generate_aspapayloadstate");

	if ((ccr->tas = generate_trustanchorstate(vd)) == NULL)
		errx(1, "generate_trustanchorstate");

	if ((ccr->rks = generate_routerkeystate(vd)) == NULL)
		errx(1, "generate_routerkeystate");

	return ccr;
}

void
serialize_ccr_content(struct validation_data *vd)
{
	ContentInfo *ci = NULL;
	int ci_der_len;

	if ((ci = ContentInfo_new()) == NULL)
		errx(1, "ContentInfo_new");

	ASN1_OBJECT_free(ci->contentType);
	if ((ci->contentType = OBJ_dup(ccr_oid)) == NULL)
		errx(1, "OBJ_dup");

	CanonicalCacheRepresentation_free(ci->content);
	ci->content = generate_ccr(vd);

	vd->ccr.der = NULL;
	if ((ci_der_len = i2d_ContentInfo(ci, &vd->ccr.der)) <= 0)
		errx(1, "i2d_ContentInfo");
	vd->ccr.der_len = ci_der_len;

	ContentInfo_free(ci);
}

static inline int
ccr_tas_ski_cmp(const struct ccr_tas_ski *a, const struct ccr_tas_ski *b)
{
	return memcmp(a->keyid, b->keyid, SHA_DIGEST_LENGTH);
}

RB_GENERATE(ccr_tas_tree, ccr_tas_ski, entry, ccr_tas_ski_cmp);

void
ccr_insert_tas(struct ccr_tas_tree *tree, const struct cert *cert)
{
	struct ccr_tas_ski *cts;

	assert(cert->purpose == CERT_PURPOSE_TA);

	if ((cts = calloc(1, sizeof(*cts))) == NULL)
		err(1, NULL);

	if ((hex_decode(cert->ski, cts->keyid, sizeof(cts->keyid))) != 0)
		errx(1, "hex_decode");

	if (RB_INSERT(ccr_tas_tree, tree, cts) != NULL)
		errx(1, "multiple TALs with the same key are not supported");
}

void
ccr_insert_mft_sub(struct ccr_mft_tree *tree, const struct cert *cert)
{
	struct ccr_mft *m, needle = { 0 };
	struct ccr_mft_sub_ski *sub;

	assert(cert->purpose == CERT_PURPOSE_CA);

	memcpy(needle.hash, cert->mfthash, sizeof(cert->mfthash));

	if ((m = RB_FIND(ccr_mft_tree, tree, &needle)) == NULL)
		errx(1, "RB_FIND ccr_mft_tree");

	if ((sub = calloc(1, sizeof(*sub))) == NULL)
		err(1, NULL);

	if (hex_decode(cert->ski, sub->ski, sizeof(sub->ski)) != 0)
		errx(1, "hex_decode");

	SLIST_INSERT_HEAD(&m->subordinates, sub, entry);
}

static inline int
ccr_mft_cmp(const struct ccr_mft *a, const struct ccr_mft *b)
{
	return memcmp(a->hash, b->hash, SHA256_DIGEST_LENGTH);
}

RB_GENERATE(ccr_mft_tree, ccr_mft, entry, ccr_mft_cmp);

static struct ccr_mft *
ccr_mft_new(void)
{
	struct ccr_mft *ccr_mft = NULL;

	if ((ccr_mft = calloc(1, sizeof(*ccr_mft))) == NULL)
		err(1, NULL);

	SLIST_INIT(&ccr_mft->subordinates);

	return ccr_mft;
}

void
ccr_insert_mft(struct ccr_mft_tree *tree, const struct mft *mft)
{
	struct ccr_mft *ccr_mft;

	ccr_mft = ccr_mft_new();

	if (hex_decode(mft->aki, ccr_mft->aki, sizeof(ccr_mft->aki)) != 0)
		errx(1, "hex_decode");

	if ((ccr_mft->sia = strdup(mft->sia)) == NULL)
		err(1, NULL);

	if ((ccr_mft->seqnum = strdup(mft->seqnum)) == NULL)
		err(1, NULL);

	memcpy(ccr_mft->hash, mft->mfthash, sizeof(ccr_mft->hash));

	ccr_mft->size = mft->mftsize;
	ccr_mft->thisupdate = mft->thisupdate;

	if (RB_INSERT(ccr_mft_tree, tree, ccr_mft) != NULL)
		errx(1, "CCR MFT tree corrupted");
}

void
ccr_insert_roa(struct ccr_vrp_tree *tree, const struct roa *roa)
{
	struct vrp *vrp;
	size_t i;

	for (i = 0; i < roa->num_ips; i++) {
		if ((vrp = calloc(1, sizeof(*vrp))) == NULL)
			err(1, NULL);

		vrp->asid = roa->asid;
		vrp->afi = roa->ips[i].afi;
		vrp->addr = roa->ips[i].addr;
		vrp->maxlength = roa->ips[i].maxlength;

		if (RB_INSERT(ccr_vrp_tree, tree, vrp) != NULL)
			free(vrp);
	}
}

/*
 * Total ordering modeled after RFC 9582, section 4.3.3.
 */
static inline int
ccr_vrp_cmp(const struct vrp *a, const struct vrp *b)
{
	int rv;

	if (a->asid > b->asid)
		return 1;
	if (a->asid < b->asid)
		return -1;

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
		abort();
		break;
	}

	if (a->addr.prefixlen < b->addr.prefixlen)
		return 1;
	if (a->addr.prefixlen > b->addr.prefixlen)
		return -1;

	if (a->maxlength < b->maxlength)
		return 1;
	if (a->maxlength > b->maxlength)
		return -1;

	return 0;
}

RB_GENERATE(ccr_vrp_tree, vrp, entry, ccr_vrp_cmp);

int
output_ccr_der(FILE *out, struct validation_data *vd, struct stats *st)
{
	if (fwrite(vd->ccr.der, vd->ccr.der_len, 1, out) != 1)
		err(1, "fwrite");

	return 0;
}

static void
ccr_mft_free(struct ccr_mft *ccr_mft)
{
	struct ccr_mft_sub_ski *sub_ski;

	if (ccr_mft == NULL)
		return;

	while (!SLIST_EMPTY(&ccr_mft->subordinates)) {
		sub_ski = SLIST_FIRST(&ccr_mft->subordinates);
		SLIST_REMOVE_HEAD(&ccr_mft->subordinates, entry);
		free(sub_ski);
	}

	free(ccr_mft->seqnum);
	free(ccr_mft->sia);
	free(ccr_mft);
}

static void
ccr_mfts_free(struct ccr_mft_tree *mfts)
{
	struct ccr_mft *ccr_mft, *tmp_ccr_mft;

	RB_FOREACH_SAFE(ccr_mft, ccr_mft_tree, mfts, tmp_ccr_mft) {
		RB_REMOVE(ccr_mft_tree, mfts, ccr_mft);
		ccr_mft_free(ccr_mft);
	}
}

static void
ccr_vrps_free(struct ccr_vrp_tree *vrps)
{
	struct vrp *vrp, *tmp_vrp;

	RB_FOREACH_SAFE(vrp, ccr_vrp_tree, vrps, tmp_vrp) {
		RB_REMOVE(ccr_vrp_tree, vrps, vrp);
		free(vrp);
	}
}

static void
vap_free(struct vap *vap)
{
	if (vap == NULL)
		return;

	free(vap->providers);
	free(vap);
}

static void
ccr_vaps_free(struct vap_tree *vaps)
{
	struct vap *vap, *tmp_vap;

	RB_FOREACH_SAFE(vap, vap_tree, vaps, tmp_vap) {
		RB_REMOVE(vap_tree, vaps, vap);
		vap_free(vap);
	}
}

static void
ccr_tas_free(struct ccr_tas_tree *tas)
{
	struct ccr_tas_ski *cts, *tmp_cts;

	RB_FOREACH_SAFE(cts, ccr_tas_tree, tas, tmp_cts) {
		RB_REMOVE(ccr_tas_tree, tas, cts);
		free(cts);
	}
}

static void
brk_free(struct brk *brk)
{
	if (brk == NULL)
		return;

	free(brk->ski);
	free(brk->pubkey);
	free(brk);
}

static void
ccr_brks_free(struct brk_tree *brks)
{
	struct brk *brk, *tmp_brk;

	RB_FOREACH_SAFE(brk, brk_tree, brks, tmp_brk) {
		RB_REMOVE(brk_tree, brks, brk);
		brk_free(brk);
	}
}

void
ccr_free(struct ccr *ccr)
{
	if (ccr == NULL)
		return;

	ccr_mfts_free(&ccr->mfts);
	ccr_vrps_free(&ccr->vrps);
	ccr_vaps_free(&ccr->vaps);
	ccr_tas_free(&ccr->tas);
	ccr_brks_free(&ccr->brks);

	free(ccr->mfts_hash);
	free(ccr->vrps_hash);
	free(ccr->vaps_hash);
	free(ccr->tas_hash);
	free(ccr->brks_hash);
	free(ccr->der);
	free(ccr);
}

static int
parse_mft_instances(const char *fn, struct ccr *ccr,
    const STACK_OF(ManifestInstance) *mis)
{
	ManifestInstance *mi;
	struct ccr_mft *ccr_mft = NULL, *prev;
	int i, j, instances_num, sub_num;
	const ACCESS_DESCRIPTION *ad;
	const SubjectKeyIdentifier *s;
	struct ccr_mft_sub_ski *sub = NULL;
	int rc = 0;
	uint64_t size = 0;

	instances_num = sk_ManifestInstance_num(mis);

	RB_INIT(&ccr->mfts);

	prev = NULL;
	for (i = 0; i < instances_num; i++) {
		ccr_mft = ccr_mft_new();

		mi = sk_ManifestInstance_value(mis, i);

		if (!copy_asn1_string(mi->hash,
		    ccr_mft->hash, sizeof(ccr_mft->hash))) {
			warnx("%s: manifest instance #%d corrupted", fn, i);
			goto out;
		}

		if (prev != NULL) {
			if (ccr_mft_cmp(ccr_mft, prev) <= 0) {
				warnx("%s: misordered ManifestInstances", fn);
				goto out;
			}
		}

		if (!copy_asn1_string(mi->aki,
		    ccr_mft->aki, sizeof(ccr_mft->aki))) {
			warnx("%s: manifest instance #%d corrupted", fn, i);
			goto out;
		}

		if (!ASN1_INTEGER_get_uint64(&size, mi->size)) {
			warnx("%s: manifest instance #%d corrupted", fn, i);
			goto out;
		}
		if (size < 1000 || size > MAX_FILE_SIZE) {
			warnx("%s: manifest instance #%d corrupted", fn, i);
			goto out;
		}
		ccr_mft->size = size;

		ccr_mft->seqnum = x509_convert_seqnum(fn, "manifest number",
		    mi->manifestNumber);
		if (ccr_mft->seqnum == NULL)
			goto out;

		if (!x509_get_generalized_time(fn, "ManifestInstance "
		    "thisUpdate", mi->thisUpdate, &ccr_mft->thisupdate))
			goto out;

		if (sk_ACCESS_DESCRIPTION_num(mi->locations) != 1) {
			warnx("%s: unexpected number of locations", fn);
			goto out;
		}

		ad = sk_ACCESS_DESCRIPTION_value(mi->locations, 0);

		if (!x509_location(fn, "SIA: signedObject", ad->location,
		    &ccr_mft->sia))
			goto out;

		sub_num = sk_SubjectKeyIdentifier_num(mi->subordinates);
		if (sub_num <= 0)
			goto insert;

		for (j = 0; j < sub_num; j++) {
			if ((sub = calloc(1, sizeof(*sub))) == NULL)
				err(1, NULL);

			s = sk_SubjectKeyIdentifier_value(mi->subordinates, j);
			if (!copy_asn1_string(s, sub->ski, sizeof(sub->ski))) {
				warnx("%s: manifest instance #%d corrupted",
				    fn, i);
				goto out;
			}
			SLIST_INSERT_HEAD(&ccr_mft->subordinates, sub, entry);
			sub = NULL;
		}

 insert:
		if (RB_INSERT(ccr_mft_tree, &ccr->mfts, ccr_mft) != NULL) {
			warnx("%s: manifest state corrupted", fn);
			goto out;
		}

		prev = ccr_mft;
		ccr_mft = NULL;
	}

	rc = 1;
 out:
	ccr_mft_free(ccr_mft);
	free(sub);
	return rc;
}

static int
parse_manifeststate(const char *fn, struct ccr *ccr, const ManifestState *state)
{
	int rc = 0;

	ccr->mfts_hash = validate_asn1_hash(fn, "ManifestState", state->hash,
	    ASN1_ITEM_rptr(ManifestInstances), state->mis);
	if (ccr->mfts_hash == NULL)
		goto out;

	if (!x509_get_generalized_time(fn, "CCR mostRecentUpdate",
	    state->mostRecentUpdate, &ccr->most_recent_update))
		goto out;

	if (!parse_mft_instances(fn, ccr, state->mis))
		goto out;

	rc = 1;
 out:
	return rc;
}

static int
parse_roa_addresses(const char *fn, struct ccr *ccr, int asid, enum afi afi,
    const STACK_OF(ROAIPAddress) *addrs)
{
	const ROAIPAddress *r;
	struct vrp *prev, *vrp = NULL;
	uint64_t maxlen;
	int addrs_num, i, rc = 0;

	if ((addrs_num = sk_ROAIPAddress_num(addrs)) <= 0) {
		warnx("%s: missing ROAIPAddress", fn);
		goto out;
	}

	prev = NULL;
	for (i = 0; i < addrs_num; i++) {
		r = sk_ROAIPAddress_value(addrs, i);

		if ((vrp = calloc(1, sizeof(*vrp))) == NULL)
			err(1, NULL);

		vrp->asid = asid;
		vrp->afi = afi;

		if (!ip_addr_parse(r->address, afi, fn, &vrp->addr)) {
			warnx("%s: invalid address in ROAPayload", fn);
			goto out;
		}

		maxlen = vrp->addr.prefixlen;
		if (r->maxLength != NULL) {
			if (!ASN1_INTEGER_get_uint64(&maxlen, r->maxLength)) {
				warnx("%s: ASN1_INTEGER_get_uint64 failed", fn);
				goto out;
			}
			if (vrp->addr.prefixlen > maxlen) {
				warnx("%s: invalid maxLength", fn);
				goto out;
			}
			if (maxlen > ((afi == AFI_IPV4) ? 32 : 128)) {
				warnx("%s: maxLength too large", fn);
				goto out;
			}
			vrp->maxlength = maxlen;
		}

		if (prev != NULL) {
			if (ccr_vrp_cmp(vrp, prev) <= 0) {
				warnx("%s: misordered ROAIPAddressFamily", fn);
				goto out;
			}
		}

		if ((RB_INSERT(ccr_vrp_tree, &ccr->vrps, vrp)) != NULL) {
			warnx("%s: duplicate ROAIPAddress", fn);
			goto out;
		}

		prev = vrp;
		vrp = NULL;
	}

	rc = 1;
 out:
	free(vrp);
	return rc;
}

static int
parse_roa_ipaddrb(const char *fn, struct ccr *ccr, int asid,
    const STACK_OF(ROAIPAddressFamily) *ipaddrblocks)
{
	const ROAIPAddressFamily *ripaf;
	enum afi afi;
	int ipv4_seen = 0, ipv6_seen = 0;
	int i, rc = 0, ipb_num;

	ipb_num = sk_ROAIPAddressFamily_num(ipaddrblocks);
	if (ipb_num != 1 && ipb_num != 2) {
		warnx("%s: unexpected ipAddrBlocks count for AS %d", fn, asid);
		goto out;
	}

	for (i = 0; i < ipb_num; i++) {
		ripaf = sk_ROAIPAddressFamily_value(ipaddrblocks, i);

		if (!ip_addr_afi_parse(fn, ripaf->addressFamily, &afi)) {
			warnx("%s: invalid afi for AS %d", fn, asid);
			goto out;
		}

		switch (afi) {
		case AFI_IPV4:
			if (ipv6_seen > 0) {
				warnx("%s: misordered IPv4 addressFamily for AS"
				    " %d", fn, asid);
				goto out;
			}
			if (ipv4_seen++ > 0) {
				warnx("%s: IPv4 addressFamily duplicate for AS"
				    " %d", fn, asid);
				goto out;
			}
			break;
		case AFI_IPV6:
			if (ipv6_seen++ > 0) {
				warnx("%s: IPv6 addressFamily duplicate for AS"
				    " %d", fn, asid);
				goto out;
			}
			break;
		}

		if (!parse_roa_addresses(fn, ccr, asid, afi, ripaf->addresses))
			goto out;
	}

	rc = 1;
 out:
	return rc;
}

static int
parse_roa_payloads(const char *fn, struct ccr *ccr,
    const STACK_OF(ROAPayloadSet) *rps)
{
	ROAPayloadSet *rp;
	int i, rc = 0, rps_num;

	rps_num = sk_ROAPayloadSet_num(rps);

	RB_INIT(&ccr->vrps);

	for (i = 0; i < rps_num; i++) {
		int asid;

		rp = sk_ROAPayloadSet_value(rps, i);

		if (!as_id_parse(rp->asID, &asid)) {
			warnx("%s: malformed asID in ROAPayloadSet", fn);
			goto out;
		}

		if (!parse_roa_ipaddrb(fn, ccr, asid, rp->ipAddrBlocks))
			goto out;
	}

	rc = 1;
 out:
	return rc;
}

static int
parse_roastate(const char *fn, struct ccr *ccr, const ROAPayloadState *state)
{
	int rc = 0;

	ccr->vrps_hash = validate_asn1_hash(fn, "ROAPayloadState", state->hash,
	    ASN1_ITEM_rptr(ROAPayloadSets), state->rps);
	if (ccr->vrps_hash == NULL)
		goto out;

	if (!parse_roa_payloads(fn, ccr, state->rps))
		goto out;

	rc = 1;
 out:
	return rc;
}

static int
parse_aspa_providers(const char *fn, struct ccr *ccr, int asid,
    STACK_OF(ASN1_INTEGER) *providers)
{
	struct vap *vap = NULL;
	ASN1_INTEGER *aint;
	uint32_t prev = 0, provider;
	int i, p_num, rc = 0;

	if ((p_num = sk_ASN1_INTEGER_num(providers)) <= 0) {
		warnx("%s: AS %d ASPAPayloadSet providers missing", fn, asid);
		goto out;
	}

	if ((vap = calloc(1, sizeof(*vap))) == NULL)
		err(1, NULL);

	vap->custasid = asid;
	vap->num_providers = p_num;

	if ((vap->providers = calloc(p_num, sizeof(vap->providers[0]))) == NULL)
		err(1, NULL);

	for (i = 0; i < p_num; i++) {
		aint = sk_ASN1_INTEGER_value(providers, i);

		if (!as_id_parse(aint, &provider)) {
			warnx("%s: AS %d malformed ASPA provider", fn, asid);
			goto out;
		}

		if (i > 0) {
			if (provider <= prev) {
				warnx("%s: AS %d misordered providers", fn,
				    asid);
				goto out;
			}
		}

		vap->providers[i] = provider;
		prev = provider;
	}

	if ((RB_INSERT(vap_tree, &ccr->vaps, vap)) != NULL) {
		warnx("%s: duplicate ASPAPayloadSet", fn);
		goto out;
	}
	vap = NULL;

	rc = 1;
 out:
	vap_free(vap);
	return rc;
}

static int
parse_aspa_payloads(const char *fn, struct ccr *ccr,
    const STACK_OF(ASPAPayloadSet) *aps)
{
	ASPAPayloadSet *a;
	uint32_t asid, prev = 0;
	int i, rc = 0, aps_num;

	aps_num = sk_ASPAPayloadSet_num(aps);

	RB_INIT(&ccr->vaps);

	for (i = 0; i < aps_num; i++) {
		a = sk_ASPAPayloadSet_value(aps, i);

		if (!as_id_parse(a->asID, &asid)) {
			warnx("%s: malformed asID in ASPAPayloadSet", fn);
			goto out;
		}

		if (i > 0) {
			if (asid <= prev) {
				warnx("%s: ASPAPayloadState misordered", fn);
				goto out;
			}
		}

		if (!parse_aspa_providers(fn, ccr, asid, a->providers))
			goto out;

		prev = asid;
	}

	rc = 1;
 out:
	return rc;
}

static int
parse_aspastate(const char *fn, struct ccr *ccr, const ASPAPayloadState *state)
{
	int rc = 0;

	ccr->vaps_hash = validate_asn1_hash(fn, "ASPAPayloadState", state->hash,
	    ASN1_ITEM_rptr(ASPAPayloadSets), state->aps);
	if (ccr->vaps_hash == NULL)
		goto out;

	if (!parse_aspa_payloads(fn, ccr, state->aps))
		goto out;

	rc = 1;
 out:
	return rc;
}

static int
parse_tas_skis(const char *fn, struct ccr *ccr,
    const STACK_OF(SubjectKeyIdentifier) *skis)
{
	SubjectKeyIdentifier *ski;
	struct ccr_tas_ski *cts = NULL, *prev;
	int i, rc = 0, skis_num;

	if ((skis_num = sk_SubjectKeyIdentifier_num(skis)) <= 0) {
		warnx("%s: missing TrustAnchorState SubjectKeyIdentifier", fn);
		goto out;
	}

	RB_INIT(&ccr->tas);

	prev = NULL;
	for (i = 0; i < skis_num; i++) {
		if ((cts = calloc(1, sizeof(*cts))) == NULL)
			err(1, NULL);

		ski = sk_SubjectKeyIdentifier_value(skis, i);

		if (!copy_asn1_string(ski, cts->keyid, sizeof(cts->keyid))) {
			warnx("%s: TAS SKI #%d corrupted", fn, i);
			goto out;
		}

		if (prev != NULL) {
			if (ccr_tas_ski_cmp(cts, prev) <= 0) {
				warnx("%s: misordered TrustAnchorState", fn);
				goto out;
			}
		}

		if (RB_INSERT(ccr_tas_tree, &ccr->tas, cts) != NULL) {
			warnx("%s: trust anchor state corrupted", fn);
			goto out;
		}

		prev = cts;
		cts = NULL;
	}

	rc = 1;
 out:
	free(cts);
	return rc;
}

static int
parse_tastate(const char *fn, struct ccr *ccr, const TrustAnchorState *state)
{
	int rc = 0;

	ccr->tas_hash = validate_asn1_hash(fn, "TrustAnchorState", state->hash,
	    ASN1_ITEM_rptr(SubjectKeyIdentifiers), state->skis);
	if (ccr->tas_hash == NULL)
		goto out;

	if (!parse_tas_skis(fn, ccr, state->skis))
		goto out;

	rc = 1;
 out:
	return rc;
}

static int
parse_routerkeys(const char *fn, struct ccr *ccr, uint32_t asid,
    STACK_OF(RouterKey) *routerkeys)
{
	RouterKey *rk;
	struct brk *brk = NULL, *prev;
	int i, rk_num, rc = 0;

	if ((rk_num = sk_RouterKey_num(routerkeys)) <= 0) {
		warnx("%s: missing RouterKey", fn);
		goto out;
	}

	prev = NULL;
	for (i = 0; i < rk_num; i++) {
		unsigned char *der;
		size_t der_len;

		if ((brk = calloc(1, sizeof(*brk))) == NULL)
			err(1, NULL);

		brk->asid = asid;

		rk = sk_RouterKey_value(routerkeys, i);

		if (ASN1_STRING_length(rk->ski) != SHA_DIGEST_LENGTH) {
			warnx("%s: AS%d RouterKey SKI corrupted", fn, asid);
			goto out;
		}

		brk->ski = hex_encode_asn1_string(rk->ski);

		der = NULL;
		if ((der_len = i2d_X509_PUBKEY(rk->spki, &der)) <= 0) {
			warnx("%s: AS%d RouterKey SPKI corrupted", fn, asid);
			goto out;
		}

		if (base64_encode(der, der_len, &brk->pubkey) == -1)
			errx(1, "base64_encode");

		free(der);

		if (prev != NULL) {
			if (strcmp(brk->ski, prev->ski) <= 0) {
				warnx("%s: misordered RouterKey", fn);
				goto out;
			}
		}

		if ((RB_INSERT(brk_tree, &ccr->brks, brk)) != NULL) {
			warnx("%s; duplicate RouterKey", fn);
			goto out;
		}

		prev = brk;
		brk = NULL;
	}

	rc = 1;
 out:
	brk_free(brk);
	return rc;
}

static int
parse_rksets(const char *fn, struct ccr *ccr, STACK_OF(RouterKeySet) *rksets)
{
	RouterKeySet *rkset;
	uint32_t asid, prev = 0;
	int i, rc = 0, rksets_num;

	rksets_num = sk_RouterKeySet_num(rksets);

	RB_INIT(&ccr->brks);

	for (i = 0; i < rksets_num; i++) {
		rkset = sk_RouterKeySet_value(rksets, i);

		if (!as_id_parse(rkset->asID, &asid)) {
			warnx("%s: malformed asID in RouterKeySet", fn);
			goto out;
		}

		if (i > 0) {
			if (asid <= prev) {
				warnx("%s: AS %d misordered routerkeyset", fn,
				    asid);
				goto out;
			}
		}

		if (!parse_routerkeys(fn, ccr, asid, rkset->routerKeys))
			goto out;

		prev = asid;
	}

	rc = 1;
 out:
	return rc;
}

static int
parse_rkstate(const char *fn, struct ccr *ccr, const RouterKeyState *state)
{
	int rc = 0;

	ccr->brks_hash = validate_asn1_hash(fn, "RouterKeyState", state->hash,
	    ASN1_ITEM_rptr(RouterKeySets), state->rksets);
	if (ccr->brks_hash == NULL)
		goto out;

	if (!parse_rksets(fn, ccr, state->rksets))
		goto out;

	rc = 1;
 out:
	return rc;
}

struct ccr *
ccr_parse(const char *fn, const unsigned char *der, size_t len)
{
	const unsigned char *oder;
	ContentInfo *ci = NULL;
	CanonicalCacheRepresentation *ccr_asn1 = NULL;
	const ASN1_OBJECT *oid;
	struct ccr *ccr = NULL;
	int nid, ptype, rc = 0;

	if (der == NULL)
		return NULL;

	oder = der;
	if ((ci = d2i_ContentInfo(NULL, &der, len)) == NULL) {
		warnx("%s: d2i_ContentInfo", fn);
		goto out;
	}
	if (der != oder + len) {
		warnx("%s: %td bytes trailing garbage", fn, oder + len - der);
		goto out;
	}

	if (OBJ_cmp(ci->contentType, ccr_oid) != 0) {
		char buf[128];

		OBJ_obj2txt(buf, sizeof(buf), ci->contentType, 1);
		warnx("%s: unexpected OID: got %s, want 1.3.6.1.4.1.41948.828",
		    fn, buf);
		goto out;
	}

	ccr_asn1 = ci->content;

	if (!valid_econtent_version(fn, ccr_asn1->version, 0))
		goto out;

	X509_ALGOR_get0(&oid, &ptype, NULL, ccr_asn1->hashAlg);
	if ((nid = OBJ_obj2nid(oid)) != NID_sha256 || ptype != V_ASN1_UNDEF) {
		warnx("%s: hashAlg: want SHA256 object without parameters "
		    "have %s with parameter type %d", fn, nid2str(nid), ptype);
		goto out;
	}

	if ((ccr = calloc(1, sizeof(*ccr))) == NULL)
		err(1, NULL);

	if (!x509_get_generalized_time(fn, "CCR producedAt",
	    ccr_asn1->producedAt, &ccr->producedat))
		goto out;

	if (ccr_asn1->mfts == NULL && ccr_asn1->vrps == NULL &&
	    ccr_asn1->vaps == NULL && ccr_asn1->tas == NULL &&
	    ccr_asn1->rks == NULL) {
		warnx("%s: must contain at least one state component", fn);
		goto out;
	}

	if (ccr_asn1->mfts != NULL) {
		if (!parse_manifeststate(fn, ccr, ccr_asn1->mfts))
			goto out;
	}

	if (ccr_asn1->vrps != NULL) {
		if (!parse_roastate(fn, ccr, ccr_asn1->vrps))
			goto out;
	}

	if (ccr_asn1->vaps != NULL) {
		if (!parse_aspastate(fn, ccr, ccr_asn1->vaps))
			goto out;
	}

	if (ccr_asn1->tas != NULL) {
		if (!parse_tastate(fn, ccr, ccr_asn1->tas))
			goto out;
	}

	if (ccr_asn1->rks != NULL) {
		if (!parse_rkstate(fn, ccr, ccr_asn1->rks))
			goto out;
	}

	rc = 1;
 out:
	ContentInfo_free(ci);

	if (rc == 0) {
		ccr_free(ccr);
		ccr = NULL;
	}

	return ccr;
}
