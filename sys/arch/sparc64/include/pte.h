/*	$OpenBSD: pte.h,v 1.2 2001/08/20 20:23:52 jason Exp $	*/
/*	$NetBSD: pte.h,v 1.7 2001/07/31 06:55:46 eeh Exp $ */

/*
 * Copyright (c) 1996-1999 Eduardo Horvath
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Address translation works as follows:
 *
 **
 * For sun4u:
 *	
 *	Take your pick; it's all S/W anyway.  We'll start by emulating a sun4.
 *	Oh, here's the sun4u TTE for reference:
 *
 *	struct sun4u_tte {
 *		u_int64	tag_g:1,	(global flag)
 *			tag_ctxt:15,	(context for mapping)
 *			tag_unassigned:6,
 *			tag_va:42;	(virtual address bits<64:22>)
 *		u_int64	data_v:1,	(valid bit)
 *			data_size:2,	(page size [8K*8**<SIZE>])
 *			data_nfo:1,	(no-fault only)
 *			data_ie:1,	(invert endianness [inefficient])
 *			data_soft2:2,	(reserved for S/W)
 *			data_pa:36,	(physical address)
 *			data_soft:6,	(reserved for S/W)
 *			data_lock:1,	(lock into TLB)
 *			data_cacheable:2,	(cacheability control)
 *			data_e:1,	(explicit accesses only)
 *			data_priv:1,	(privileged page)
 *			data_w:1,	(writeable)
 *			data_g:1;	(same as tag_g)
 *	};	
 */

/* virtual address to virtual page number */
#define	VA_SUN4U_VPG(va)	(((int)(va) >> 13) & 31)

/* virtual address to offset within page */
#define VA_SUN4U_OFF(va)       	(((int)(va)) & 0x1FFF)

/* When we go to 64-bit VAs we need to handle the hole */
#define VA_VPG(va)	VA_SUN4U_VPG(va)
#define VA_OFF(va)	VA_SUN4U_OFF(va)

#define PG_SHIFT4U	13
#define MMU_PAGE_ALIGN	8192

/* If you know where a tte is in the tsb, how do you find its va? */	
#define TSBVA(i)	((tsb[(i)].tag.f.tag_va<<22)|(((i)<<13)&0x3ff000))

#ifndef _LOCORE
/* 
 *  This is the spitfire TTE.
 *
 *  We could use bitmasks and shifts to construct this if
 *  we had a 64-bit compiler w/64-bit longs.  Otherwise it's
 *  a real pain to do this in C.
 */
#if 0
/* We don't use bitfeilds anyway. */
struct sun4u_tag_fields {
	u_int64_t	tag_g:1,	/* global flag */
		tag_ctxt:15,	/* context for mapping */
		tag_unassigned:6,
		tag_va:42;	/* virtual address bits<64:22> */
};
union sun4u_tag { struct sun4u_tag_fields f; int64_t tag; };
struct sun4u_data_fields {
	u_int64_t	data_v:1,	/* valid bit */
		data_size:2,	/* page size [8K*8**<SIZE>] */
		data_nfo:1,	/* no-fault only */
		data_ie:1,	/* invert endianness [inefficient] */
		data_soft2:2,	/* reserved for S/W */
		data_pa:36,	/* physical address */
		data_accessed:1,/* S/W accessed bit */
		data_modified:1,/* S/W modified bit */
		data_realw:1,	/* S/W real writable bit (to manage modified) */
		data_tsblock:1,	/* S/W TSB locked entry */
		data_exec:1,	/* S/W Executable */
		data_onlyexec:1,/* S/W Executable only */
		data_lock:1,	/* lock into TLB */
		data_cacheable:2,	/* cacheability control */
		data_e:1,	/* explicit accesses only */
		data_priv:1,	/* privileged page */
		data_w:1,	/* writeable */
		data_g:1;	/* same as tag_g */
};
union sun4u_data { struct sun4u_data_fields f; int64_t data; };
struct sun4u_tte {
	union sun4u_tag tag;
	union sun4u_data data;
};
#else
struct sun4u_tte {
	int64_t tag;
	int64_t data;
};
#endif
typedef struct sun4u_tte pte_t;

/* Assembly routine to flush a mapping */
extern void tlb_flush_pte __P((vaddr_t addr, int ctx));
extern void tlb_flush_ctx __P((int ctx));

#endif /* _LOCORE */

/* TSB tag masks */
#define CTX_MASK		((1<<13)-1)
#define TSB_TAG_CTX_SHIFT	48
#define TSB_TAG_VA_SHIFT	22
#define TSB_TAG_G		0x8000000000000000LL

#define TSB_TAG_CTX(t)		((((int64_t)(t))>>TSB_TAG_CTX_SHIFT)&CTX_MASK)
#define TSB_TAG_VA(t)		((((int64_t)(t))<<TSB_TAG_VA_SHIFT))
#define TSB_TAG(g,ctx,va)	((((u_int64_t)((g)!=0))<<63)|(((u_int64_t)(ctx)&CTX_MASK)<<TSB_TAG_CTX_SHIFT)|(((u_int64_t)va)>>TSB_TAG_VA_SHIFT))

/* Page sizes */
#define	PGSZ_8K			0
#define	PGSZ_64K		1
#define	PGSZ_512K		2
#define	PGSZ_4M			3

#define	PGSZ_SHIFT		61

/*
 * Why couldn't Sun pick better page sizes?
 *
 * Page sizes are 2**(12+(3*sz)), except for 8K which
 * is 2**12+1 instead of 2**12.
 */
#define	PG_SZ(s)		(1<<(12+(s?(3*s):1)))
#define	TLB_SZ(s)		(((uint64_t)(s))<<PGSZ_SHIFT)

/* TLB data masks */
#define TLB_V			0x8000000000000000LL
#define TLB_8K			TLB_SZ(PGSZ_8K)
#define TLB_64K			TLB_SZ(PGSZ_64K)
#define TLB_512K		TLB_SZ(PGSZ_512K)
#define TLB_4M			TLB_SZ(PGSZ_4M)
#define TLB_SZ_MASK		0x6000000000000000LL
#define TLB_NFO			0x1000000000000000LL
#define TLB_IE			0x0800000000000000LL
#define TLB_SOFT2_MASK		0x07fe000000000000LL
#define TLB_DIAG_MASK		0x0001fe0000000000LL
#define TLB_PA_MASK		0x000001ffffffe000LL
#define TLB_SOFT_MASK		0x0000000000001f80LL
/* S/W bits */
/* Access & TSB locked bits are swapped so I can set access w/one insn */
/* #define TLB_ACCESS		0x0000000000001000LL */
#define TLB_ACCESS		0x0000000000000200LL
#define TLB_MODIFY		0x0000000000000800LL
#define TLB_REAL_W		0x0000000000000400LL
/* #define TLB_TSB_LOCK		0x0000000000000200LL */
#define TLB_TSB_LOCK		0x0000000000001000LL
#define TLB_EXEC		0x0000000000000100LL
#define TLB_EXEC_ONLY		0x0000000000000080LL
/* H/W bits */
#define TLB_L			0x0000000000000040LL
#define TLB_CACHE_MASK		0x0000000000000030LL
#define TLB_CP			0x0000000000000020LL
#define TLB_CV			0x0000000000000010LL
#define TLB_E			0x0000000000000008LL
#define TLB_P			0x0000000000000004LL
#define TLB_W			0x0000000000000002LL
#define TLB_G			0x0000000000000001LL

/* 
 * The following bits are used by locore so they should
 * be duplicates of the above w/o the "long long"
 */
/* S/W bits */
/* #define TTE_ACCESS		0x0000000000001000 */
#define TTE_ACCESS		0x0000000000000200
#define TTE_MODIFY		0x0000000000000800
#define TTE_REAL_W		0x0000000000000400
/* #define TTE_TSB_LOCK		0x0000000000000200 */
#define TTE_TSB_LOCK		0x0000000000001000
#define TTE_EXEC		0x0000000000000100
#define TTE_EXEC_ONLY		0x0000000000000080
/* H/W bits */
#define TTE_L			0x0000000000000040
#define TTE_CACHE_MASK		0x0000000000000030
#define TTE_CP			0x0000000000000020
#define TTE_CV			0x0000000000000010
#define TTE_E			0x0000000000000008
#define TTE_P			0x0000000000000004
#define TTE_W			0x0000000000000002
#define TTE_G			0x0000000000000001

#define TTE_DATA_BITS	"\177\20" \
        "b\77V\0" "f\75\2SIZE\0" "b\77V\0" "f\75\2SIZE\0" \
        "=\0008K\0" "=\00164K\0" "=\002512K\0" "=\0034M\0" \
        "b\74NFO\0"     "b\73IE\0"      "f\62\10SOFT2\0" \
        "f\51\10DIAG\0" "f\15\33PA<40:13>\0" "f\7\5SOFT\0" \
        "b\6L\0"        "b\5CP\0"       "b\4CV\0" \
        "b\3E\0"        "b\2P\0"        "b\1W\0"        "b\0G\0"

#define TSB_DATA(g,sz,pa,priv,write,cache,aliased,valid,ie) \
(((valid)?TLB_V:0LL)|TLB_SZ(sz)|(((u_int64_t)(pa))&TLB_PA_MASK)|\
((cache)?((aliased)?TLB_CP:TLB_CACHE_MASK):TLB_E)|\
((priv)?TLB_P:0LL)|((write)?TLB_W:0LL)|((g)?TLB_G:0LL)|((ie)?TLB_IE:0LL))

#define MMU_CACHE_VIRT	0x3
#define MMU_CACHE_PHYS	0x2
#define MMU_CACHE_NONE	0x0

/* This needs to be updated for sun4u IOMMUs */
/*
 * IOMMU PTE bits.
 */
#define IOPTE_PPN_MASK  0x07ffff00
#define IOPTE_PPN_SHIFT 8
#define IOPTE_RSVD      0x000000f1
#define IOPTE_WRITE     0x00000004
#define IOPTE_VALID     0x00000002
