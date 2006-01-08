/*	$OpenBSD: grfvar.h,v 1.19 2006/01/08 20:35:21 miod Exp $	*/
/*	$NetBSD: grfvar.h,v 1.11 1996/08/04 06:03:58 scottr Exp $	*/
/*	$NetBSD: grfioctl.h,v 1.5 1995/07/02 05:26:45 briggs Exp $	*/

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

struct grfmode {
	caddr_t		fbbase;		/* Base of page of frame buffer     */
	u_int32_t	fbsize;		/* Size of frame buffer             */
	u_int16_t	fboff;		/* Offset of frame buffer from base */
	u_int16_t	rowbytes;	/* Screen rowbytes                  */
	u_int16_t	width;		/* Screen width                     */
	u_int16_t	height;		/* Screen height                    */
	u_int16_t	psize;		/* Screen depth                     */
};

#define CARD_NAME_LEN	64

/*
 * State info, per hardware instance.
 */
struct grfbus_softc {
	struct	device	sc_dev;
	nubus_slot	sc_slot;

	bus_addr_t		sc_basepa;	/* base of video space */
	bus_addr_t		sc_fbofs;	/* offset to framebuffer */

	bus_space_tag_t		sc_tag;
	bus_space_handle_t	sc_handle;
	bus_space_handle_t	sc_regh;

	u_int32_t	card_id;	/* DrHW value for nubus cards	*/
	bus_size_t	cli_offset;	/* Offset of byte to clear intr */
					/* for cards where that's suff.  */
	u_int32_t	cli_value;	/* Value to write at cli_offset */
};

/*
 * Attach grf and ite semantics to Mac video hardware.
 */
struct grfbus_attach_args {
	char		*ga_name;	/* name of semantics to attach */
	struct grfmode	ga_grfmode;
	bus_addr_t	ga_phys;
};

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

void	grf_establish(struct grfbus_softc *, struct grfmode *);
int	grfbusprint(void *, const char *);
