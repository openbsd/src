/*	$OpenBSD: ghcb.c,v 1.1 2025/05/24 12:47:00 bluhm Exp $	*/

/*
 * Copyright (c) 2024, 2025 Hans-Joerg Hoexer <hshoexer@genua.de>
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

#include <sys/types.h>
#include <sys/systm.h>

#include <machine/frame.h>
#include <machine/ghcb.h>

/* Mask for adjusting GPR sizes. */
const uint64_t ghcb_sz_masks[] = {
    0x00000000000000ffULL, 0x000000000000ffffULL,
    0x00000000ffffffffULL, 0xffffffffffffffffULL
};

/*
 * ghcb_clear
 *
 * Clear GHCB by setting to all 0.
 * Used by host and guest.
 */
void
ghcb_clear(struct ghcb_sa *ghcb)
{
	memset(ghcb, 0, sizeof(*ghcb));
}

/*
 * ghcb_valbm_set
 *
 * Set the quad word position of qword in the GHCB valid bitmap.
 * Used by host and guest.
 */
int
ghcb_valbm_set(uint8_t *bm, int qword)
{
	if (qword > GHCB_MAX)
		return (1);

	bm[GHCB_IDX(qword)] |= (1 << GHCB_BIT(qword));

	return (0);
}

/*
 * ghcb_valbm_isset
 *
 * Indicate wether a specific quad word is set or not.
 * Used by host and guest.
 */
int
ghcb_valbm_isset(uint8_t *bm, int qword)
{
	if (qword > GHCB_MAX)
		return (0);

	return (bm[GHCB_IDX(qword)] & (1 << GHCB_BIT(qword)));
}

/*
 * ghcb_valid
 *
 * To provide valid information, the exitcode, exitinfo1 and exitinfo2
 * must be set in the GHCB.  Verify by checking valid_bitmap.
 * Used by host only.
 */
int
ghcb_valid(struct ghcb_sa *ghcb)
{
	uint8_t	*bm = ghcb->valid_bitmap;

	return (ghcb_valbm_isset(bm, GHCB_SW_EXITCODE) &&
	    ghcb_valbm_isset(bm, GHCB_SW_EXITINFO1) &&
	    ghcb_valbm_isset(bm, GHCB_SW_EXITINFO2));
}

/*
 * ghcb_verify_bm
 *
 * To be verified positive, the given expected bitmap must be at
 * least a subset of the provided valid bitmap.
 * Used by host and guest.
 */
int
ghcb_verify_bm(uint8_t *valid_bm, uint8_t *expected_bm)
{
	return ((ghcb_valbm_isset(expected_bm, GHCB_RAX) &&
	    !ghcb_valbm_isset(valid_bm, GHCB_RAX)) ||
	    (ghcb_valbm_isset(expected_bm, GHCB_RBX) &&
	    !ghcb_valbm_isset(valid_bm, GHCB_RBX)) ||
	    (ghcb_valbm_isset(expected_bm, GHCB_RCX) &&
	    !ghcb_valbm_isset(valid_bm, GHCB_RCX)) ||
	    (ghcb_valbm_isset(expected_bm, GHCB_RDX) &&
	    !ghcb_valbm_isset(valid_bm, GHCB_RDX)) ||
	    (ghcb_valbm_isset(expected_bm, GHCB_SW_EXITCODE) &&
	    !ghcb_valbm_isset(valid_bm, GHCB_SW_EXITCODE)) ||
	    (ghcb_valbm_isset(expected_bm, GHCB_SW_EXITINFO1) &&
	    !ghcb_valbm_isset(valid_bm, GHCB_SW_EXITINFO1)) ||
	    (ghcb_valbm_isset(expected_bm, GHCB_SW_EXITINFO2) &&
	    !ghcb_valbm_isset(valid_bm, GHCB_SW_EXITINFO2)) ||
	    (ghcb_valbm_isset(expected_bm, GHCB_SW_SCRATCH) &&
	    !ghcb_valbm_isset(valid_bm, GHCB_SW_SCRATCH)) ||
	    (ghcb_valbm_isset(expected_bm, GHCB_XCR0) &&
	    !ghcb_valbm_isset(valid_bm, GHCB_XCR0)) ||
	    (ghcb_valbm_isset(expected_bm, GHCB_XSS) &&
	    !ghcb_valbm_isset(valid_bm, GHCB_XSS)));
}

/*
 * ghcb_sync_val
 *
 * Record a value for synchronization to GHCB in the valid bitmap.
 * For GPRs A to D also record size.
 */
void
ghcb_sync_val(int type, int size, struct ghcb_sync *gs)
{
	if (size > GHCB_SZ64)
		panic("invalide size: %d", size);

	switch (type) {
	case GHCB_RAX:
		gs->sz_a = size;
		break;
	case GHCB_RBX:
		gs->sz_b = size;
		break;
	case GHCB_RCX:
		gs->sz_c = size;
		break;
	case GHCB_RDX:
		gs->sz_d = size;
		break;
	case GHCB_SW_EXITCODE:
	case GHCB_SW_EXITINFO1:
	case GHCB_SW_EXITINFO2:
		break;

	default:
		panic("invalid type: %d", type);
		/* NOTREACHED */
	}

	ghcb_valbm_set(gs->valid_bitmap, type);
}

/*
 * ghcb_sync_out
 *
 * Copy values provided in trap frame (GPRs) and additional arguments
 * according to valid bitmap to GHCB.  For GPRs respect given size.
 * Used by guest only.
 */
void
ghcb_sync_out(struct trapframe *frame, uint64_t exitcode, uint64_t exitinfo1,
    uint64_t exitinfo2, struct ghcb_sa *ghcb, struct ghcb_sync *gsout)
{
	ghcb_clear(ghcb);

	memcpy(ghcb->valid_bitmap, gsout->valid_bitmap,
	    sizeof(ghcb->valid_bitmap));

	if (ghcb_valbm_isset(gsout->valid_bitmap, GHCB_RAX))
		ghcb->v_rax = frame->tf_rax & ghcb_sz_masks[gsout->sz_a];
	if (ghcb_valbm_isset(gsout->valid_bitmap, GHCB_RBX))
		ghcb->v_rbx = frame->tf_rbx & ghcb_sz_masks[gsout->sz_b];
	if (ghcb_valbm_isset(gsout->valid_bitmap, GHCB_RCX))
		ghcb->v_rcx = frame->tf_rcx & ghcb_sz_masks[gsout->sz_c];
	if (ghcb_valbm_isset(gsout->valid_bitmap, GHCB_RDX))
		ghcb->v_rdx = frame->tf_rdx & ghcb_sz_masks[gsout->sz_d];

	if (ghcb_valbm_isset(gsout->valid_bitmap, GHCB_SW_EXITCODE))
		ghcb->v_sw_exitcode = exitcode;
	if (ghcb_valbm_isset(gsout->valid_bitmap, GHCB_SW_EXITINFO1))
		ghcb->v_sw_exitinfo1 = exitinfo1;
	if (ghcb_valbm_isset(gsout->valid_bitmap, GHCB_SW_EXITINFO2))
		ghcb->v_sw_exitinfo2 = exitinfo2;
}

/*
 * ghcb_sync_in
 *
 * Copy GPRs back to stack frame.  Respect provided GPR size.
 * Used by guest only.
 */
void
ghcb_sync_in(struct trapframe *frame, struct ghcb_sa *ghcb,
    struct ghcb_sync *gsin)
{
	if (ghcb_valbm_isset(gsin->valid_bitmap, GHCB_RAX)) {
		frame->tf_rax &= ~ghcb_sz_masks[gsin->sz_a];
		frame->tf_rax |= (ghcb->v_rax & ghcb_sz_masks[gsin->sz_a]);
	}
	if (ghcb_valbm_isset(gsin->valid_bitmap, GHCB_RBX)) {
		frame->tf_rbx &= ~ghcb_sz_masks[gsin->sz_b];
		frame->tf_rbx |= (ghcb->v_rbx & ghcb_sz_masks[gsin->sz_b]);
	}
	if (ghcb_valbm_isset(gsin->valid_bitmap, GHCB_RCX)) {
		frame->tf_rcx &= ~ghcb_sz_masks[gsin->sz_c];
		frame->tf_rcx |= (ghcb->v_rcx & ghcb_sz_masks[gsin->sz_c]);
	}
	if (ghcb_valbm_isset(gsin->valid_bitmap, GHCB_RDX)) {
		frame->tf_rdx &= ~ghcb_sz_masks[gsin->sz_d];
		frame->tf_rdx |= (ghcb->v_rdx & ghcb_sz_masks[gsin->sz_d]);
	}

	ghcb_clear(ghcb);
}
