/*	$OpenBSD: sfbvar.h,v 1.2 1996/07/29 23:02:18 niklas Exp $	*/
/*	$NetBSD: sfbvar.h,v 1.1 1996/05/01 21:15:51 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <machine/sfbreg.h>
#include <dev/rcons/raster.h>
#include <alpha/wscons/wsconsvar.h>
#include <alpha/wscons/wscons_raster.h>

struct sfb_devconfig;
struct fbcmap;
struct fbcursor;
struct fbcurpos;

struct sfb_devconfig {
	vm_offset_t dc_vaddr;		/* memory space virtual base address */
	vm_offset_t dc_paddr;		/* memory space physical base address */
	vm_offset_t dc_size;		/* size of slot memory */

	int	    dc_wid;		/* width of frame buffer */
	int	    dc_ht;		/* height of frame buffer */
	int	    dc_depth;		/* depth, bits per pixel */
	int	    dc_rowbytes;	/* bytes in a FB scan line */

	vm_offset_t dc_videobase;	/* base of flat frame buffer */

	struct raster	dc_raster;	/* raster description */
	struct rcons	dc_rcons;	/* raster blitter control info */

	int	    dc_blanked;		/* currently has video disabled */
};
	
struct sfb_softc {
	struct device sc_dev;

	struct sfb_devconfig *sc_dc;	/* device configuration */
};
