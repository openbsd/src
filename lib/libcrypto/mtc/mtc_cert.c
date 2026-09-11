/*	$OpenBSD$ */
/*
 * Copyright (c) 2026, Bob Beck <beck@obtuse.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Parsing of the MTCProof of section 6.2 of
 * draft-ietf-plants-merkle-tree-certs-05.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bytestring.h"
#include "mtc_internal.h"

static int
cbs_get_u48(CBS *cbs, uint64_t *out)
{
	uint32_t lo;
	uint16_t hi;

	if (!CBS_get_u16(cbs, &hi))
		return 0;
	if (!CBS_get_u32(cbs, &lo))
		return 0;

	*out = ((uint64_t)hi << 32) | lo;

	return 1;
}

/*
 * cosigner_ids order by length, then bytewise, per section 6.2 of
 * draft-ietf-plants-merkle-tree-certs-05.
 */
static int
cosigner_id_less(const CBS *a, const CBS *b)
{
	if (CBS_len(a) != CBS_len(b))
		return CBS_len(a) < CBS_len(b);

	return memcmp(CBS_data(a), CBS_data(b), CBS_len(a)) < 0;
}

int
mtc_proof_get_cosignatures(const struct mtc_proof *proof,
    struct mtc_cosignature *out, size_t max, size_t *count)
{
	struct mtc_cosignature cosig, prev;
	CBS sigs;
	size_t n = 0;

	CBS_dup(&proof->signatures, &sigs);
	CBS_init(&prev.cosigner_id, NULL, 0);

	while (CBS_len(&sigs) > 0) {
		/*
		 * MTCSignature, section 6.2 of
		 * draft-ietf-plants-merkle-tree-certs-05.
		 */
		if (!CBS_get_u8_length_prefixed(&sigs, &cosig.cosigner_id))
			return 0;
		if (CBS_len(&cosig.cosigner_id) == 0)
			return 0;
		if (!CBS_get_u16_length_prefixed(&sigs, &cosig.signature))
			return 0;

		if (n > 0 && !cosigner_id_less(&prev.cosigner_id,
		    &cosig.cosigner_id))
			return 0;

		if (out != NULL) {
			if (n >= max)
				return 0;
			out[n] = cosig;
		}
		prev = cosig;
		n++;
	}

	*count = n;

	return 1;
}

int
mtc_proof_parse(const uint8_t *in, size_t in_len, struct mtc_proof *proof)
{
	struct mtc_proof parsed;
	size_t count;
	CBS cbs;

	CBS_init(&cbs, in, in_len);

	/* MTCProof, section 6.2 of draft-ietf-plants-merkle-tree-certs-05. */
	if (!CBS_get_u16_length_prefixed(&cbs, &parsed.extensions))
		return 0;
	if (!cbs_get_u48(&cbs, &parsed.start))
		return 0;
	if (!cbs_get_u48(&cbs, &parsed.end))
		return 0;
	if (!CBS_get_u16_length_prefixed(&cbs, &parsed.inclusion_proof))
		return 0;
	if (!CBS_get_u16_length_prefixed(&cbs, &parsed.signatures))
		return 0;
	if (CBS_len(&cbs) != 0)
		return 0;

	if (parsed.start >= parsed.end)
		return 0;

	if (!mtc_proof_get_cosignatures(&parsed, NULL, 0, &count))
		return 0;

	*proof = parsed;

	return 1;
}
