/*	$OpenBSD: md.c,v 1.2 2002/07/19 19:28:12 marc Exp $	*/
/*	$NetBSD$	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Wolfgang Solfrank.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* First cut for arm32 (currently a simple copy of i386 code) */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <err.h>
#include <fcntl.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>

#include "ld.h"

/*
 * Get relocation addend corresponding to relocation record RP
 * from address ADDR
 */
long
md_get_addend(struct relocation_info *rp, unsigned char *addr)
{
	long rel;
	
	switch (rp->r_length) {
	case 0:
		rel = get_byte(addr);
		break;
	case 1:
		rel = get_short(addr);
		break;
	case 2:
		rel = get_long(addr);
		break;
	case 3:			/* looks like a special hack for b & bl */
		rel = (((long)get_long(addr) & 0xffffff) << 8) >> 6;
		rel -= rp->r_address; /* really?			XXX */
		break;
	default:
		errx(1, "Unsupported relocation size: %x",
		    rp->r_length);
	}
	return rp->r_neg ? -rel : rel; /* Hack to make r_neg work */
}

/*
 * Put RELOCATION at ADDR according to relocation record RP.
 */
static struct relocation_info *rrs_reloc; /* HACK HACK HACK			XXX */

void
md_relocate(struct relocation_info *rp, long relocation, unsigned char *addr,
	    int relocatable_output)
{
	if (rp == rrs_reloc	/* HACK HACK HACK			XXX */
	    || (RELOC_PCREL_P(rp) && relocatable_output)) {
		rrs_reloc = NULL;
		return;
	}
	rrs_reloc = NULL;
	
	if (rp->r_neg)		/* Not sure, whether this works in all cases XXX */
		relocation = -relocation;
	
	switch (rp->r_length) {
	case 0:
		put_byte(addr, relocation);
		break;
	case 1:
		put_short(addr, relocation);
		break;
	case 2:
		put_long(addr, relocation);
		break;
	case 3:
		put_long(addr,
			 (get_long(addr)&0xff000000)
			 | ((relocation&0x3ffffff) >> 2));
		break;
	default:
		errx(1, "Unsupported relocation size: %x",
		    rp->r_length);
	}
}

#ifndef	RTLD
/*
 * Machine dependent part of claim_rrs_reloc().
 * Set RRS relocation type.
 */
int
md_make_reloc(struct relocation_info *rp, struct relocation_info *r, int type)
{
	if (type == RELTYPE_EXTERN)
		rrs_reloc = rp;	/* HACK HACK HACK			XXX */
	
	/* Copy most attributes */
	r->r_pcrel = rp->r_pcrel;
	r->r_length = rp->r_length;
	r->r_neg = rp->r_neg;
	r->r_baserel = rp->r_baserel;
	r->r_jmptable = rp->r_jmptable;
	r->r_relative = rp->r_relative;

	return 0;
}
#endif	/* RTLD */

/*
 * Set up a transfer from jmpslot at OFFSET (relative to the PLT table)
 * to the binder slot (which is at offset 0 of the PLT).
 */
void
md_make_jmpslot(jmpslot_t *sp, long offset, long index)
{
	u_long	fudge = - (offset + 12);

	sp->opcode1 = SAVEPC;
	sp->opcode2 = CALL | ((fudge >> 2) & 0xffffff);
	sp->reloc_index = index;
}

/*
 * Set up a "direct" transfer (ie. not through the run-time binder) from
 * jmpslot at OFFSET to ADDR. Used by `ld' when the SYMBOLIC flag is on,
 * and by `ld.so' after resolving the symbol.
 */
void
md_fix_jmpslot(jmpslot_t *sp, long offset, u_long addr)
{
	/*
	 * Generate the following sequence:
	 *	ldr	pc, [pc]
	 *	.word	addr
	 */
	sp->opcode1 = JUMP;
	sp->reloc_index = addr;
}

/*
 * Update the relocation record for a RRS jmpslot.
 */
void
md_make_jmpreloc(struct relocation_info	*rp, struct relocation_info *r,
		 int type)
{
	r->r_address += 8;
	r->r_pcrel = 0;
	r->r_length = 2;
	r->r_neg = 0;
	r->r_baserel = 0;
	r->r_jmptable = 1;
	r->r_relative = 0;
}

/*
 * Set relocation type for a RRS GOT relocation.
 */
void
md_make_gotreloc(struct relocation_info *rp, struct relocation_info *r,
		 int type)
{
	r->r_pcrel = 0;
	r->r_length = 2;
	r->r_neg = 0;
	r->r_baserel = 1;
	r->r_jmptable = 0;
	r->r_relative = 0;
}

/*
 * Set relocation type for a RRS copy operation.
 */
void
md_make_cpyreloc(struct relocation_info *rp, struct relocation_info *r)
{
	r->r_pcrel = 0;
	r->r_length = 2;
	r->r_neg = 0;
	r->r_baserel = 0;
	r->r_jmptable = 0;
	r->r_relative = 0;
}

void
md_set_breakpoint(long where, long *savep)
{
	*savep = *(long *)where;
	*(long *)where = TRAP;
}

#ifndef RTLD

/*
 * Initialize (output) exec header such that useful values are
 * obtained from subsequent N_*() macro evaluations.
 */
void
md_init_header(struct exec *hp, int magic, int flags)
{
	N_SETMAGIC((*hp), magic, MID_ARM6, flags);

	/* TEXT_START depends on the value of outheader.a_entry.  */
	if (!(link_mode & SHAREABLE))
		hp->a_entry = PAGSIZ;
}
#endif /* RTLD */


#ifdef NEED_SWAP
/*
 * Byte swap routines for cross-linking.
 */

void
md_swapin_exec_hdr(struct exec *h)
{
	/* NetBSD: Always leave magic alone */
	int skip = 1;

	swap_longs((long *)h + skip, sizeof(*h)/sizeof(long) - skip);
}

void
md_swapout_exec_hdr(struct exec *h)
{
	/* NetBSD: Always leave magic alone */
	int skip = 1;

	swap_longs((long *)h + skip, sizeof(*h)/sizeof(long) - skip);
}


void
md_swapin_reloc(struct relocation_info *r, int n)
{
	int	bits;

	for (; n; n--, r++) {
		r->r_address = md_swap_long(r->r_address);
		bits = ((int *)r)[1];
		r->r_symbolnum = md_swap_long(bits) & 0x00ffffff;
		r->r_pcrel = (bits & 1);
		r->r_length = (bits >> 1) & 3;
		r->r_extern = (bits >> 3) & 1;
		r->r_neg = (bits >> 4) & 1;
		r->r_baserel = (bits >> 5) & 1;
		r->r_jmptable = (bits >> 6) & 1;
		r->r_relative = (bits >> 7) & 1;
	}
}

void
md_swapout_reloc(struct relocation_info *r, int n)
{
	int	bits;

	for (; n; n--, r++) {
		r->r_address = md_swap_long(r->r_address);
		bits = md_swap_long(r->r_symbolnum) & 0xffffff00;
		bits |= (r->r_pcrel & 1);
		bits |= (r->r_length & 3) << 1;
		bits |= (r->r_extern & 1) << 3;
		bits |= (r->r_neg & 1) << 4;
		bits |= (r->r_baserel & 1) << 5;
		bits |= (r->r_jmptable & 1) << 6;
		bits |= (r->r_relative & 1) << 7;
		((int *)r)[1] = bits;
	}
}

void
md_swapout_jmpslot(jmpslot_t *j, int n)
{
	for (; n; n--, j++) {
		j->opcode1 = md_swap_long(j->opcode1);
		j->opcode2 = md_swap_long(j->opcode2);
		j->reloc_index = md_swap_long(j->reloc_index);
	}
}

#endif /* NEED_SWAP */
