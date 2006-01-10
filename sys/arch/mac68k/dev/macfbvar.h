/*	$OpenBSD: macfbvar.h,v 1.4 2006/01/10 21:19:14 miod Exp $	*/
/* $NetBSD: macfbvar.h,v 1.3 2005/01/15 16:00:59 chs Exp $ */
/*	$NetBSD: grfvar.h,v 1.11 1996/08/04 06:03:58 scottr Exp $	*/
/*	$NetBSD: grfioctl.h,v 1.5 1995/07/02 05:26:45 briggs Exp $	*/
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
/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 * from: Utah $Hdr: grfvar.h 1.9 91/01/21$
 *
 *	@(#)grfvar.h	7.3 (Berkeley) 5/7/91
 */

/*
 * Nubus image data structure.  This is the equivalent of a PixMap in
 * MacOS programming parlance.  One of these structures exists for each
 * video mode that a quickdraw compatible card can fit in.
 */
struct image_data {
	u_int32_t	size;
	u_int32_t	offset;
	u_int16_t	rowbytes;
	u_int16_t	top;
	u_int16_t	left;
	u_int16_t	bottom;
	u_int16_t	right;
	u_int16_t	version;
	u_int16_t	packType;
	u_int32_t	packSize;
	u_int32_t	hRes;
	u_int32_t	vRes;
	u_int16_t	pixelType;
	u_int16_t	pixelSize;	
	u_int16_t	cmpCount;
	u_int16_t	cmpSize;
	u_int32_t	planeBytes;
};

#define VID_PARAMS		1
#define VID_TABLE_OFFSET	2
#define VID_PAGE_CNT		3
#define VID_DEV_TYPE		4

struct macfb_devconfig {
	vaddr_t		dc_vaddr;	/* memory space virtual base address */
	paddr_t		dc_paddr;	/* memory space physical base address */
	psize_t		dc_size;	/* size of slot memory */

	int		dc_offset;	/* offset to base of flat fb */

	int		dc_wid;		/* width of frame buffer */
	int		dc_ht;		/* height of frame buffer */
	int		dc_depth;	/* depth of frame buffer */
	int		dc_rowbytes;	/* bytes in fb scan line */

	/* rasops information */
	struct rasops_info dc_ri;

	/* wsdisplay information */
	struct wsscreen_descr wsd;
	int		nscreens;
};

struct macfb_softc {
	struct device		sc_dev;
				
	nubus_slot		sc_slot;

	bus_addr_t		sc_basepa;	/* base of video space */
	bus_addr_t		sc_fbofs;	/* offset to framebuffer */

	bus_space_tag_t		sc_tag;
	bus_space_handle_t	sc_handle;
	bus_space_handle_t	sc_regh;

	u_int32_t	card_id;	/* DrHW value for nubus cards	*/
	bus_size_t	cli_offset;	/* Offset of byte to clear intr */
					/* for cards where that's suff.  */
	u_int32_t	cli_value;	/* Value to write at cli_offset */

	struct macfb_devconfig	*sc_dc;
};

void	macfb_attach_common(struct macfb_softc *, struct macfb_devconfig *);
int	macfb_cnattach(void);
