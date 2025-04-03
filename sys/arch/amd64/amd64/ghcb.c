/*	$OpenBSD: ghcb.c,v 1.5 2025/08/27 06:11:12 sf Exp $	*/

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
#include <sys/param.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/ghcb.h>
#include <machine/vmmvar.h>

/* Masks for adjusting GPR sizes. */
const uint64_t ghcb_sz_masks[] = {
    0x00000000000000ffULL, 0x000000000000ffffULL,
    0x00000000ffffffffULL, 0xffffffffffffffffULL
};

/*
 * In 64-bit mode, when performing 32-bit operations with a GPR
 * destination, the 32-bit value gets zero extended to the full
 * 64-bit destination size.  This means, unlike 8-bit or 16-bit
 * values, the upper 32 bits of the destination register are not
 * retained for 32-bit values.
 *
 * Therefore, when syncing values back to the stack frame, use a
 * 64-bit "all 1" mask for 32-bit values.
 */
const uint64_t ghcb_sz_clear_masks[] = {
    0x00000000000000ffULL, 0x000000000000ffffULL,
    0xffffffffffffffffULL, 0xffffffffffffffffULL
};

vaddr_t ghcb_vaddr;
paddr_t ghcb_paddr;

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
 * Indicate whether a specific quad word is set or not.
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
		panic("invalid size: %d", size);

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
	case GHCB_XSS:
	case GHCB_SW_EXITCODE:
	case GHCB_SW_EXITINFO1:
	case GHCB_SW_EXITINFO2:
	case GHCB_SW_SCRATCH:
	case GHCB_XCR0:
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
ghcb_sync_out(struct trapframe *frame, const struct ghcb_extra_regs *regs,
    struct ghcb_sa *ghcb, struct ghcb_sync *gsout)
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

	if (ghcb_valbm_isset(gsout->valid_bitmap, GHCB_XSS)) {
		if (rcr4() & CR4_OSXSAVE)
			ghcb->v_xss = rdmsr(MSR_XSS);
	}
	if (ghcb_valbm_isset(gsout->valid_bitmap, GHCB_XCR0)) {
		if (rcr4() & CR4_OSXSAVE)
			ghcb->v_xcr0 = xgetbv(0);
		else
			ghcb->v_xcr0 = 1;
	}

	if (ghcb_valbm_isset(gsout->valid_bitmap, GHCB_SW_EXITCODE))
		ghcb->v_sw_exitcode = regs->exitcode;
	if (ghcb_valbm_isset(gsout->valid_bitmap, GHCB_SW_EXITINFO1))
		ghcb->v_sw_exitinfo1 = regs->exitinfo1;
	if (ghcb_valbm_isset(gsout->valid_bitmap, GHCB_SW_EXITINFO2))
		ghcb->v_sw_exitinfo2 = regs->exitinfo2;
	if (ghcb_valbm_isset(gsout->valid_bitmap, GHCB_SW_SCRATCH)) {
		ghcb->v_sw_scratch = regs->scratch;
		*(uint64_t *)ghcb->v_sharedbuf = regs->scratch2; /* XXX mask */
	}
}

/*
 * ghcb_sync_in
 *
 * Copy GPRs back to stack frame.  Respect provided GPR size.
 * Used by guest only.
 */
void
ghcb_sync_in(struct trapframe *frame, uint64_t *val64, struct ghcb_sa *ghcb,
    struct ghcb_sync *gsin)
{
	if (ghcb_valbm_isset(gsin->valid_bitmap, GHCB_RAX)) {
		frame->tf_rax &= ~ghcb_sz_clear_masks[gsin->sz_a];
		frame->tf_rax |= (ghcb->v_rax & ghcb_sz_masks[gsin->sz_a]);
	}
	if (ghcb_valbm_isset(gsin->valid_bitmap, GHCB_RBX)) {
		frame->tf_rbx &= ~ghcb_sz_clear_masks[gsin->sz_b];
		frame->tf_rbx |= (ghcb->v_rbx & ghcb_sz_masks[gsin->sz_b]);
	}
	if (ghcb_valbm_isset(gsin->valid_bitmap, GHCB_RCX)) {
		frame->tf_rcx &= ~ghcb_sz_clear_masks[gsin->sz_c];
		frame->tf_rcx |= (ghcb->v_rcx & ghcb_sz_masks[gsin->sz_c]);
	}
	if (ghcb_valbm_isset(gsin->valid_bitmap, GHCB_RDX)) {
		frame->tf_rdx &= ~ghcb_sz_clear_masks[gsin->sz_d];
		frame->tf_rdx |= (ghcb->v_rdx & ghcb_sz_masks[gsin->sz_d]);
	}
	if (val64)	/* XXX size */
		*val64 = *(uint64_t *)ghcb->v_sharedbuf;

	ghcb_clear(ghcb);
}

/*
 * _ghcb_io_rw
 *
 * Paravirtualize IN and OUT instructions by directly calling vmgexit().
 * Allows to avoid #VC trap on IN and OUT.
 */
static int
_ghcb_io_rw(unsigned int port, int valsz, uint64_t *val, int read)
{
	struct ghcb_sync	 syncout, syncin;
	struct ghcb_sa		*ghcb;
	struct ghcb_extra_regs	 ghcb_regs;
	struct trapframe	 frame;
	unsigned long		 s, rflag;

	if (val == NULL)
		return (1);

	if (read)
		rflag = 1;
	else
		rflag = 0;

	memset(&syncout, 0, sizeof(syncout));
	memset(&syncin, 0, sizeof(syncin));
	memset(&ghcb_regs, 0, sizeof(ghcb_regs));
	memset(&frame, 0, sizeof(frame));

	switch (valsz) {
	case GHCB_SZ8:
		ghcb_regs.exitinfo1 = ((port << 16) | (1ULL << 4) |
		    (rflag << 0));
		break;
	case GHCB_SZ16:
		ghcb_regs.exitinfo1 = ((port << 16) | (1ULL << 5) |
		    (rflag << 0));
		break;
	case GHCB_SZ32:
		ghcb_regs.exitinfo1 = ((port << 16) | (1ULL << 6) |
		    (rflag << 0));
		break;
	default:
		return (1);
	}

	ghcb_regs.exitcode = SVM_VMEXIT_IOIO;

	if (!read) {
		frame.tf_rax = *val;
		ghcb_sync_val(GHCB_RAX, valsz, &syncout);
	} else
		ghcb_sync_val(GHCB_RAX, valsz, &syncin);

	ghcb_sync_val(GHCB_SW_EXITCODE, GHCB_SZ64, &syncout);
	ghcb_sync_val(GHCB_SW_EXITINFO1, GHCB_SZ64, &syncout);
	ghcb_sync_val(GHCB_SW_EXITINFO2, GHCB_SZ64, &syncout);

	s = intr_disable();

	ghcb = (struct ghcb_sa *)ghcb_vaddr;
	ghcb_sync_out(&frame, &ghcb_regs, ghcb, &syncout);

	vmgexit();

	if (ghcb_verify_bm(ghcb->valid_bitmap, syncin.valid_bitmap)) {
		ghcb_clear(ghcb);
		panic("invalid hypervisor response");
	}

	if (read)
		ghcb_sync_in(&frame, NULL, ghcb, &syncin);

	intr_restore(s);

	if (read && val)
		*val = frame.tf_rax;

	return (0);
}

uint8_t
ghcb_io_read_1(unsigned int port)
{
	uint64_t	val;

	if (_ghcb_io_rw(port, GHCB_SZ8, &val, 1))
		panic("_ghcb_io_rw() failed");

	return ((uint8_t)val);
}

uint16_t
ghcb_io_read_2(unsigned int port)
{
	uint64_t	val;

	if (_ghcb_io_rw(port, GHCB_SZ16, &val, 1))
		panic("_ghcb_iio_read() failed");

	return ((uint16_t)val);
}

uint32_t
ghcb_io_read_4(unsigned int port)
{
	uint64_t	val;

	if (_ghcb_io_rw(port, GHCB_SZ32, &val, 1))
		panic("_ghcb_io_rw() failed");

	return ((uint32_t)val);
}

void
ghcb_io_write_1(unsigned int port, uint8_t v)
{
	uint64_t val = (uint64_t)v;

	if (_ghcb_io_rw(port, GHCB_SZ8, &val, 0))
		panic("_ghcb_io_rw() failed");
}

void
ghcb_io_write_2(unsigned int port, uint16_t v)
{
	uint64_t val = (uint64_t)v;

	if (_ghcb_io_rw(port, GHCB_SZ16, &val, 0))
		panic("_ghcb_io_rw() failed");
}

void
ghcb_io_write_4(unsigned int port, uint32_t v)
{
	uint64_t val = (uint64_t)v;

	if (_ghcb_io_rw(port, GHCB_SZ32, &val, 0))
		panic("_ghcb_io_rw() failed");
}

/*
 * _ghcb_mem_rw
 *
 * Paravirtualize MMIO by directly calling vmgexit().  Allows to
 * avoid #VC trap on MMIO.
 */
static int
_ghcb_mem_rw(uint64_t addr, int valsz, uint64_t *val, int read)
{
	uint64_t		 value;
	size_t			 size;
	paddr_t			 paddr;
	struct ghcb_sync	 syncout, syncin;
	struct ghcb_sa		*ghcb;
	struct ghcb_extra_regs	 ghcb_regs;
	struct trapframe	 frame;	/* XXX not needed */
	unsigned long		 s;

	if (val == NULL)
		return (1);

	memset(&syncout, 0, sizeof(syncout));
	memset(&syncin, 0, sizeof(syncin));
	memset(&ghcb_regs, 0, sizeof(ghcb_regs));
	memset(&frame, 0, sizeof(frame));

	switch (valsz) {
	case GHCB_SZ8:
		size = sizeof(uint8_t);
		break;
	case GHCB_SZ16:
		size = sizeof(uint16_t);
		break;
	case GHCB_SZ32:
		size = sizeof(uint32_t);
		break;
	case GHCB_SZ64:
		size = sizeof(uint64_t);
		break;
	default:
		return (1);
	}

	if (!pmap_extract(pmap_kernel(), addr, &paddr))
		return (1);

	if (read) {
		ghcb_regs.exitcode = SEV_VMGEXIT_MMIO_READ;
		ghcb_regs.exitinfo1 = paddr;
		ghcb_regs.exitinfo2 = size;
		ghcb_regs.scratch = ghcb_paddr + offsetof(struct ghcb_sa,
		    v_sharedbuf);
	} else {
		ghcb_regs.exitcode = SEV_VMGEXIT_MMIO_WRITE;
		ghcb_regs.exitinfo1 = paddr;
		ghcb_regs.exitinfo2 = size;
		ghcb_regs.scratch = ghcb_paddr + offsetof(struct ghcb_sa,
		    v_sharedbuf);
		ghcb_regs.scratch2 = *val;	/* XXX mask */
	}

	ghcb_sync_val(GHCB_SW_EXITCODE, GHCB_SZ64, &syncout);
	ghcb_sync_val(GHCB_SW_EXITINFO1, GHCB_SZ64, &syncout);
	ghcb_sync_val(GHCB_SW_EXITINFO2, GHCB_SZ64, &syncout);
	ghcb_sync_val(GHCB_SW_SCRATCH, GHCB_SZ64, &syncout);

	s = intr_disable();

	ghcb = (struct ghcb_sa *)ghcb_vaddr;
	ghcb_sync_out(&frame, &ghcb_regs, ghcb, &syncout);

	vmgexit();

	if (ghcb_verify_bm(ghcb->valid_bitmap, syncin.valid_bitmap)) {
		ghcb_clear(ghcb);
		panic("invalid hypervisor response");
	}

	if (read)
		ghcb_sync_in(&frame, &value, ghcb, &syncin);

	intr_restore(s);

	if (read && val)
		*val = value;

	return (0);
}

uint8_t
ghcb_mem_read_1(uint64_t addr)
{
	uint64_t val;

	if (_ghcb_mem_rw(addr, GHCB_SZ8, &val, 1))
		panic("_ghcb_mem_rw() failed");

	return ((uint8_t)val);
}


uint16_t
ghcb_mem_read_2(uint64_t addr)
{
	uint64_t val;

	if (_ghcb_mem_rw(addr, GHCB_SZ16, &val, 1))
		panic("_ghcb_mem_rw() failed");

	return ((uint16_t)val);
}

uint32_t
ghcb_mem_read_4(uint64_t addr)
{
	uint64_t val;

	if (_ghcb_mem_rw(addr, GHCB_SZ32, &val, 1))
		panic("_ghcb_mem_rw() failed");

	return ((uint32_t)val);
}

uint64_t
ghcb_mem_read_8(uint64_t addr)
{
	uint64_t val;

	if (_ghcb_mem_rw(addr, GHCB_SZ64, &val, 1))
		panic("_ghcb_mem_rw() failed");

	return (val);
}

void
ghcb_mem_write_1(uint64_t addr, uint8_t v)
{
	uint64_t val = (uint64_t)v;

	if (_ghcb_mem_rw(addr, GHCB_SZ8, &val, 0))
		panic("_ghcb_mem_rw() failed");
}


void
ghcb_mem_write_2(uint64_t addr, uint16_t v)
{
	uint64_t val = (uint64_t)v;

	if (_ghcb_mem_rw(addr, GHCB_SZ16, &val, 0))
		panic("_ghcb_mem_rw() failed");
}

void
ghcb_mem_write_4(uint64_t addr, uint32_t v)
{
	uint64_t val = (uint64_t)v;

	if (_ghcb_mem_rw(addr, GHCB_SZ32, &val, 0))
		panic("_ghcb_mem_rw() failed");
}

void
ghcb_mem_write_8(uint64_t addr, uint64_t v)
{
	uint64_t val = v;

	if (_ghcb_mem_rw(addr, GHCB_SZ64, &val, 0))
		panic("_ghcb_mem_rw() failed");
}
