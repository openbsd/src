/*	$OpenBSD: kap.h,v 1.2 2006/04/15 17:35:48 miod Exp $	*/
/*
 * Copyright (c) 2005, Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SOLBOURNE_KAP_H_
#define _SOLBOURNE_KAP_H_

/*
 * KAP specific control registers
 */

#ifdef _KERNEL

/* TLB handling - write only */
#define	ASI_GTLB_RANDOM		0xc0	/* random TLB drop-in */
#define	ASI_GTLB_DROPIN		0xc1	/* TLB drop-in */
#define	ASI_GTLB_INVAL_ENTRY	0xc2	/* invalidate entry */
#define	ASI_GTLB_INVAL_PID	0xc3	/* invalidate PID */
#define	ASI_GTLB_INVALIDATE	0xc4	/* invalidate entire TLB */
#define	ASI_ITLB_DROPIN		0xc8	/* iTLB drop-in */

/* TLB position addressing */
#define	TLB_SLOT(x)		((x) << 3)
#define	TLB_INCR		(1 << 3)
#define	GTLB_SLOTS		(128 + 8)	/* XXX unsure */
#define	ITLB_SLOTS		8		/* XXX unsure */

/* data cache handling - read only except ASI_DCACHE_RW */
#define	ASI_DCACHE_FLUSH	0xd0	/* flush dcache block */
#define	ASI_DCACHE_LOOKUP	0xd1	/* check for dcache hit */
#define	ASI_DCACHE_RW		0xd2	/* read/write dcache */
#define	ASI_DCACHE_INVAL	0xd3	/* invalidate dcache */

/* cache line addressing (for D flushes) */
#define	DCACHE_LINE(x)		((x) << 2)
#define	DCACHE_INCR		(1 << 2)
#define	DCACHE_LINES		256

/* bus access - read/write */
#define	ASI_PHYS_IO		0xd4	/* not cached */
#define	ASI_PHYS_CACHED		0xd5	/* cached */
#define	ASI_PHYS_NBW		0xd6	/* non byte writeable shared */
#define	ASI_PHYS_BW		0xd7	/* byte writeable shared */

/* inst cache handling - read only except ASI_ICACHE_RW */
#define	ASI_ICACHE_LOOKUP	0xd9	/* check for icache hit */
#define	ASI_ICACHE_RW		0xda	/* read/write icache */
#define	ASI_ICACHE_INVAL	0xdb	/* invalidate icache */

/* MMU registers */
#define	ASI_MMCR		0xe0	/* control register, rw */
#define	ASI_PDBR		0xe1	/* page directory base address, rw */
#define	ASI_FVAR		0xe2	/* fault va, rw */
#define	ASI_PDER		0xe3	/* page directory entry pointer, ro */
#define	ASI_PTOR		0xe4	/* page table offset, ro */
#define	ASI_FPAR		0xe5	/* fault pa, rw */
#define	ASI_FPSR		0xe6	/* fault ASI, rw */
#define	ASI_PIID		0xe7	/* process ID invalidation, rw */
#define	ASI_PID			0xe8	/* process ID, rw */
#define	ASI_BCR			0xe9	/* bus control, rw */
#define	ASI_FCR			0xea	/* fault cause, rw */
#define	ASI_PTW0		0xeb	/* translation window #0, rw */
#define	ASI_PTW1		0xec	/* translation window #0, rw */
#define	ASI_PTW2		0xed	/* translation window #0, rw */

/* Hardware watchdog */
#define	ASI_WAR0		0xee	/* watchpoint address 0, rw */
#define	ASI_WAR1		0xef	/* watchpoint address 1, rw */
#define	ASI_WCR			0xf0	/* watchpoint control register, rw */

/* MMCR fields */
#define	MMCR_ENABLE		0x00000001	/* MMU enable */
#define	MMCR_MATCH_PTW		0x00000002	/* lookup matches PTW */
#define	MMCR_MATCH_ITLB		0x00000004	/* lookup matches ITLB */
#define	MMCR_MATCH_GTLB		0x00000008	/* lookup matches GTLB */
#define	MMCR_ISET0		0x00000080	/* icache set 0 */
#define	MMCR_ISET1		0x00000100	/* icache set 1 */
#define	MMCR_ISET2		0x00000200	/* icache set 2 */
#define	MMCR_DSET0		0x00000400	/* dcache set 0 */
#define	MMCR_DSET1		0x00000800	/* dcache set 1 */

/* BCR fields */
#define	BCR_FAULT_SYNDROME	0x000000ff	/* ECC syndrome byte */
#define	BCR_ECC			0x00000100	/* ECC enable */
#define	BCR_FAULT_DISABLE	0x00000200	/* disable ECC faults */

/* FCR fields */
#define	FCR_PROTMASK		0x0000000f
#define	FCR_V			0x00000001	/* page not valid */
#define	FCR_RO			0x00000002	/* write access on read only */
#define	FCR_S			0x00000004	/* user access on sup. only */
#define	FCR_EXTERNAL		0x00000100	/* external fault */
#define	FCR_ECC_SINGLE		0x00000200	/* single bit ECC */
#define	FCR_ECC_MULTIPLE	0x00000400	/* multiple bit ECC */

#define	FCR_BITS	"\020\01V\02RO\03S\011EXTERNAL\012ECCS\013ECCM"

/* PTW fields */
#define	PTW_V			0x00000001	/* valid */
#define	PTW_RO			0x00000002	/* read only */
#define	PTW_RW			0x00000000
#define	PTW_S			0x00000004	/* supervisor only */
#define	PTW_CACHEABLE		0x00000008
#define	PTW_BYTE_SHARED		0x00000010
#define	PTW_SHARED		0x00000018
#define	PTW_MASK_MASK		0x0000ff00	/* window address mask */
#define	PTW_MASK_SHIFT		8
#define	PTW_PA_MASK		0x00ff0000	/* physical window */
#define	PTW_PA_SHIFT		16
#define	PTW_VA_MASK		0xff000000	/* virtual window */
#define	PTW_VA_SHIFT		24

#define	PTW_WINDOW_SIZE		0x01000000
#define	PTW_WINDOW_MASK		0xff000000
#define	PTW_WINDOW_SHIFT	24

#define	PTW_TEMPLATE(va,pa,size) \
	(((va) << PTW_VA_SHIFT) | ((pa) << PTW_PA_SHIFT) | \
	 (((~((size) - 1) >> 24) << PTW_MASK_SHIFT) & PTW_MASK_MASK))

/*
 * Initial virtual memory settings
 */

#define	ROM_WINDOW	0x00
#define	PTW0_BASE	(vaddr_t)(ROM_WINDOW << PTW_WINDOW_SHIFT)
#define	PHYSMEM_WINDOW	0xf0
#define	PHYSMEM_BASE	(vaddr_t)(PHYSMEM_WINDOW << PTW_WINDOW_SHIFT)
#define	PTW1_WINDOW	0xfd
#define	PTW1_BASE	(vaddr_t)(PTW1_WINDOW << PTW_WINDOW_SHIFT)
#define	PTW2_WINDOW	0xfe
#define	PTW2_BASE	(vaddr_t)(PTW2_WINDOW << PTW_WINDOW_SHIFT)
#define	PTW0_DEFAULT	\
	PTW_TEMPLATE(ROM_WINDOW, ROM_WINDOW, 0x10000000) | PTW_S | PTW_V
#define	PTW1_DEFAULT	PTW_CACHEABLE | \
	PTW_TEMPLATE(PTW1_WINDOW, PHYSMEM_WINDOW, 0x01000000) | PTW_S | PTW_V
#define	PTW2_DEFAULT	PTW_SHARED | \
	PTW_TEMPLATE(PTW2_WINDOW, PHYSMEM_WINDOW, 0x01000000) | PTW_S | PTW_V

#define	PTW0_TO_PHYS(va)	(paddr_t)(va)
#define	PTW1_TO_PHYS(va)	(paddr_t)((va) - PTW1_BASE + PHYSMEM_BASE)
#define	PTW2_TO_PHYS(va)	(paddr_t)((va) - PTW2_BASE + PHYSMEM_BASE)

#define	PHYS_TO_PTW0(pa)	(vaddr_t)(pa)
#define	PHYS_TO_PTW1(pa)	(vaddr_t)((pa) - PHYSMEM_BASE + PTW1_BASE)
#define	PHYS_TO_PTW2(pa)	(vaddr_t)((pa) - PHYSMEM_BASE + PTW2_BASE)

#endif	/* _KERNEL */

#endif /* _SOLBOURNE_KAP_H_ */
