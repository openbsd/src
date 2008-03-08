/*	$OpenBSD: hvcall.S,v 1.1 2008/03/08 19:15:56 kettenis Exp $	*/

/*
 * Copyright (c) 2008 Mark Kettenis
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

#include "assym.h"
#include <machine/asm.h>

#define FAST_TRAP	0x80
#define MMU_MAP_ADDR	0x83
#define MMU_UNMAP_ADDR	0x84
#define CORE_TRAP	0xff

#define MACH_EXIT		0x00
#define MACH_DESC		0x01
#define MACH_SIR		0x02
#define MACH_SET_SOFT_STATE	0x03
#define MACH_GET_SOFT_STATE	0x04
#define MACH_SET_WATCHDOG	0x05

#define CPU_START		0x10
#define CPU_STOP		0x11
#define CPU_YIELD		0x12
#define CPU_QCONF		0x14
#define CPU_QINFO		0x15
#define CPU_MYID		0x16
#define CPU_STATE		0x17
#define CPU_SET_RTBA		0x18
#define CPU_GET_RTBA		0x19

#define MMU_TSB_CTX0		0x20
#define MMU_TSB_CTXNON0		0x21
#define MMU_DEMAP_PAGE		0x22
#define MMU_DEMAP_CTX		0x23
#define MMU_DEMAP_ALL		0x24
#define MMU_MAP_PERM_ADDR	0x25
#define MMU_FAULT_AREA_CONF	0x26
#define MMU_ENABLE		0x27
#define MMU_UNMAP_PERM_ADDR	0x28
#define MMU_TSB_CTX0_INFO	0x29
#define MMU_TSB_CTXNON0_INFO	0x2a
#define MMU_FAULT_AREA_INFO	0x2b

#define MEM_SCRUB		0x30
#define MEM_SYNC		0x31

#define CPU_MONDO_SEND		0x42

#define TOD_GET			0x50
#define TOD_SET			0x51

#define CONS_GETCHAR		0x60
#define CONS_PUTCHAR		0x61

#define INTR_DEVINO2SYSINO	0xa0
#define INTR_GETENABLED		0xa1
#define INTR_SETENABLED		0xa2
#define INTR_GETSTATE		0xa3
#define INTR_SETSTATE		0xa4
#define INTR_GETTARGET		0xa5
#define INTR_SETTARGET		0xa6

#define PCI_IOMMU_MAP		0xb0
#define PCI_IOMMU_DEMAP		0xb1
#define PCI_IOMMU_GETMAP	0xb2
#define PCI_IOMMU_GETBYPASS	0xb3
#define PCI_CONFIG_GET		0xb4
#define PCI_CONFIG_PUT		0xb5

#define API_SET_VERSION		0x00
#define API_PUTCHAR		0x01
#define API_EXIT		0x02
#define API_GET_VERSION		0x03


ENTRY(hv_api_putchar)
	mov	API_PUTCHAR, %o5
	ta	CORE_TRAP
	retl
	 nop

ENTRY(hv_api_get_version)
	mov	%o2, %o4
	mov	%o1, %o3
	mov	API_GET_VERSION, %o5
	ta	CORE_TRAP
	stx	%o1, [%o3]
	retl
	 stx	%o2, [%o4]

ENTRY(hv_mach_set_soft_state)
	mov	MACH_SET_SOFT_STATE, %o5
	ta	FAST_TRAP
	retl
	 nop

ENTRY(hv_cpu_qconf)
	mov	CPU_QCONF, %o5
	ta	FAST_TRAP
	retl
	 nop

ENTRY(hv_cpu_mondo_send)
	mov	CPU_MONDO_SEND, %o5
	ta	FAST_TRAP
	retl
	 nop

ENTRY(hv_cpu_myid)
	mov	%o0, %o2
	mov	CPU_MYID, %o5
	ta	FAST_TRAP
	retl
	 stx	%o1, [%o2]

ENTRY(hv_mmu_tsb_ctx0)
	mov	MMU_TSB_CTX0, %o5
	ta	FAST_TRAP
	retl	
	 nop

ENTRY(hv_mmu_tsb_ctxnon0)
	mov	MMU_TSB_CTXNON0, %o5
	ta	FAST_TRAP
	retl	
	 nop

ENTRY(hv_mmu_demap_page)
	mov	%o2, %o4
	mov	%o1, %o3
	mov	%o0, %o2
	clr	%o1
	clr	%o0
	mov	MMU_DEMAP_PAGE, %o5
	ta	FAST_TRAP
	retl	
	 nop

ENTRY(hv_mmu_demap_ctx)
	mov	%o1, %o3
	mov	%o0, %o2
	clr	%o1
	clr	%o0
	mov	MMU_DEMAP_CTX, %o5
	ta	FAST_TRAP
	retl	
	 nop

ENTRY(hv_mmu_demap_all)
	mov	%o1, %o3
	mov	%o0, %o2
	clr	%o1
	clr	%o0
	mov	MMU_DEMAP_CTX, %o5
	ta	FAST_TRAP
	retl	
	 nop

ENTRY(hv_mmu_map_perm_addr)
	mov	%o2, %o3
	mov	%o1, %o2
	clr	%o1
	mov	MMU_MAP_PERM_ADDR, %o5
	ta	FAST_TRAP
	retl
	 nop

ENTRY(hv_mmu_unmap_perm_addr)
	mov	%o1, %o2
	clr	%o1
	mov	MMU_UNMAP_PERM_ADDR, %o5
	ta	FAST_TRAP
	retl
	 nop

ENTRY(hv_mmu_map_addr)
	ta	MMU_MAP_ADDR
	retl
	 nop

ENTRY(hv_mmu_unmap_addr)
	ta	MMU_UNMAP_ADDR
	retl
	 nop

ENTRY(hv_mem_scrub)
	mov	MEM_SCRUB, %o5
	ta	FAST_TRAP
	retl
	 nop

ENTRY(hv_mem_sync)
	mov	MEM_SYNC, %o5
	ta	FAST_TRAP
	retl
	 nop

ENTRY(hv_tod_get)
	mov	%o0, %o2
	mov	TOD_GET, %o5
	ta	FAST_TRAP
	retl
	 stx	%o1, [%o2]

ENTRY(hv_tod_set)
	mov	TOD_SET, %o5
	ta	FAST_TRAP
	retl
	 nop

ENTRY(hv_cons_getchar)
	mov	%o0, %o2
	mov	CONS_GETCHAR, %o5
	ta	FAST_TRAP
	retl
	 stx	%o1, [%o2]

ENTRY(hv_cons_putchar)
	mov	CONS_PUTCHAR, %o5
	ta	FAST_TRAP
	retl
	 nop

ENTRY(hv_intr_devino_to_sysino)
	mov	INTR_DEVINO2SYSINO, %o5
	ta	FAST_TRAP
	retl
	 stx	%o1, [%o2]

ENTRY(hv_intr_getenabled)
	mov	%o1, %o2
	mov	INTR_GETENABLED, %o5
	ta	FAST_TRAP
	retl
	 stx	%o1, [%o2]

ENTRY(hv_intr_setenabled)
	mov	INTR_SETENABLED, %o5
	ta	FAST_TRAP
	retl
	 nop

ENTRY(hv_intr_getstate)
	mov	%o1, %o2
	mov	INTR_GETSTATE, %o5
	ta	FAST_TRAP
	retl
	 stx	%o1, [%o2]

ENTRY(hv_intr_setstate)
	mov	INTR_SETSTATE, %o5
	ta	FAST_TRAP
	retl
	 nop

ENTRY(hv_intr_gettarget)
	mov	%o1, %o2
	mov	INTR_GETTARGET, %o5
	ta	FAST_TRAP
	retl
	 stx	%o1, [%o2]

ENTRY(hv_intr_settarget)
	mov	INTR_SETTARGET, %o5
	ta	FAST_TRAP
	retl
	 nop

ENTRY(hv_pci_iommu_map)
	mov	%o5, %g5
	mov	PCI_IOMMU_MAP, %o5
	ta	FAST_TRAP
	retl
	 stx	%o1, [%g5]

ENTRY(hv_pci_iommu_demap)
	mov	PCI_IOMMU_DEMAP, %o5
	ta	FAST_TRAP
	retl
	 stx	%o1, [%o3]

ENTRY(hv_pci_iommu_getmap)
	mov	%o2, %o4
	mov	PCI_IOMMU_GETMAP, %o5
	ta	FAST_TRAP
	stx	%o1, [%o4]
	retl
	 stx	%o2, [%o3]

ENTRY(hv_pci_iommu_getbypass)
	mov	PCI_IOMMU_GETBYPASS, %o5
	ta	FAST_TRAP
	retl
	 stx	%o1, [%o3]

ENTRY(hv_pci_config_get)
	mov	%o5, %g5
	mov	PCI_CONFIG_GET, %o5
	ta	FAST_TRAP
	stx	%o1, [%o4]
	retl
	 stx	%o2, [%g5]

ENTRY(hv_pci_config_put)
	mov	%o5, %g5
	mov	PCI_CONFIG_PUT, %o5
	ta	FAST_TRAP
	retl
	 stx	%o1, [%g5]
