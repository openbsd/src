/* $OpenBSD: vgareg.h,v 1.1 2000/11/15 20:17:38 aaron Exp $ */
/* $NetBSD: vgareg.h,v 1.2 1998/05/28 16:48:41 drochner Exp $ */

/*
 * Copyright (c) 1998
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 */

struct reg_vgaattr { /* indexed via port 0x3c0 */
	char palette[16];
	char mode, overscan, colplen, horpixpan;
	char colreset, misc;
};
#define VGA_ATC_INDEX 0
#define VGA_ATC_DATAW 0
#define VGA_ATC_DATAR 1

struct reg_vgats { /* indexed via port 0x3c4 */
	char syncreset, mode, wrplmask, fontsel, memmode;
};
#define VGA_TS_INDEX 4
#define VGA_TS_DATA 5

struct reg_vgagdc { /* indexed via port 0x3ce */
	char setres, ensetres, colorcomp, rotfunc;
	char rdplanesel, mode, misc, colorcare;
	char bitmask;
};
#define VGA_GDC_INDEX 0xe
#define VGA_GDC_DATA 0xf
