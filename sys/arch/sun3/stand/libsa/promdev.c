/*	$NetBSD: promdev.c,v 1.7 1996/01/29 23:41:10 gwr Exp $ */

/*
 * Copyright (c) 1995 Gordon W. Ross
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
 */

#include <sys/param.h>
#include <machine/mon.h>
#include <machine/pte.h>
#include <machine/saio.h>

#include "stand.h"
#include "dvma.h"

extern void set_pte __P((int, int));
extern int debug;

static int promdev_inuse;

static char *
prom_mapin(u_long physaddr, int length, int maptype);

/*
 * Note: caller sets the fields:
 *	si->si_boottab
 *	si->si_ctlr
 *	si->si_unit
 *	si->si_boff
 */

int
prom_iopen(si)
	struct saioreq *si;
{
	struct boottab *ops;
	struct devinfo *dip;
	int	error;

	if (promdev_inuse)
		return(EMFILE);

	ops = si->si_boottab;
	dip = ops->b_devinfo;

#ifdef DEBUG_PROM
	if (debug) {
		printf("Boot device type: %s\n", ops->b_desc);
		printf("d_devbytes=%d\n", dip->d_devbytes);
		printf("d_dmabytes=%d\n", dip->d_dmabytes);
		printf("d_localbytes=%d\n", dip->d_localbytes);
		printf("d_stdcount=%d\n", dip->d_stdcount);
		printf("d_stdaddrs[%d]=%x\n", si->si_ctlr,
			   dip->d_stdaddrs[si->si_ctlr]);
		printf("d_devtype=%d\n", dip->d_devtype);
		printf("d_maxiobytes=%d\n", dip->d_maxiobytes);
	}
#endif

	if (si->si_ctlr > dip->d_stdcount) {
		printf("Invalid controller number\n");
		return(ENXIO);
	}

	dvma_init();

	if (dip->d_devbytes) {
		si->si_devaddr = prom_mapin(dip->d_stdaddrs[si->si_ctlr],
			dip->d_devbytes, dip->d_devtype);
#ifdef	DEBUG_PROM
		if (debug)
			printf("prom_iopen: devaddr=0x%x pte=0x%x\n",
				   si->si_devaddr, get_pte(si->si_devaddr));
#endif
	}

	if (dip->d_dmabytes) {
		int addr, size;
		/* try page-aligned address... */
		size = dip->d_dmabytes + NBPG;
		addr = (int) dvma_alloc(size);
		addr = sun3_round_page(addr);
		si->si_dmaaddr = (char*) addr;
#ifdef	DEBUG_PROM
		if (debug)
			printf("prom_iopen: dmaaddr=0x%x\n", si->si_dmaaddr);
#endif
	}

	if (dip->d_localbytes) {
		si->si_devdata = alloc(dip->d_localbytes);
#ifdef	DEBUG_PROM
		if (debug)
			printf("prom_iopen: devdata=0x%x\n", si->si_devdata);
#endif
	}

	/* OK, call the PROM device open routine. */
#ifdef	DEBUG_PROM
	if (debug)
		printf("prom_iopen: calling prom open...\n");
#endif
	error = (*ops->b_open)(si);
	if (error != 0) {
		printf("prom_iopen: \"%s\" error=%d\n",
			   ops->b_desc, error);
		return (ENXIO);
	}
#ifdef	DEBUG_PROM
	if (debug)
		printf("prom_iopen: prom open returned %d\n", error);
#endif

	promdev_inuse++;
	return (0);
}

void
prom_iclose(si)
	struct saioreq *si;
{
	struct boottab *ops;
	struct devinfo *dip;

	if (promdev_inuse == 0)
		return;

	ops = si->si_boottab;
	dip = ops->b_devinfo;

#ifdef	DEBUG_PROM
	if (debug)
		printf("prom_iclose: calling prom close...\n");
#endif
	(*ops->b_close)(si);

	promdev_inuse = 0;
}

struct mapinfo {
	int maptype;
	int pgtype;
	int base;
};

static struct mapinfo
prom_mapinfo[] = {
	/* On-board memory, I/O */
	{ MAP_MAINMEM,   PGT_OBMEM, 0 },
	{ MAP_OBIO,      PGT_OBIO,  0 },
	/* Multibus adapter */
	{ MAP_MBMEM,     PGT_VME_D16, 0xFF000000 },
	{ MAP_MBIO,      PGT_VME_D16, 0xFFFF0000 },
	/* VME A16 */
	{ MAP_VME16A16D, PGT_VME_D16, 0xFFFF0000 },
	{ MAP_VME16A32D, PGT_VME_D32, 0xFFFF0000 },
	/* VME A24 */
	{ MAP_VME24A16D, PGT_VME_D16, 0xFF000000 },
	{ MAP_VME24A32D, PGT_VME_D32, 0xFF000000 },
	/* VME A32 */
	{ MAP_VME32A16D, PGT_VME_D16, 0 },
	{ MAP_VME32A32D, PGT_VME_D32, 0 },
};
static prom_mapinfo_cnt = sizeof(prom_mapinfo) / sizeof(prom_mapinfo[0]);

/* The virtual address we will use for PROM device mappings. */
static int prom_devmap = MONSHORTSEG;

static char *
prom_mapin(physaddr, length, maptype)
	u_long physaddr;
	int length, maptype;
{
	int i, pa, pte, va;

	if (length > (4*NBPG))
		panic("prom_mapin: length=%d\n", length);

	for (i = 0; i < prom_mapinfo_cnt; i++)
		if (prom_mapinfo[i].maptype == maptype)
			goto found;
	panic("prom_mapin: invalid maptype %d\n", maptype);
found:

	pte = prom_mapinfo[i].pgtype;
	pte |= PG_PERM;
	pa = prom_mapinfo[i].base;
	pa += physaddr;
	pte |= PA_PGNUM(pa);

	va = prom_devmap;
	do {
		set_pte(va, pte);
		va += NBPG;
		pte += 1;
		length -= NBPG;
	} while (length > 0);
	return ((char*)(prom_devmap | (pa & PGOFSET)));
}
