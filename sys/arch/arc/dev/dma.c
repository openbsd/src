/*	$OpenBSD: dma.c,v 1.1.1.1 1996/06/24 09:07:19 pefo Exp $	*/
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)rz.c	8.1 (Berkeley) 7/29/93
 *      $Id: dma.c,v 1.1.1.1 1996/06/24 09:07:19 pefo Exp $
 */

/*
 * PICA system dma driver. Handles resource allocation and
 * logical (viritual) address remaping. 
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <machine/pte.h>
#include <machine/pio.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <arc/pica/pica.h>
#include <arc/dev/dma.h>


extern vm_map_t phys_map;

#define dma_pte_to_pa(x)	(((x) - first_dma_pte) * PICA_DMA_PAGE_SIZE)

dma_pte_t *free_dma_pte;	/* Pointer to free dma pte list */
dma_pte_t *first_dma_pte;	/* Pointer to first dma pte */

/*
 *  Initialize the dma mapping register area and pool.
 */
void
picaDmaInit()
{
	int map = PICA_TL_BASE;

	MachFlushCache();	/* Make shure no map entries are cached */

	bzero((char *)map, PICA_TL_SIZE);
	free_dma_pte = (dma_pte_t *)map;
	first_dma_pte = (dma_pte_t *)map;
	free_dma_pte->queue.next = NULL;
	free_dma_pte->queue.size = PICA_TL_SIZE / sizeof(dma_pte_t);

	out32(PICA_SYS_TL_BASE, UNCACHED_TO_PHYS(map));
	out32(PICA_SYS_TL_LIMIT, PICA_TL_SIZE);
	out32(PICA_SYS_TL_IVALID, 0);
}

/*
 *  Allocate an array of 'size' dma pte entrys.
 *  Return address to first pte.
 */
void
picaDmaTLBAlloc(dma_softc_t *dma)
{
	dma_pte_t *list;
	dma_pte_t *found;
	int size;
	int s;

	found = NULL;
	size = dma->pte_size;
	do {
		list = (dma_pte_t *)&free_dma_pte;
		s = splhigh();
		while(list) {
			if(list->queue.next->queue.size >= size) {
				found = list->queue.next;
				break;
			}
		}
/*XXX Wait for release wakeup */
	} while(found == NULL);
	if(found->queue.size == size) {
		list->queue.next = found->queue.next;
	}
	else {
		list->queue.next = found + size;
		list = found + size;
		list->queue.next = found->queue.next;
		list->queue.size = found->queue.size - size;
	}
	splx(s);
	dma->pte_base = found;
	dma->dma_va = dma_pte_to_pa(found);
}

/*
 *  Free an array of dma pte entrys.
 */
void
picaDmaTLBFree(dma_softc_t *dma)
{
	dma_pte_t *list;
	dma_pte_t *entry;
	int	   size;
	int s;

	s = splhigh();
	entry = dma->pte_base;
	size = dma->pte_size;
	entry->queue.next = NULL;
	entry->queue.size = size;
	if(free_dma_pte == NULL || entry < free_dma_pte) {
		list = entry;
		list->queue.next = free_dma_pte;
		free_dma_pte = entry;
	}
	else {
		list = free_dma_pte;
		while(list < entry && list->queue.next != NULL) {
			if(list + list->queue.size == entry) {
				list->queue.size += size;
				break;
			}
			else if(list->queue.next == NULL) {
				list->queue.next = entry;
				break;
			}
			else
				list = list->queue.next;
		}
	}
	if(list->queue.next != NULL) {
		if(list + list->queue.size == list->queue.next) {
			list->queue.size += list->queue.next->queue.size;
			list->queue.next = list->queue.next->queue.next;
		}
	}
	splx(s);
/*XXX Wakeup waiting */
}

/*
 *  Map up a viritual address space in dma space given by
 *  the dma control structure and invalidate dma TLB cache.
 */

picaDmaTLBMap(dma_softc_t *sc)
{
	vm_offset_t pa;
	vm_offset_t va;
	dma_pte_t *dma_pte;
	int nbytes;

	va = sc->next_va - sc->dma_va;
	dma_pte = sc->pte_base + (va / PICA_DMA_PAGE_SIZE);
	nbytes = dma_page_round(sc->next_size + dma_page_offs(va));
	va = sc->req_va;
	while(nbytes > 0) {
		if(va < VM_MIN_KERNEL_ADDRESS) {
			pa = CACHED_TO_PHYS(va);
		}
		else {
			pa = pmap_extract(vm_map_pmap(phys_map), va);
		}
		pa &= PICA_DMA_PAGE_NUM;
		if(pa == 0)
			panic("picaDmaTLBMap: null page frame");
		dma_pte->entry.lo_addr = pa;
		dma_pte->entry.hi_addr = 0;
		dma_pte++;
		va += PICA_DMA_PAGE_SIZE;
		nbytes -= PICA_DMA_PAGE_SIZE;
	}
}

/*
 *  Start local dma channel.
 */
void
picaDmaStart(sc, addr, size, datain)
	struct dma_softc *sc;
	char	*addr;
	size_t  size;
	int     datain;
{
	int mode;
	pDmaReg regs = sc->dma_reg;

	/* Halt DMA */
	regs->dma_enab = 0;
	regs->dma_mode = 0;

	/* Remap request space va into dma space va */

	sc->req_va = (int)addr;
	sc->next_va = sc->dma_va + dma_page_offs(addr);
	sc->next_size = size;

	/* Map up the request viritual dma space */
	picaDmaTLBMap(sc);
	out32(PICA_SYS_TL_IVALID, 0);	/* Flush dma map cache */

	/* Load new transfer parameters */
	regs->dma_addr = sc->next_va;
	regs->dma_count = sc->next_size;
	regs->dma_mode = sc->mode & PICA_DMA_MODE;

	sc->sc_active = 1;
	if(datain == DMA_FROM_DEV) {
		sc->mode &= ~DMA_DIR_WRITE;
		regs->dma_enab = PICA_DMA_ENAB_RUN | PICA_DMA_ENAB_READ;
	}
	else {
		sc->mode |= DMA_DIR_WRITE;
		regs->dma_enab = PICA_DMA_ENAB_RUN | PICA_DMA_ENAB_WRITE;
	}
	wbflush();
}

/*
 *  Set up DMA mapper for external dma.
 *  Used by ISA dma and SONIC
 */
void
picaDmaMap(sc, addr, size, offset)
	struct dma_softc *sc;
	char	*addr;
	size_t  size;
	int	offset;
{
	/* Remap request space va into dma space va */

	sc->req_va = (int)addr;
	sc->next_va = sc->dma_va + dma_page_offs(addr) + offset;
	sc->next_size = size;

	/* Map up the request viritual dma space */
	picaDmaTLBMap(sc);
}

/*
 *  Prepare for new dma by flushing
 */
void
picaDmaFlush(sc, addr, size, datain)
	struct dma_softc *sc;
	char	*addr;
	size_t  size;
	int     datain;
{
	out32(PICA_SYS_TL_IVALID, 0);	/* Flush dma map cache */
}

/*
 *  Stop/Reset a DMA channel
 */
void
picaDmaReset(dma_softc_t *sc)
{
	pDmaReg regs = sc->dma_reg;

	/* Halt DMA */
	regs->dma_enab = 0;
	regs->dma_mode = 0;
	sc->sc_active = 0;
}

/*
 *  End dma operation, return byte count left.
 */
int
picaDmaEnd(dma_softc_t *sc)
{
	pDmaReg regs = sc->dma_reg;
	int res;

	res = regs->dma_count = sc->next_size;

	/* Halt DMA */
	regs->dma_enab = 0;
	regs->dma_mode = 0;
	sc->sc_active = 0;

	return res;
}

/*
 *  Null call rathole!
 */
void
picaDmaNull(dma_softc_t *sc)
{
	pDmaReg regs = sc->dma_reg;

	printf("picaDmaNull called\n");
}

/*
 *  dma_init..
 *	Called from asc to set up dma
 */
void
asc_dma_init(dma_softc_t *sc)
{
	sc->reset = picaDmaReset;
	sc->enintr = picaDmaNull;
	sc->start = picaDmaStart;
	sc->map = picaDmaMap;
	sc->isintr = (int(*)())picaDmaNull;
	sc->intr = (int(*)())picaDmaNull;
	sc->end = picaDmaEnd;

	sc->dma_reg = (pDmaReg)PICA_SYS_DMA0_REGS;
	sc->pte_size = 32;
	sc->mode = PICA_DMA_MODE_160NS | PICA_DMA_MODE_16;
	picaDmaTLBAlloc(sc);
}
/*
 *  dma_init..
 *	Called from fdc to set up dma
 */
void
fdc_dma_init(dma_softc_t *sc)
{
	sc->reset = picaDmaReset;
	sc->enintr = picaDmaNull;
	sc->start = picaDmaStart;
	sc->map = picaDmaMap;
	sc->isintr = (int(*)())picaDmaNull;
	sc->intr = (int(*)())picaDmaNull;
	sc->end = picaDmaEnd;

	sc->dma_reg = (pDmaReg)PICA_SYS_DMA1_REGS;
	sc->pte_size = 32;
	sc->mode = PICA_DMA_MODE_160NS | PICA_DMA_MODE_8;
	picaDmaTLBAlloc(sc);
}
/*
 *  dma_init..
 *	Called from sonic to set up dma
 */
void
sn_dma_init(dma_softc_t *sc, int pages)
{
	sc->reset = picaDmaNull;
	sc->enintr = picaDmaNull;
	sc->start = picaDmaFlush;
	sc->map = picaDmaMap;
	sc->isintr = (int(*)())picaDmaNull;
	sc->intr = (int(*)())picaDmaNull;
	sc->end = (int(*)())picaDmaNull;

	sc->dma_reg = (pDmaReg)NULL;
	sc->pte_size = pages;
	sc->mode = 0;
	picaDmaTLBAlloc(sc);
}
