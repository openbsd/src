/*	$NetBSD: cgeightreg.h,v 1.4 1994/11/20 20:52:03 deraadt Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed by Theo de Raadt.
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
 */

/*
 * cgeight display registers.  Much like bwtwo registers, except that
 * there is a Brooktree Video DAC in there (so we also use btreg.h).
 */

/* offsets */
#define CG8REG_CMAP	0x200000
#define CG8REG_P4REG	0x300000
#define	CG8REG_OVERLAY	0x400000
#define	CG8REG_ENABLE	0x600000
#define	CG8REG_COLOUR	0x800000
#define	CG8REG_END	0xa00000

/* same, but for gdb */
struct cgeight_all {
	char	ba_nothing[0x200000];
	struct	bt_regs ba_btreg;	/* Brooktree registers */
	char	ba_xxx1[0x100000-sizeof(struct bt_regs)];
	char	ba_pfourreg[0x100000];
	char	ba_overlay[0x200000];
	char	ba_enable[0x200000];
	char	ba_color[0x200000];
};
