/*	$OpenBSD: md.h,v 1.3 2002/07/15 21:05:57 marc Exp $	*/
/*	$NetBSD: md.h,v 1.1 1996/05/17 21:51:10 mark Exp $	*/

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

/* First cut for arm32 */
#if defined(CROSS_LINKER)
#include <sys/endian.h>
#if BYTE_ORDER != LITTLE_ENDIAN
#define NEED_SWAP
#endif

#endif

#define	MAX_ALIGNMENT		(sizeof (long))

#define PAGSIZ			__LDPGSZ

#define N_SET_FLAG(ex,f)	(N_SETMAGIC(ex,			\
					    N_GETMAGIC(ex),	\
					    MID_MACHINE,	\
					    N_GETFLAG(ex)|(f)))

#define N_IS_DYNAMIC(ex)	((N_GETFLAG(ex) & EX_DYNAMIC))

#define N_BADMID(ex)		(N_GETMID(ex) != MID_MACHINE)

/*
 * Should be handled by a.out.h ?
 */
#define N_ADJUST(ex)		(((ex).a_entry < PAGSIZ) ? -PAGSIZ : 0)
#ifdef	__notyet__
#define TEXT_START(ex)		(N_TXTADDR(ex) + N_ADJUST(ex))
#define DATA_START(ex)		(N_DATADDR(ex) + N_ADJUST(ex))
#else
#define	TEXT_START(ex)		((ex).a_entry < PAGSIZ ? 0 : 0x001000)
#define	DATA_START(ex)		(N_GETMAGIC(ex) == OMAGIC \
	 ? TEXT_START(ex) + (ex).a_text \
	 : (TEXT_START(ex) + (ex).a_text + __LDPGSZ - 1) & ~(__LDPGSZ - 1))
#endif

#define RELOC_STATICS_THROUGH_GOT_P(r)	0	/* ??? */
#define JMPSLOT_NEEDS_RELOC		1

#define md_got_reloc(r)			0

#define md_get_rt_segment_addend(r,a)	md_get_addend(r,a)

/* Width of a Global Offset Table entry */
#define GOT_ENTRY_SIZE	4
typedef long	got_t;

typedef struct jmpslot {
	u_long	opcode1;	/* mov ip, lr */
	u_long	opcode2;	/* call */
	u_long	reloc_index;	/* used as destination after fixup */
#define	JMPSLOT_RELOC_MASK	0xffffffff
} jmpslot_t;

#define	SAVEPC	0xe1a0c00e	/* MOV ip, lr */
#define CALL	0xeb000000	/* CALL opcode */
#define JUMP	0xe59ff000	/* LDR pc, [pc] (used as JMP) */
#define TRAP	0xe6000010	/* Undefined Instruction (used for bpt) */

/*
 * Byte swap defs for cross linking
 */

#if !defined(NEED_SWAP)

#define md_swapin_exec_hdr(h)
#define md_swapout_exec_hdr(h)
#define md_swapin_symbols(s,n)
#define md_swapout_symbols(s,n)
#define md_swapin_zsymbols(s,n)
#define md_swapout_zsymbols(s,n)
#define md_swapin_reloc(r,n)
#define md_swapout_reloc(r,n)
#define md_swapin__dynamic(l)
#define md_swapout__dynamic(l)
#define md_swapin_section_dispatch_table(l)
#define md_swapout_section_dispatch_table(l)
#define md_swapin_so_debug(d)
#define md_swapout_so_debug(d)
#define md_swapin_rrs_hash(f,n)
#define md_swapout_rrs_hash(f,n)
#define md_swapin_sod(l,n)
#define md_swapout_sod(l,n)
#define md_swapout_jmpslot(j,n)
#define md_swapout_got(g,n)
#define md_swapin_ranlib_hdr(h,n)
#define md_swapout_ranlib_hdr(h,n)

#endif /* NEED_SWAP */

#ifdef CROSS_LINKER

#define get_byte(p)	( ((unsigned char *)(p))[0] )

#define get_short(p)	( ( ((unsigned char *)(p))[1] << 8) | \
			  ( ((unsigned char *)(p))[0]	  )   \
			)

#define get_long(p)	( ( ((unsigned char *)(p))[3] << 24) | \
			  ( ((unsigned char *)(p))[2] << 16) | \
			  ( ((unsigned char *)(p))[1] << 8 ) | \
			  ( ((unsigned char *)(p))[0]	   )   \
			)

#define put_byte(p, v)	{ ((unsigned char *)(p))[0] = ((unsigned long)(v)); }

#define put_short(p, v)	{ ((unsigned char *)(p))[1] =			\
				((((unsigned long)(v)) >> 8) & 0xff); 	\
			  ((unsigned char *)(p))[0] =			\
				((((unsigned long)(v))	   ) & 0xff); }

#define put_long(p, v)	{ ((unsigned char *)(p))[3] =			\
				((((unsigned long)(v)) >> 24) & 0xff); 	\
			  ((unsigned char *)(p))[2] =			\
				((((unsigned long)(v)) >> 16) & 0xff); 	\
			  ((unsigned char *)(p))[1] =			\
				((((unsigned long)(v)) >>  8) & 0xff); 	\
			  ((unsigned char *)(p))[0] =			\
				((((unsigned long)(v))	    ) & 0xff); }

#ifdef NEED_SWAP

/* Define IO byte swapping routines */

void	md_swapin_exec_hdr __P((struct exec *));
void	md_swapout_exec_hdr __P((struct exec *));
void	md_swapin_reloc __P((struct relocation_info *, int));
void	md_swapout_reloc __P((struct relocation_info *, int));
void	md_swapout_jmpslot __P((jmpslot_t *, int));

#define md_swapin_symbols(s,n)			swap_symbols(s,n)
#define md_swapout_symbols(s,n)			swap_symbols(s,n)
#define md_swapin_zsymbols(s,n)			swap_zsymbols(s,n)
#define md_swapout_zsymbols(s,n)		swap_zsymbols(s,n)
#define md_swapin__dynamic(l)			swap__dynamic(l)
#define md_swapout__dynamic(l)			swap__dynamic(l)
#define md_swapin_section_dispatch_table(l)	swap_section_dispatch_table(l)
#define md_swapout_section_dispatch_table(l)	swap_section_dispatch_table(l)
#define md_swapin_so_debug(d)			swap_so_debug(d)
#define md_swapout_so_debug(d)			swap_so_debug(d)
#define md_swapin_rrs_hash(f,n)			swap_rrs_hash(f,n)
#define md_swapout_rrs_hash(f,n)		swap_rrs_hash(f,n)
#define md_swapin_sod(l,n)			swapin_sod(l,n)
#define md_swapout_sod(l,n)			swapout_sod(l,n)
#define md_swapout_got(g,n)			swap_longs((long*)(g),n)
#define md_swapin_ranlib_hdr(h,n)		swap_ranlib_hdr(h,n)
#define md_swapout_ranlib_hdr(h,n)		swap_ranlib_hdr(h,n)

#define md_swap_short(x) ( (((x) >> 8) & 0xff) | (((x) & 0xff) << 8) )

#define md_swap_long(x) ( (((x) >> 24) & 0xff	 ) | (((x) >> 8 ) & 0xff00   ) | \
			(((x) << 8 ) & 0xff0000) | (((x) << 24) & 0xff000000))

#else	/* We need not swap, but must pay attention to alignment: */

#define md_swap_short(x)	(x)
#define md_swap_long(x)		(x)

#endif /* NEED_SWAP */

#else	/* Not a cross linker: use native */

#define md_swap_short(x)		(x)
#define md_swap_long(x)			(x)

#define get_byte(where)			(*(char *)(where))
#define get_short(where)		(*(short *)(where))
#define get_long(where)			(*(long *)(where))

#define put_byte(where,what)		(*(char *)(where) = (what))
#define put_short(where,what)		(*(short *)(where) = (what))
#define put_long(where,what)		(*(long *)(where) = (what))

#endif /* CROSS_LINKER */

/*
 * Define all the RELOC_ macros
 */
#define RELOC_ADDRESS(r)		((r)->r_address)
#define RELOC_EXTERN_P(r)		((r)->r_extern)
#define RELOC_TYPE(r)			((r)->r_symbolnum)
#define RELOC_SYMBOL(r)			((r)->r_symbolnum)
/* #define RELOC_MEMORY_SUB_P(r)		((r)->r_neg)	not used */
#define RELOC_MEMORY_ADD_P(r)		1
#undef RELOC_ADD_EXTRA
#define RELOC_PCREL_P(r)		(((r)->r_pcrel == 1) ^ ((r)->r_length == 3))
#define RELOC_VALUE_RIGHTSHIFT(r)	0
/*#define RELOC_TARGET_SIZE(r)		((r)->r_length)		not used */
/*#define RELOC_TARGET_BITPOS(r)		0		not used */
/*#define RELOC_TARGET_BITSIZE(r)		32		not used */

#define RELOC_JMPTAB_P(r)		((r)->r_jmptable)
#define RELOC_BASEREL_P(r)		((r)->r_baserel)
#define RELOC_RELATIVE_P(r)		((r)->r_relative)
#define RELOC_COPY_P(r)			(0)
#define RELOC_LAZY_P(r)			((r)->r_jmptable)

#define CHECK_GOT_RELOC(r)		RELOC_PCREL_P(r)
#define RELOC_PIC_TYPE(r)		((r)->r_baserel?\
						PIC_TYPE_LARGE:PIC_TYPE_NONE)

#define RELOC_INIT_SEGMENT_RELOC(r)
