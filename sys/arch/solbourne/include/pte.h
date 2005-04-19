/*	$OpenBSD: pte.h,v 1.1 2005/04/19 21:30:18 miod Exp $	*/
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

/*
 * KAP page table entries.
 *
 * Ref/Mod bits are handled in software.
 */

/*
 * First-level : Page Directory Tables (topmost 9 bits of a va)
 *
 * Page directory entries contain both the pa and the va of the page
 * tables they point to.
 */

#define	PDT_INDEX_SIZE		9
#define	PDT_INDEX_SHIFT		23
#define	PDT_INDEX_MASK		0xff800000

/*
 * Second-level: Page Table Entries (middle 10 bits of a va)
 */

#define	PT_INDEX_SIZE		10
#define	PT_INDEX_SHIFT		13
#define	PT_INDEX_MASK		0x007fe000

#define	PG_V			0x00000001
#define	PG_NV			0x00000000
#define	PG_RO			0x00000002	/* read only */
#define	PG_RW			0x00000000
#define	PG_PROT			(PG_RO | PG_S)
#define	PG_S			0x00000004	/* supervisor only */
#define	PG_MA			0x00000018	/* memory attributes mask */
#define	PG_G			0x00000020	/* global */
/* software bits from now on... */
#define	PG_W			0x00000040	/* wired */
#define	PG_M			0x00000080	/* modified */
#define	PG_U			0x00000100	/* referenced */
			/*	0x00001e00	unused */
#define	PG_FRAME		0xffffe000	/* PFN mask */

/* memory attributes */
#define	PG_IO			0x00000000	/* not cached */
#define	PG_CACHE		0x00000008	/* cached */
#define	PG_BYTE_SHARED		0x00000010	/* byte-writeable shared */
#define	PG_SHARED		0x00000018	/* non byte-writeable shared */

/*
 * Page directory constants
 */

#define	PDT_SIZE		4096	/* size of a page directory table */
#define	PT_SIZE			4096	/* size of a page table */

#define	NBR_PDE			(PDT_SIZE / 8)
#define	NBR_PTE			(PT_SIZE / 4)

#define	NBSEG			(1 << PDT_INDEX_SHIFT)

#if !defined(_LOCORE)

typedef	u_int32_t	pt_entry_t;

typedef struct {
	u_int32_t	pde_pa;
	pt_entry_t*	pde_va;
} pd_entry_t;

#endif
