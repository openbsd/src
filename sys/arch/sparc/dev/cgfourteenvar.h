/*	$OpenBSD: cgfourteenvar.h,v 1.1 1997/08/08 08:24:50 downsj Exp $	*/
/*	$NetBSD: cgfourteenvar.h,v 1.1 1996/09/30 22:41:03 abrown Exp $ */

/*
 * Copyright (c) 1996 
 *	The President and Fellows of Harvard College. All rights reserved.
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
 *	This product includes software developed by Harvard University and
 *	its contributors.
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
 */

/*
 * Layout of cg14 hardware colormap
 */
union cg14cmap {
	u_char  	cm_map[256][4];	/* 256 R/G/B/A entries (B is high)*/
	u_int32_t   	cm_chip[256];	/* the way the chip gets loaded */
};

/*
 * cg14 hardware cursor colormap
 */
union cg14cursor_cmap {		/* colormap, like bt_cmap, but tiny */
	u_char		cm_map[2][4];	/* 2 R/G/B/A entries */
	u_int32_t	cm_chip[2];	/* 2 chip equivalents */
};

/*
 * cg14 hardware cursor status
 */
struct cg14_cursor {		/* cg14 hardware cursor status */
	short	cc_enable;		/* cursor is enabled */
	struct	fbcurpos cc_pos;	/* position */
	struct	fbcurpos cc_hot;	/* hot-spot */
	struct	fbcurpos cc_size;	/* size of mask & image fields */
	u_int	cc_eplane[32];		/* enable plane */
	u_int	cc_cplane[32];		/* color plane */
	union	cg14cursor_cmap cc_color; /* cursor colormap */
};

/* 
 * per-cg14 variables/state
 */
struct cgfourteen_softc {
	struct	device sc_dev;		/* base device */
	struct	fbdevice sc_fb;		/* frame buffer device */

	struct 	rom_reg	sc_phys;	/* phys address of frame buffer */
#if defined(DEBUG) && defined(CG14_MAP_REGS)
	struct	rom_reg	sc_regphys;	/* phys addr of fb regs; for debug */
#endif
	union	cg14cmap sc_cmap;	/* current colormap */

	struct	cg14_cursor sc_cursor;	/* Hardware cursor state */

	union 	cg14cmap sc_saveclut; 	/* a place to stash PROM state */
	u_int8_t	sc_savexlut[256];
	u_int8_t	sc_savectl;
	u_int8_t	sc_savehwc;

	struct	cg14ctl  *sc_ctl; 	/* various registers */
	struct	cg14curs *sc_hwc;
	struct 	cg14dac	 *sc_dac;
	struct	cg14xlut *sc_xlut;
	struct 	cg14clut *sc_clut1;
	struct	cg14clut *sc_clut2;
	struct	cg14clut *sc_clut3;
	u_int	*sc_clutincr;
};
