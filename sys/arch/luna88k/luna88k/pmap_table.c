/*	$OpenBSD: pmap_table.c,v 1.1.1.1 2004/04/21 15:24:13 aoyama Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1993-1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <machine/board.h>
#include <machine/cmmu.h>
#include <uvm/uvm_extern.h>
#include <machine/pmap_table.h>

#define	R	VM_PROT_READ
#define	RW	(VM_PROT_READ | VM_PROT_WRITE)
#define	CW	CACHE_WT
#define	CI	CACHE_INH
#define	CG	CACHE_GLOBAL

/*  phys_start, virt_start, size, prot, cacheability */
const pmap_table_entry
luna88k_board_table[] = {
#if 0
	{ 0           , VEQR_ADDR   , 48*1024*1024 /* XXX memory size */, RW,  CG},
#endif
	{ 0           , 0           , round_page(0x20000)  /* XXX kernel start */, RW,  CG},
	{ OBIO_START  , OBIO_START  , round_page(OBIO_SIZE),  RW,     CI },
#if 0
	{ TRI_PORT_RAM, TRI_PORT_RAM, TRI_PORT_RAM_SPACE, RW, CI },
	{ 0           , VEQR_ADDR   , 48*1024*1024 /* XXX memory size */, RW,  CG},
	{ 0           , 0           , 0x20000 /* XXX kernel start */, RW,  CG},
	{ PROM_ADDR   , PROM_ADDR   , PROM_SPACE  ,  R,   C },
	{ FUSE_ROM_ADDR, FUSE_ROM_ADDR, FUSE_ROM_SPACE,  RW,    CI },
	{ NVRAM_ADDR   , NVRAM_ADDR   , NVRAM_SPACE  ,   RW,     CI },
	{ OBIO_PIO0_BASE,     OBIO_PIO0_BASE, PAGE,  RW,     CI },
	{ OBIO_PIO1_BASE,     OBIO_PIO1_BASE, PAGE,  RW,     CI },
	{ OBIO_SIO      ,     OBIO_SIO,       PAGE,  RW,     CI },
	{ OBIO_TAS      ,     OBIO_TAS,       PAGE,  RW,     CI },
	{ OBIO_CLOCK0   ,     OBIO_CLOCK0,    PAGE,  RW,     CI },
	{ INT_ST_MASK0  ,     INT_ST_MASK0,   PAGE,  RW,     CI },
	{ SOFT_INT0     ,     SOFT_INT0,      PAGE,  W,     CI },
	{ SOFT_INT_FLAG0,     SOFT_INT_FLAG0, PAGE,  RW,     CI },
	{ RESET_CPU0    ,     RESET_CPU0,     PAGE,  RW,     CI },
#if 0
	{ EXT_A_ADDR    ,     EXT_A_ADDR,     EXT_A_SPACE,  RW,     CI },
	{ EXT_B_ADDR    ,     EXT_B_ADDR,     EXT_B_SPACE,  RW,     CI },
	{ PC_BASE       ,     PC_BASE,        PC_SPACE,    RW,     CI },
#endif
	{ MROM_ADDR     ,     MROM_ADDR,      MROM_SPACE, R,      CI },
	{ BMAP_START    ,     BMAP_START,     BMAP_SPACE, RW,     CI },
	{ BMAP_PALLET0  ,     BMAP_PALLET0,   PAGE,       RW,     CI },
	{ BMAP_PALLET1  ,     BMAP_PALLET1,   PAGE,       RW,     CI },
	{ BMAP_PALLET2  ,     BMAP_PALLET2,   PAGE,       RW,     CI },
	{ BOARD_CHECK_REG,    BOARD_CHECK_REG,PAGE,       RW,     CI },
	{ BMAP_CRTC,          BMAP_CRTC,      PAGE,      RW,     CI },
	{ SCSI_ADDR,          SCSI_ADDR,      PAGE,      RW,     C},
	{ LANCE_ADDR,         LANCE_ADDR,     PAGE,      RW,     CI },
#endif
	{ 0,           0,          0xffffffff,             0,      0  },
};

pmap_table_t 
pmap_table_build(void)
{
	return luna88k_board_table;
}
