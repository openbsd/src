/*	$OpenBSD: pdc.c,v 1.1.1.1 1998/06/23 18:46:42 mickey Exp $	*/

/*
 * Copyright 1996 1995 by Open Software Foundation, Inc.   
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */
/*
 * Copyright (c) 1990 mt Xinu, Inc.  All rights reserved.
 * Copyright (c) 1990 University of Utah.  All rights reserved.
 *
 * This file may be freely distributed in any form as long as
 * this copyright notice is included.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	Utah $Hdr: pdc.c 1.8 92/03/14$
 */

#include <sys/time.h>
#include "libsa.h"
#include <sys/reboot.h>
#include <sys/disklabel.h>

#include "dev_hppa.h"

#include <machine/pdc.h>
#include <machine/iodc.h>
#include <machine/iomod.h>
#include <machine/nvm.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))

/*
 * Interface routines to initialize and access the PDC.
 */

int (*pdc)();
int	pdcbuf[64] __attribute ((aligned(8)));		/* PDC return buffer */
struct	stable_storage sstor;	/* contents of Stable Storage */
int	sstorsiz;		/* size of Stable Storage */
unsigned int rstaddr;
struct bootdata bd;
int bdsize = sizeof(struct bootdata);
unsigned int chasdata;

extern unsigned int howto, bootdev;

/*
 * Initialize PDC and related variables.
 */
void
pdc_init()
{
	int err;

	/*
	 * Initialize important global variables (defined above).
	 */
	pdc = PAGE0->mem_pdc;

	err = (*pdc)(PDC_STABLE, PDC_STABLE_SIZE, pdcbuf, 0, 0);
	if (err >= 0) {
		sstorsiz = MIN(pdcbuf[0],sizeof(sstor));
		err = (*pdc)(PDC_STABLE, PDC_STABLE_READ, 0, &sstor, sstorsiz);
	}

	/*
	 * Now that we (may) have an output device, if we encountered
	 * an error reading Stable Storage (above), let them know.
	 */
	if (err)
		printf("Stable storage PDC_STABLE Read Ret'd %d\n", err);

	/*
	 * Clear the FAULT light (so we know when we get a real one)
	 */
	chasdata = PDC_OSTAT(PDC_OSTAT_BOOT) | 0xCEC0;
	(void) (*pdc)(PDC_CHASSIS, PDC_CHASSIS_DISP, chasdata);
}

/*
 * Read in `bootdev' and `howto' from Non-Volatile Memory.
 */
void
getbinfo()
{
	int err;

	/*
	 * Try to read bootdata from NVM through PDC.
	 * If successful, set `howto' and `bootdev'.
	 */
	if ((err = (*pdc)(PDC_NVM, PDC_NVM_READ, NVM_BOOTDATA, &bd, bdsize)) < 0) {
		/*
		 * First, determine if this machine has Non-Volatile Memory.
		 * If not, just return (until we come up with a new plan)!
		 */
		if (err == -1)		/* Nonexistent procedure */
			return;
		printf("NVM bootdata Read ret'd %d\n", err);
	} else {
		if (bd.cksum == NVM_BOOTCKSUM(bd)) {
			/*
			 * The user may override the PDC auto-boot, setting
			 * an interactive boot.  We give them this right by
			 * or'ing the bootaddr flags into `howto'.
			 */
			howto |= bd.flags;
			bootdev = bd.device;
		} else {
			printf("NVM bootdata Bad Checksum (%x)\n", bd.cksum);
		}
	}

	/*
	 * Reset the bootdata to defaults (if necessary).
	 */
	if (bd.flags != RB_AUTOBOOT || bd.device != 0) {
		bd.flags = RB_AUTOBOOT;
		bd.device = 0;
		bd.cksum = NVM_BOOTCKSUM(bd);
		if ((err = (*pdc)(PDC_NVM, PDC_NVM_WRITE, NVM_BOOTDATA,
				  &bd, bdsize)) < 0)
			printf("NVM bootdata Write ret'd %d\n", err);
	}
}

/*
 * Generic READ/WRITE through IODC.  Takes pointer to PDC device
 * information, returns (positive) number of bytes actually read or
 * the (negative) error condition, or zero if at "EOF".
 */
int
iodc_rw(maddr, daddr, count, func, pzdev)
	char * maddr; /* io->i_ma = labelbuf */
	unsigned int daddr;
	unsigned int count;   /* io->i_cc = DEV_BSIZE */
	int func;      
	struct pz_device *pzdev;
{
	register int	offset;
	register int	xfer_cnt = 0;
	register int	ret;

	if (pzdev == 0)		/* default: use BOOT device */ 
		pzdev = &PAGE0->mem_boot;
	
	if (pzdev->pz_iodc_io == 0)
		return(-1);

	/*
	 * IODC arguments are constrained in a number of ways.  If the
	 * request doesn't fit one or more of these constraints, we have
	 * to do the transfer to a buffer and copy it.
	 */
	if ((((int)maddr)&(MINIOSIZ-1))||(count&IOPGOFSET)||(daddr&IOPGOFSET))
	    for (; count > 0; count -= ret, maddr += ret, daddr += ret) {
		offset = daddr & IOPGOFSET;
		if ((ret = (*pzdev->pz_iodc_io)(pzdev->pz_hpa,
					(func == F_READ)? IODC_IO_BOOTIN:
							  IODC_IO_BOOTOUT,
					pzdev->pz_spa, pzdev->pz_layers, pdcbuf,
					daddr - offset, btbuf, BTIOSIZ,
					BTIOSIZ)) < 0)
			return (ret);
		if ((ret = pdcbuf[0]) == 0)
			break;
		if ((ret -= offset) > count)
			ret = count;
		bcopy(btbuf + offset, maddr, ret);
		xfer_cnt += ret;
	    }
	else 
		for (; count > 0; count -= ret, maddr += ret, daddr += ret) {
			if ((offset = count) > MAXIOSIZ)
				offset = MAXIOSIZ;
			if ((ret = (*pzdev->pz_iodc_io)(pzdev->pz_hpa,
					(func == F_READ)? IODC_IO_BOOTIN:
							  IODC_IO_BOOTOUT,
					pzdev->pz_spa, pzdev->pz_layers,
					pdcbuf, daddr, maddr, offset,
					count)) < 0)
				return (ret);
			if ((ret = pdcbuf[0]) == 0)
				break;
			xfer_cnt += ret;
		}

	return (xfer_cnt);
}
