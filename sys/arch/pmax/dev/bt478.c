/*	$NetBSD: bt478.c,v 1.2 1995/11/25 10:38:42 mellon Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	@(#)bt478.c	8.1 (Berkeley) 6/10/93
 */

/* 
 *  devGraphics.c --
 *
 *     	This file contains machine-dependent routines for the graphics device.
 *
 *	Copyright (C) 1989 Digital Equipment Corporation.
 *	Permission to use, copy, modify, and distribute this software and
 *	its documentation for any purpose and without fee is hereby granted,
 *	provided that the above copyright notice appears in all copies.  
 *	Digital Equipment Corporation makes no representations about the
 *	suitability of this software for any purpose.  It is provided "as is"
 *	without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/dev/ds3100.md/RCS/devGraphics.c,
 *	v 9.2 90/02/13 22:16:24 shirriff Exp  SPRITE (DECWRL)";
 */


#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/select.h>

#include <machine/machConst.h>
#include <machine/pmioctl.h>

#include <machine/fbio.h>
#include <machine/fbvar.h>

#include <pmax/dev/bt478.h>
#include <pmax/pmax/kn01.h>

/*
 * Forward references.
 */
void bt478RestoreCursorColor();
void bt478CursorColor();	/* qvss ioctl interface uses this */
/*static*/ void bt478InitColorMap();
int bt478GetColorMap();
static void bt478VDACInit();
int bt478LoadColorMap();

extern int pmax_boardtype;
extern u_short defCursor[32];

static u_char	bg_RGB[3];	/* background color for the cursor */
static u_char	fg_RGB[3];	/* foreground color for the cursor */

/* Initialize the VDAC. */

int
bt478init(fi)
	struct fbinfo *fi;
{
	register VDACRegs *vdac = (VDACRegs *)(fi -> fi_vdac);

	/*
	 *
	 * Initialize the VDAC
	 */
	vdac->overWA = 0x04; MachEmptyWriteBuffer();
	vdac->over = 0x00; MachEmptyWriteBuffer();
	vdac->over = 0x00; MachEmptyWriteBuffer();
	vdac->over = 0x00; MachEmptyWriteBuffer();
	vdac->overWA = 0x08; MachEmptyWriteBuffer();
	vdac->over = 0x00; MachEmptyWriteBuffer();
	vdac->over = 0x00; MachEmptyWriteBuffer();
	vdac->over = 0x7f; MachEmptyWriteBuffer();
	vdac->overWA = 0x0c; MachEmptyWriteBuffer();
	vdac->over = 0xff; MachEmptyWriteBuffer();
	vdac->over = 0xff; MachEmptyWriteBuffer();
	vdac->over = 0xff; MachEmptyWriteBuffer();

	/* Initialize the cursor position... */
	fi -> fi_cursor.width = 16;
	fi -> fi_cursor.height = 16;
	fi -> fi_cursor.x = 0;
	fi -> fi_cursor.y = 0;

	/*
	 * Initialize the color map and the screen.
	 */
	bt478InitColorMap(fi);
	return (1);
}

/* restore the color of the cursor. */

void
bt478RestoreCursorColor(fi)
	struct fbinfo *fi;
{
	register VDACRegs *vdac = (VDACRegs *)(fi -> fi_vdac);
	register int i;

	vdac->overWA = 0x04;
	MachEmptyWriteBuffer();
	for (i = 0; i < 3; i++) {  
		vdac->over = bg_RGB[i];
		MachEmptyWriteBuffer();
	}

	vdac->overWA = 0x08;
	MachEmptyWriteBuffer();
	vdac->over = 0x00;
	MachEmptyWriteBuffer();
	vdac->over = 0x00;
	MachEmptyWriteBuffer();
	vdac->over = 0x7f;
	MachEmptyWriteBuffer();

	vdac->overWA = 0x0c;
	MachEmptyWriteBuffer();
	for (i = 0; i < 3; i++) {
		vdac->over = fg_RGB[i];
		MachEmptyWriteBuffer();
	}
}

/* Set the color of the cursor. */

void
bt478CursorColor (fi, color)
	struct fbinfo *fi;
	unsigned int color[];
{
	register int i, j;

	for (i = 0; i < 3; i++)
		bg_RGB[i] = (u_char)(color[i] >> 8);

	for (i = 3, j = 0; i < 6; i++, j++)
		fg_RGB[j] = (u_char)(color[i] >> 8);

	bt478RestoreCursorColor (fi);
}

/* Set the cursor foreground color to zero (used by pm.c
   for screen blanking). */

void
bt478BlankCursor(fi)
	struct fbinfo *fi;
{
	register VDACRegs *vdac = (VDACRegs *)(fi -> fi_vdac);
	register int i;

	vdac->overWA = 0x0c;
	MachEmptyWriteBuffer();
	for (i = 0; i < 3; i++) {
		vdac->over = 0;
		MachEmptyWriteBuffer();
	}
}

/* Initialize the color map. */

/*static*/
void
bt478InitColorMap (fi)
	struct fbinfo *fi;
{
	register VDACRegs *vdac = (VDACRegs *)(fi -> fi_vdac);
	register int i;

	*(volatile char *)MACH_PHYS_TO_UNCACHED
		(KN01_PHYS_COLMASK_START) = 0xff;	/* XXX */
	MachEmptyWriteBuffer();

	if (fi -> fi_type.fb_depth == 1) {
		vdac->mapWA = 0; MachEmptyWriteBuffer();
		for (i = 0; i < 256; i++) {
			((u_char *)(fi -> fi_cmap_bits)) [i * 3] = 0;
			((u_char *)(fi -> fi_cmap_bits)) [i * 3 + 1]
				= (i < 128) ? 0x00 : 0xff;
			((u_char *)(fi -> fi_cmap_bits)) [i * 3 + 2] = 0;
			vdac->map = 0;
			MachEmptyWriteBuffer();
			vdac->map = (i < 128) ? 0x00 : 0xff;
			MachEmptyWriteBuffer();
			vdac->map = 0;
			MachEmptyWriteBuffer();
		}
	} else {
		vdac->mapWA = 0; MachEmptyWriteBuffer();
		((u_char *)(fi -> fi_cmap_bits)) [0] = 0;
		((u_char *)(fi -> fi_cmap_bits)) [1] = 0;
		((u_char *)(fi -> fi_cmap_bits)) [2] = 0;
		vdac->map = 0;
		MachEmptyWriteBuffer();
		vdac->map = 0;
		MachEmptyWriteBuffer();
		vdac->map = 0;
		MachEmptyWriteBuffer();

		for (i = 1; i < 256; i++) {
			((u_char *)(fi -> fi_cmap_bits)) [i * 3] = 0xff;
			((u_char *)(fi -> fi_cmap_bits)) [i * 3 + 1] = 0xff;
			((u_char *)(fi -> fi_cmap_bits)) [i * 3 + 2] = 0xff;
			vdac->map = 0xff;
			MachEmptyWriteBuffer();
			vdac->map = 0xff;
			MachEmptyWriteBuffer();
			vdac->map = 0xff;
			MachEmptyWriteBuffer();
		}
	}

	for (i = 0; i < 3; i++) {
		bg_RGB[i] = 0x00;
		fg_RGB[i] = 0xff;
	}
	bt478RestoreCursorColor();
}

/* Load the color map. */

int
bt478LoadColorMap(fi, bits, index, count)
	struct fbinfo *fi;
	caddr_t bits;
	int index, count;
{
	register VDACRegs *vdac = (VDACRegs *)(fi -> fi_vdac);
	u_char *cmap_bits;
	u_char *cmap;
	int i;

	if (index > 256 || index < 0 || index + count > 256)
		return EINVAL;

	cmap_bits = (u_char *)bits;
	cmap = (u_char *)(fi -> fi_cmap_bits) + index * 3;

	vdac->mapWA = index; MachEmptyWriteBuffer();
	for (i = 0; i < count; i++) {
		cmap [(i + index) * 3]
			= vdac->map = cmap_bits [i * 3];
		MachEmptyWriteBuffer();
		cmap [(i + index) * 3 + 1]
			= vdac->map = cmap_bits [i * 3 + 1];
		MachEmptyWriteBuffer();
		cmap [(i + index) * 3 + 2]
			= vdac -> map = cmap_bits [i * 3 + 2];
		MachEmptyWriteBuffer();
	}
	return 0;
}

/* Copy out count entries of the colormap starting at index into bits. */

int
bt478GetColorMap(fi, bits, index, count)
	struct fbinfo *fi;
	caddr_t bits;
	int index, count;
{
	u_char *cmap_bits;
	u_char *cmap;

	if (index > 256 || index < 0 || index + count > 256)
		return EINVAL;

	cmap_bits = (u_char *)bits;
	cmap = (u_char *)(fi -> fi_cmap_bits) + index * 3;

	bcopy (cmap, cmap_bits, count * 3);
	return 0;
}
