/* *	$OpenBSD: md.c,v 1.1 1999/02/09 05:35:14 smurph Exp $*/
/*
 * Copyright (c) 1993 Paul Kranenburg
 * All rights reserved.
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
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
md_get_addend(rp, addr)
struct relocation_info_m88k	*rp;
unsigned char		*addr;
{
	return rp->r_addend;
}

/*
 * Put RELOCATION at ADDR according to relocation record RP.
 */
void
md_relocate(rp, relocation, addr, relocatable_output)
struct relocation_info_m88k	*rp;
long			relocation;
unsigned char		*addr;
{
	if (relocatable_output) {
		/*
		 * Store relocation where the next link-edit run
		 * will look for it.
		 */
		rp->r_addend = relocation;
		return;
	}

	switch ((rp->r_type)) {
	case RELOC_LO16:
		put_short(addr,relocation & 0xffff);
		break;
	case RELOC_HI16:
		put_short(addr,(relocation >>16) & 0xffff);
		break;
	case RELOC_PC16:
		put_short(addr,(relocation)>>2);
		break;
	case RELOC_PC26:
		put_b26(addr,(relocation)>>2);
		break;
	case RELOC_32:
		put_long(addr,relocation);
		break;
	case RELOC_IW16:
		printf ("oops\n"); exit(100);
		break;
	case NO_RELOC:
		break;
	case RELOC_GLOB_DAT:
		put_long(addr,relocation);
		break;
/*
	case RELOC_JMP_SLOT:
		md_make_jmpslot(addr,,
		break;
*/
	case RELOC_RELATIVE: 
	printf("reloc_rel not supported %x\n",addr);
	exit (15);
		break;

	default:
		errx(1, "Unsupported relocation size: %x",
		     (rp->r_type));
	}
#ifdef RTLD
	_cachectl (addr, RELOC_TARGET_SIZE(rp)); /* maintain cache coherency */
#endif
}

/*
 * Machine dependent part of claim_rrs_reloc().
 * Set RRS relocation type.
 */
int
md_make_reloc(rp, r, type)
struct relocation_info_m88k	*rp, *r;
int			type;
{
	r->r_address = rp->r_address;
	r->r_type = rp->r_type;
	r->r_jmptable = rp->r_jmptable;
	r->r_pcrel = rp->r_pcrel;
	r->r_baserel = rp->r_baserel;
	r->r_extern = rp->r_extern;
	r->r_symbolnum = rp->r_symbolnum;
	r->r_addend = rp->r_addend;

	return 0;
}

/*
 * Set up a transfer from jmpslot at OFFSET (relative to the PLT table)
 * to the binder slot (which is at offset 0 of the PLT).
 */
void
md_make_jmpslot(sp, offset, index)
jmpslot_t	*sp;
long		offset;
long		index;
{
	/*
	 * On m68k machines, a long branch offset is relative to
	 * the address of the offset.
	 */
	u_long	fudge = - (sizeof(sp->opcode) + offset);

	sp->opcode = BSRL;
	sp->addr[0] = fudge >> 16;
	sp->addr[1] = fudge;
	sp->reloc_index = index;
#ifdef RTLD
	_cachectl (sp, 6);		/* maintain cache coherency */
#endif
}

/*
 * Set up a "direct" transfer (ie. not through the run-time binder) from
 * jmpslot at OFFSET to ADDR. Used by `ld' when the SYMBOLIC flag is on,
 * and by `ld.so' after resolving the symbol.
 * On the m68k, we use the BRA instruction which is PC relative, so no
 * further RRS relocations will be necessary for such a jmpslot.
 */
void
md_fix_jmpslot(sp, offset, addr)
jmpslot_t	*sp;
long		offset;
u_long		addr;
{
	u_long	fudge = addr - (sizeof(sp->opcode) + offset);

	sp->opcode = BRAL;
	sp->addr[0] = fudge >> 16;
	sp->addr[1] = fudge;
	sp->reloc_index = 0;
#ifdef RTLD
	_cachectl (sp, 6);		/* maintain cache coherency */
#endif
}

/*
 * Update the relocation record for a RRS jmpslot.
 */
void
md_make_jmpreloc(rp, r, type)
struct relocation_info_m88k	*rp, *r;
int			type;
{
	jmpslot_t	*sp;

	/*
	 * Fix relocation address to point to the correct
	 * location within this jmpslot.
	 */
	r->r_address += sizeof(sp->opcode);

#if 0
	/* Relocation size */
	r->r_length = 2;
#endif

	/* Set relocation type */
	r->r_jmptable = 1;
#if 0
	if (type & RELTYPE_RELATIVE)
		r->r_relative = 1;
#endif

printf("jmp reloc not supported \n");
exit (15);
}

/*
 * Set relocation type for a RRS GOT relocation.
 */
void
md_make_gotreloc(rp, r, type)
struct relocation_info_m88k	*rp, *r;
int			type;
{
#if 0
	r->r_baserel = 1;
	if (type & RELTYPE_RELATIVE)
		r->r_relative = 1;

	/* Relocation size */
	r->r_length = 2;
#endif
printf("got not supported \n");
exit (15);
}

/*
 * Set relocation type for a RRS copy operation.
 */
void
md_make_cpyreloc(rp, r)
struct relocation_info_m88k	*rp, *r;
{
	r->r_address = rp->r_address;
	r->r_type = rp->r_type;
	r->r_jmptable = rp->r_jmptable;
	r->r_pcrel = rp->r_pcrel;
	r->r_baserel = rp->r_baserel;
	r->r_extern = rp->r_extern;
	r->r_symbolnum = rp->r_symbolnum;
	r->r_addend = rp->r_addend;
}

void
md_set_breakpoint(where, savep)
long	where;
long	*savep;
{
	*savep = *(long *)where;
	*(short *)where = BPT;
}

#ifndef MID_M88K
#define MID_M88K 151
#endif

#ifndef RTLD
/*
 * Initialize (output) exec header such that useful values are
 * obtained from subsequent N_*() macro evaluations.
 */
void
md_init_header(hp, magic, flags)
struct exec	*hp;
int		magic, flags;
{
	if (oldmagic)
		hp->a_midmag = oldmagic;
	else
		N_SETMAGIC((*hp), magic, MID_M88K, flags);

	/* TEXT_START depends on the value of outheader.a_entry.  */
	if (!(link_mode & SHAREABLE))
		hp->a_entry = PAGSIZ;
}

/*
 * Check for acceptable foreign machine Ids
 */
int
md_midcompat(hp)
struct exec *hp;
{
	int	mid = N_GETMID(*hp);

	if (mid == MID_M88K )
		return 1;
#if 0
	return (((md_swap_long(hp->a_midmag)&0x00ff0000) >> 16) == MID_SUN020);
#endif
	return 0;
}
#endif /* RTLD */


#ifdef NEED_SWAP
/*
 * Byte swap routines for cross-linking.
 */

void
md_swapin_exec_hdr(h)
struct exec *h;
{
	int skip = 1;

	if (!N_BADMAG(*h))
		skip = 1;

	swap_longs((long *)h + skip, sizeof(*h)/sizeof(long) - skip);
}

void
md_swapout_exec_hdr(h)
struct exec *h;
{
	/* NetBSD: Always leave magic alone -- unless on little endian*/
	int skip = 1;
#if 0
	if (N_GETMAGIC(*h) == OMAGIC)
		skip = 0;
#endif

	swap_longs((long *)h + skip, sizeof(*h)/sizeof(long) - skip);
}


void
md_swapin_reloc(r, n)
struct relocation_info_m88k *r;
int n;
{
	struct r_relocation_info_m88k r_r;
	int *rev_int;

	if (sizeof (struct relocation_info_m88k) != sizeof (struct r_relocation_info_m88k)){
		fprintf(stderr, "fatal: relocation entry swapin\n");
		exit(23);
	}
	for (; n; n--, r++) {

		memcpy(&r_r,r,sizeof (struct relocation_info_m88k));
		r->r_address = md_swap_long(r_r.r_address);
		r->r_addend = md_swap_long(r_r.r_addend);
		rev_int = (((int *)(&r_r)) +1);
		*rev_int = md_swap_long(*rev_int);


		r->r_symbolnum = r_r.r_symbolnum;
		r->r_extern = r_r.r_extern;
		r->r_type = r_r.r_type;
		switch(r->r_type){
		case RELOC_LO16:
		case RELOC_HI16:
			r->r_baserel =1;
			break;
		case RELOC_PC16:
		case RELOC_PC26:
			r->r_pcrel =1;
			r->r_baserel =1;
			break;
		case RELOC_32:
			r->r_jmptable =1;
			break;
		case RELOC_IW16:
			break;
		}
#if 0
printf("%08x: %5x %4x %4x\n",
	r->r_address,
	r->r_symbolnum,
	r->r_extern,
	r->r_type);
#endif
	}
}

void
md_swapout_reloc(r, n)
struct relocation_info_m88k *r;
int n;
{
	int *rev_int;
	struct r_relocation_info_m88k r_r;

	for (; n; n--, r++) {
		r_r.r_address = md_swap_long(r->r_address);
		r_r.r_addend = md_swap_long(r->r_addend);
		r_r.r_symbolnum = r->r_symbolnum;
		r_r.r_extern = r->r_extern;
		r_r.r_type = r->r_type;
		rev_int = (((int *)(&r_r)) +1);
		*rev_int = md_swap_long(*rev_int);
		memcpy(r,&r_r,sizeof (struct relocation_info_m88k));
	}
}

void
md_swapout_jmpslot(j, n)
jmpslot_t	*j;
int		n;
{
	for (; n; n--, j++) {
		j->opcode = md_swap_short(j->opcode);
		j->addr[0] = md_swap_short(j->addr[0]);
		j->addr[1] = md_swap_short(j->addr[1]);
		j->reloc_index = md_swap_short(j->reloc_index);
	}
}

#else /* ! defined(NEEDSWAP) */
void
md_in_reloc(r, n)
struct relocation_info_m88k *r;
int n;
{
	for (; n; n--, r++) {

		switch(r->r_type){
		case RELOC_LO16:
		case RELOC_HI16:
/*			r->r_baserel =1; */
			break;
		case RELOC_PC16:
		case RELOC_PC26:
			r->r_pcrel =1;
/* 			r->r_baserel =1; */
			break;
		case RELOC_32:
	/*		r->r_jmptable =1; */
			break;
		case RELOC_IW16:
			break;
		case RELOC_GLOB_DAT:
			break;
#if 1
		case RELOC_JMP_SLOT:
	/*		r->r_jmptable =1; */
			break;
#endif
		case RELOC_RELATIVE: 
			r->r_pcrel =1;
			break;
		default:
			printf("error in relocation type %x\n",r->r_type);
		print_reloc(r);
			exit(10);
		}
/*
		print_reloc(r);
*/
	}
}
#endif /* ! NEED_SWAP */
print_reloc(struct relocation_info_m88k *r)
{
	printf ("%6x:\t%4d\t%d\t%d\t%d\t%d\t%x\t",
		r->r_address,
		r->r_symbolnum,
		r->r_pcrel,
		r->r_extern,
		r->r_baserel,
		r->r_jmptable,
		r->r_addend
	);
	switch (r->r_type) {
	case RELOC_LO16:
		printf("lo16\n");
		break;
	case RELOC_HI16:
		printf("hi16\n");
		break;
	case RELOC_PC16:
		printf("pc16\n");
		break;
	case RELOC_PC26:
		printf("pc26\n");
		break;
	case RELOC_32:
		printf("reloc32\n");
		break;
	case RELOC_IW16:
		printf("iw16\n");
		break;
	case NO_RELOC:
		printf("none\n");
		break;
	case RELOC_GLOB_DAT:
		printf("glob_dat\n");
		break;
	case RELOC_JMP_SLOT:
		printf("jmp_slot\n");
		break;
	case RELOC_RELATIVE: 
		printf("rel\n");
		break;
	}

}
