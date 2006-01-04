/*	$OpenBSD: macfbvar.h,v 1.1 2006/01/04 20:39:05 miod Exp $	*/
/* $NetBSD: macfbvar.h,v 1.3 2005/01/15 16:00:59 chs Exp $ */
/*
 * Copyright (c) 1998 Matt DeBergalis
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
 *      This product includes software developed by Matt DeBergalis
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

#include <dev/rcons/raster.h>
#include <dev/wscons/wscons_raster.h>

#include <machine/bus.h>

struct macfb_devconfig {
	vaddr_t	dc_vaddr;	/* memory space virtual base address */
	paddr_t	dc_paddr;	/* memory space physical base address */
	psize_t	dc_size;	/* size of slot memory */

	int	dc_offset;	/* offset from dc_vaddr to base of flat fb */

	int	dc_wid;		/* width of frame buffer */
	int	dc_ht;		/* height of frame buffer */
	int	dc_depth;	/* depth of frame buffer */
	int	dc_rowbytes;	/* bytes in fb scan line */

	struct raster dc_raster; /* raster description */
	struct rcons dc_rcons;	/* raster blitter control info */
};

struct macfb_softc {
	struct device sc_dev;
				
	int nscreens;
	struct macfb_devconfig *sc_dc;
};

int	macfb_cnattach(paddr_t);
void	macfb_clear(struct macfb_devconfig *);
