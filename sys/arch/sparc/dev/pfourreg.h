/*	$OpenBSD: pfourreg.h,v 1.5 2003/06/02 18:47:58 deraadt Exp $	*/
/*	$NetBSD: pfourreg.h,v 1.1 1996/02/27 22:09:36 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
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
 * pfour framebuffer registers.
 */

/* Offset of bwtwo framebuffer from pfour register */
#define PFOUR_BW_OFF		0x00100000

/* Offsets for color framebuffers */
#define PFOUR_COLOR_OFF_OVERLAY	0x00100000
#define PFOUR_COLOR_OFF_ENABLE	0x00300000
#define PFOUR_COLOR_OFF_COLOR	0x00500000
#define PFOUR_COLOR_OFF_END	0x00700000
#define PFOUR_COLOR_OFF_CMAP	0xfff00000	/* (-0x00100000) */

#define PFOUR_REG_DIAG		0x80
#define PFOUR_REG_READBACKCLR	0x40
#define PFOUR_REG_VIDEO		0x20
#define PFOUR_REG_SYNC		0x10
#define PFOUR_REG_VTRACE	0x08
#define PFOUR_REG_INT		0x04
#define PFOUR_REG_INTCLR	0x04
#define PFOUR_REG_INTEN		0x02
#define PFOUR_REG_FIRSTHALF	0x01
#define PFOUR_REG_RESET		0x01

#define PFOUR_FBTYPE_MASK	0x7f000000
#define	PFOUR_FBTYPE(x)		((x) >> 24)

#define	PFOUR_ID_MASK		0xf0
#define PFOUR_ID(x)		(PFOUR_FBTYPE((x)) == PFOUR_ID_COLOR24 ? \
				    PFOUR_ID_COLOR24 : \
				    PFOUR_FBTYPE((x)) & PFOUR_ID_MASK)
#define PFOUR_ID_BW		0x00	/* monochrome */
#define PFOUR_ID_FASTCOLOR	0x60	/* accelerated 8-bit color */
#define PFOUR_ID_COLOR8P1	0x40	/* 8-bit color + overlay */
#define PFOUR_ID_COLOR24	0x45	/* 24-bit color + overlay */

#define PFOUR_SIZE_MASK		0x0f
#define PFOUR_SIZE(x)		(PFOUR_FBTYPE((x)) & PFOUR_SIZE_MASK)
#define PFOUR_SIZE_1152X900	0x01
#define PFOUR_SIZE_1024X1024	0x02
#define PFOUR_SIZE_1280X1024	0x03
#define PFOUR_SIZE_1600X1280	0x00
#define PFOUR_SIZE_1440X1440	0x04
#define PFOUR_SIZE_640X480	0x05

#define PFOUR_NOTPFOUR		-1
