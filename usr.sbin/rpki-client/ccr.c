/*	$OpenBSD: ccr.c,v 1.5 2025/09/06 16:13:48 tb Exp $ */
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

#include "extern.h"
#include "rpki-asn1.h"

/*
 * RpkiCanonicalCacheRepresentation-2025
 *   { iso(1) member-body(2) us(840) rsadsi(113549)
 *     pkcs(1) pkcs9(9) smime(16) mod(0) id-mod-rpkiCCR-2025(TBD) }
 *
 * DEFINITIONS EXPLICIT TAGS ::=
 * BEGIN
 *
 * IMPORTS
 *   CONTENT-TYPE, Digest, DigestAlgorithmIdentifier, SubjectKeyIdentifier
 *   FROM CryptographicMessageSyntax-2010 -- in [RFC6268]
 *     { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1)
 *       pkcs-9(9) smime(16) modules(0) id-mod-cms-2009(58) }
 *
 * -- in [draft-spaghetti-sidrops-rpki-erik-protocol-01]
 * -- https://sobornost.net/~job/draft-spaghetti-sidrops-rpki-erik-protocol.html
 *   ManifestRef
 *   FROM RpkiErikPartition-2025
 *     { iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1)
 *       pkcs9(9) smime(16) mod(0) id-mod-rpkiErikPartition-2025(TBD) }
 *
 *   ASID, ROAIPAddressFamily
 *   FROM RPKI-ROA-2023 -- in [RFC9582]
 *     { so(1) member-body(2) us(840) rsadsi(113549) pkcs(1)
 *       pkcs9(9) smime(16) mod(0) id-mod-rpkiROA-2023(75) }
 * ;
 *
 * ct-rpkiCanonicalCacheRepresentation CONTENT-TYPE ::=
 *   { TYPE RpkiCanonicalCacheRepresentation
 *     IDENTIFIED BY id-ct-rpkiCanonicalCacheRepresentation }
 *
 * id-ct-rpkiCanonicalCacheRepresentation OBJECT IDENTIFIER ::=
 *   { iso(1) identified-organization(3) dod(6) internet(1) private(4)
 *     enterprise(1) snijders(41948) ccr(825) }
 *
 * RpkiCanonicalCacheRepresentation ::= SEQUENCE {
 *   version   [0]     INTEGER DEFAULT 0,
 *   hashAlg           DigestAlgorithmIdentifier,
 *   producedAt        GeneralizedTime,
 *   mfts      [1]     ManifestState OPTIONAL,
 *   vrps      [2]     ROAPayloadState OPTIONAL,
 *   vaps      [3]     ASPAPayloadState OPTIONAL,
 *   tas       [4]     TrustAnchorState OPTIONAL,
 *   ... }
 *   -- at least one of mfts, vrps, vaps, or tas MUST be present
 *   ( WITH COMPONENTS { ..., mfts PRESENT } |
 *     WITH COMPONENTS { ..., vrps PRESENT } |
 *     WITH COMPONENTS { ..., vaps PRESENT } |
 *     WITH COMPONENTS { ..., tas PRESENT } )
 *
 * ManifestState ::= SEQUENCE {
 *   mftrefs           SEQUENCE OF ManifestRef,
 *   mostRecentUpdate  GeneralizedTime,
 *   hash              Digest }
 *
 * ROAPayloadState ::= SEQUENCE {
 *   rps               SEQUENCE OF ROAPayloadSet,
 *   hash              Digest }
 *
 * ROAPayloadSet ::= SEQUENCE {
 *   asID              ASID,
 *   ipAddrBlocks      SEQUENCE (SIZE(1..2)) OF ROAIPAddressFamily }
 *
 * ASPAPayloadState ::= SEQUENCE {
 *   aps               SEQUENCE OF ASPAPayloadSet,
 *   hash              Digest }
 *
 * ASPAPayloadSet ::= SEQUENCE {
 *   asID              ASID
 *   providers         SEQUENCE (SIZE(1..MAX)) OF ASID }
 *
 * TrustAnchorState ::= SEQUENCE {
 *   skis              SEQUENCE (SIZE(1..MAX)) OF SubjectKeyIdentifier,
 *   hash              Digest }
 *
 * END
 */

ASN1_ITEM_EXP ContentInfo_it;
ASN1_ITEM_EXP CanonicalCacheRepresentation_it;
ASN1_ITEM_EXP ManifestRefs_it;
ASN1_ITEM_EXP ManifestRef_it;
ASN1_ITEM_EXP ROAPayloadSets_it;
ASN1_ITEM_EXP ROAPayloadSet_it;
ASN1_ITEM_EXP ASPAPayloadSets_it;
ASN1_ITEM_EXP ASPAPayloadSet_it;
ASN1_ITEM_EXP SubjectKeyIdentifiers_it;
ASN1_ITEM_EXP SubjectKeyIdentifier_it;

/*
 * Can't use CMS_ContentInfo since it is not backed by a public struct
 * and since the OpenSSL CMS API does not support custom contentTypes.
 */
ASN1_SEQUENCE(ContentInfo) = {
	ASN1_SIMPLE(ContentInfo, contentType, ASN1_OBJECT),
	ASN1_EXP(ContentInfo, content, ASN1_OCTET_STRING, 0),
} ASN1_SEQUENCE_END(ContentInfo);

IMPLEMENT_ASN1_FUNCTIONS(ContentInfo);

ASN1_SEQUENCE(CanonicalCacheRepresentation) = {
	ASN1_EXP_OPT(CanonicalCacheRepresentation, version, ASN1_INTEGER, 0),
	ASN1_SIMPLE(CanonicalCacheRepresentation, hashAlg, ASN1_OBJECT),
	ASN1_SIMPLE(CanonicalCacheRepresentation, producedAt,
	    ASN1_GENERALIZEDTIME),
	ASN1_EXP_OPT(CanonicalCacheRepresentation, mfts, ManifestState, 1),
	ASN1_EXP_OPT(CanonicalCacheRepresentation, vrps, ROAPayloadState, 2),
	ASN1_EXP_OPT(CanonicalCacheRepresentation, vaps, ASPAPayloadState, 3),
	ASN1_EXP_OPT(CanonicalCacheRepresentation, tas, TrustAnchorState, 4),
} ASN1_SEQUENCE_END(CanonicalCacheRepresentation);

IMPLEMENT_ASN1_FUNCTIONS(CanonicalCacheRepresentation);

ASN1_SEQUENCE(ManifestState) = {
	ASN1_SEQUENCE_OF(ManifestState, mftrefs, ManifestRef),
	ASN1_SIMPLE(ManifestState, mostRecentUpdate, ASN1_GENERALIZEDTIME),
	ASN1_SIMPLE(ManifestState, hash, ASN1_OCTET_STRING),
} ASN1_SEQUENCE_END(ManifestState);

IMPLEMENT_ASN1_FUNCTIONS(ManifestState);

ASN1_ITEM_TEMPLATE(ManifestRefs) =
    ASN1_EX_TEMPLATE_TYPE(ASN1_TFLG_SEQUENCE_OF, 0, mftrefs, ManifestRef)
ASN1_ITEM_TEMPLATE_END(ManifestRefs);

IMPLEMENT_ASN1_ENCODE_FUNCTIONS_fname(ManifestRefs, ManifestRefs, ManifestRefs);

ASN1_SEQUENCE(ManifestRef) = {
	ASN1_SIMPLE(ManifestRef, hash, ASN1_OCTET_STRING),
	ASN1_SIMPLE(ManifestRef, size, ASN1_INTEGER),
	ASN1_SIMPLE(ManifestRef, aki, ASN1_OCTET_STRING),
	ASN1_SIMPLE(ManifestRef, manifestNumber, ASN1_INTEGER),
	ASN1_SEQUENCE_OF(ManifestRef, location, ACCESS_DESCRIPTION),
} ASN1_SEQUENCE_END(ManifestRef);

IMPLEMENT_ASN1_FUNCTIONS(ManifestRef);

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
	if ((ad->method = OBJ_nid2obj(NID_signedObject)) == NULL)
		errx(1, "OBJ_nid2obj");

	GENERAL_NAME_free(ad->location);
	ad->location = a2i_GENERAL_NAME(NULL, NULL, NULL, GEN_URI, sia, 0);
	if (ad->location == NULL)
		errx(1, "a2i_GENERAL_NAME");

	if (sk_ACCESS_DESCRIPTION_push(sad, ad) <= 0)
		errx(1, "sk_ACCESS_DESCRIPTION_push");
}

static void
append_cached_manifest(STACK_OF(ManifestRef) *mftrefs, struct ccr_mft *cm)
{
	ManifestRef *mftref;

	if ((mftref = ManifestRef_new()) == NULL)
		errx(1, "ManifestRef_new");

	if (!ASN1_OCTET_STRING_set(mftref->hash, cm->hash, sizeof(cm->hash)))
		errx(1, "ASN1_OCTET_STRING_set");

	if (!ASN1_OCTET_STRING_set(mftref->aki, cm->aki, sizeof(cm->aki)))
		errx(1, "ASN1_OCTET_STRING_set");

	if (!ASN1_INTEGER_set_uint64(mftref->size, cm->size))
		errx(1, "ASN1_INTEGER_set_uint64");

	asn1int_set_seqnum(mftref->manifestNumber, cm->seqnum);

	location_add_sia(mftref->location, cm->sia);

	if (sk_ManifestRef_push(mftrefs, mftref) <= 0)
		errx(1, "sk_ManifestRef_push");
}

static void
hash_asn1_item(ASN1_OCTET_STRING *astr, const ASN1_ITEM *it, void *val)
{
	unsigned char hash[SHA256_DIGEST_LENGTH];

	if (!ASN1_item_digest(it, EVP_sha256(), val, hash, NULL))
		errx(1, "ASN1_item_digest");

	if (!ASN1_OCTET_STRING_set(astr, hash, sizeof(hash)))
		errx(1, "ASN1_STRING_set");
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
		append_cached_manifest(ms->mftrefs, cm);

		if (cm->thisupdate > most_recent_update)
			most_recent_update = cm->thisupdate;
	}

	if (ASN1_GENERALIZEDTIME_set(ms->mostRecentUpdate,
	    most_recent_update) == NULL)
		errx(1, "ASN1_GENERALIZEDTIME_set");

	hash_asn1_item(ms->hash, ASN1_ITEM_rptr(ManifestRefs), ms->mftrefs);

	if (base64_encode(ms->hash->data, ms->hash->length,
	    &ccr->mfts_hash) == -1)
		errx(1, "base64_encode");

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
	ROAPayloadState *vrps;
	struct vrp *prev, *vrp;
	ROAPayloadSet *rp;
	ROAIPAddressFamily *ripaf;
	unsigned char afibuf[2];

	if ((vrps = ROAPayloadState_new()) == NULL)
		errx(1, "ROAPayloadState_new");

	prev = NULL;
	RB_FOREACH(vrp, ccr_vrp_tree, &vd->ccr.vrps) {
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

	if (base64_encode(vrps->hash->data, vrps->hash->length,
	    &vd->ccr.vrps_hash) == -1)
		errx(1, "base64_encode");

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
	ASPAPayloadState *vaps;
	struct vap *vap;

	if ((vaps = ASPAPayloadState_new()) == NULL)
		errx(1, "ASPAPayloadState_new");

	RB_FOREACH(vap, vap_tree, &vd->vaps) {
		append_cached_aspa(vaps->aps, vap);
	}

	hash_asn1_item(vaps->hash, ASN1_ITEM_rptr(ASPAPayloadSets), vaps->aps);

	if (base64_encode(vaps->hash->data, vaps->hash->length,
	    &vd->ccr.vaps_hash) == -1)
		errx(1, "base64_encode");

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

	if (base64_encode(tas->hash->data, tas->hash->length,
	    &vd->ccr.tas_hash) == -1)
		errx(1, "base64_encode");

	return tas;
}

static CanonicalCacheRepresentation *
generate_ccr(struct validation_data *vd)
{
	CanonicalCacheRepresentation *ccr = NULL;
	time_t now = get_current_time();

	if ((ccr = CanonicalCacheRepresentation_new()) == NULL)
		errx(1, "CanonicalCacheRepresentation_new");

	ASN1_OBJECT_free(ccr->hashAlg);
	if ((ccr->hashAlg = OBJ_nid2obj(NID_sha256)) == NULL)
		errx(1, "OBJ_nid2obj");

	if (ASN1_GENERALIZEDTIME_set(ccr->producedAt, now) == NULL)
		errx(1, "ASN1_GENERALIZEDTIME_set");

	if ((ccr->mfts = generate_manifeststate(vd)) == NULL)
		errx(1, "generate_manifeststate");

	if ((ccr->vrps = generate_roapayloadstate(vd)) == NULL)
		errx(1, "generate_roapayloadstate");

	if ((ccr->vaps = generate_aspapayloadstate(vd)) == NULL)
		errx(1, "generate_aspapayloadstate");

	if ((ccr->tas = generate_trustanchorstate(vd)) == NULL)
		errx(1, "generate_trustanchorstate");

	return ccr;
}

void
serialize_ccr_content(struct validation_data *vd)
{
	CanonicalCacheRepresentation *ccr;
	ContentInfo *ci = NULL;
	unsigned char *out;
	int out_len, ci_der_len;

	if ((ci = ContentInfo_new()) == NULL)
		errx(1, "ContentInfo_new");

	/*
	 * At some point the below PEN OID should be replaced by one from IANA.
	 */
	ASN1_OBJECT_free(ci->contentType);
	if ((ci->contentType = OBJ_txt2obj("1.3.6.1.4.1.41948.825", 1)) == NULL)
		errx(1, "OBJ_txt2obj");

	ccr = generate_ccr(vd);

	out = NULL;
	if ((out_len = i2d_CanonicalCacheRepresentation(ccr, &out)) <= 0)
		err(1, "i2d_CanonicalCacheRepresentation");

	CanonicalCacheRepresentation_free(ccr);

	if (!ASN1_OCTET_STRING_set(ci->content, out, out_len))
		errx(1, "ASN1_OCTET_STRING_set");

	free(out);

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
		err(1, NULL);

	if (RB_INSERT(ccr_tas_tree, tree, cts) != NULL)
		errx(1, "CCR TAS tree corrupted");
}

static inline int
ccr_mft_cmp(const struct ccr_mft *a, const struct ccr_mft *b)
{
	return memcmp(a->hash, b->hash, SHA256_DIGEST_LENGTH);
}

RB_GENERATE(ccr_mft_tree, ccr_mft, entry, ccr_mft_cmp);

void
ccr_insert_mft(struct ccr_mft_tree *tree, const struct mft *mft)
{
	struct ccr_mft *ccr_mft;

	if ((ccr_mft = calloc(1, sizeof(*ccr_mft))) == NULL)
		err(1, NULL);

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
