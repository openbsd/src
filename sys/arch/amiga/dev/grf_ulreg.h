/*	$NetBSD: grf_ulreg.h,v 1.3 1995/12/31 01:22:03 chopps Exp $	*/

/*
 * Copyright (c) 1995 Ignatios Souvatzis
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
 *      This product includes software developed by Lutz Vieweg.
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
 * Registers etc. for the University of Lowell graphics board.
 */

struct gspregs {
	/*
	 * alas, they didn't invert the A1 bit, so be have to write the 
	 * address pointer in two parts
	 */
	__volatile u_short hstadrl;
	__volatile u_short hstadrh;
	__volatile u_short data;
	__volatile u_short ctrl;
};

/* Bits in ctrl */
#define HLT	0x8000
#define CF	0x4000
#define LBL	0x2000
#define INCR	0x1000
#define INCW	0x0800
#define NMIM	0x0200
#define NMI	0x0100

#define INTOUT	0x0080
#define MSGOUT	0x0070
#define INTIN	0x0008
#define	MSGIN	0x0007

/* address macros */

#define GSPSETHADRS(gsp,adrs)	do {	\
    (gsp)->hstadrh = (adrs) >> 16;	\
    (gsp)->hstadrl = (adrs) & 0xFFFF;	\
    } while (0)
#define GSPGETHADRS(gsp)	((gsp)->hstadrh << 16 | (gsp)->hstadrl)

/* Standard addresses in GSP memory */

#define PUT_PTR_ADRS    0xFFA20000	/* put pointer in ring buffer */
#define PUT_HI_PTR_ADRS 0xFFA20010	/* put pointer high word */
#define GET_PTR_ADRS    0xFFA20020	/* get pointer (low word) */
#define GSP_MODE_ADRS   0xFFA20040	/* GSP mode word */

/* Bits in GSP mode word */
#define GMODE_HOLD      1               /* hold screen */
#define GMODE_FLUSH     2               /* flush GSP input queue */
#define GMODE_ALTSCRN   4               /* use alternate screen */
#define GMODE_DISPCTRL  8               /* display control chars */

/* command words */
#define GCMD_CMD_MSK	0x000F
#define GCMD_PAR_MSK	0xFFF0

#define GCMD_NOP	0
#define GCMD_CHAR	1	/* char, fg, bg, x, y */
#define GCMD_FILL	2	/* fg, x, y, w, h, ppop */
#define GCMD_PIXBLT	3	/* x, y, w, h, dx, dy */
#define GCMD_FNTMIR	4	/* */
#define GCMD_CMAP	5	/* overlay==1, index, red, green, blue */
#define GCMD_MCHG	6	/* width, height, baseh, basel, pitch, depth */

struct grf_ul_softc {
	struct grf_softc	gus_sc;
	u_int8_t		gus_imcmap[768];
	u_int8_t		gus_ovcmap[12];
	u_int8_t		gus_ovslct;
	/* realconfig stuff assumes this is last, else it would get copied: */
	struct isr		gus_isr;
};

#ifdef _KERNEL
void gsp_write(struct gspregs *gsp, u_int16_t *data, size_t size);
int grful_cnprobe(void);
void grful_iteinit(struct grf_softc *gp);
#endif

