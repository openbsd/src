/*	$NetBSD: grf_etreg.h,v 1.1.4.1 1996/05/27 01:12:13 is Exp $	*/

/*
 * Copyright (c) 1996 Tobias Abt
 * Copyright (c) 1995 Ezra Story
 * Copyright (c) 1995 Kari Mettinen
 * Copyright (c) 1994 Markus Wild
 * Copyright (c) 1994 Lutz Vieweg
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

#ifndef _GRF_ETREG_H
#define _GRF_ETREG_H

/*
 * Written & Copyright by Kari Mettinen, Ezra Story.
 *
 * This is derived from Cirrus driver source
 */

/* Extension to grfvideo_mode to support text modes.
 * This can be passed to both text & gfx functions 
 * without worry.  If gv.depth == 4, then the extended
 * fields for a text mode are present.
 */
struct grfettext_mode {
    	struct grfvideo_mode gv;
    	unsigned short	fx; 	    /* font x dimension */
    	unsigned short	fy; 	    /* font y dimension */
    	unsigned short  cols;       /* screen dimensions */
    	unsigned short	rows;
    	void	    	*fdata;     /* font data */
    	unsigned short	fdstart;
    	unsigned short	fdend;
};


/* Tseng boards types, stored in ettype in grf_et.c.
 * used to decide how to handle Pass-through, etc. 
 */

#define OMNIBUS		2181
#define DOMINO		2167
#define MERLIN		2117

/* VGA controller types */
#define ET4000      0
#define ETW32       1

/* DAC types */
#define SIERRA11483 0    /* Sierra 11483 HiColor DAC */
#define SIERRA15025 1    /* Sierra 15025 TrueColor DAC */
#define MUSICDAC    2    /* MUSIC TrueColor DAC */
#define MERLINDAC   3    /* Merlin's BrookTree TrueColor DAC */

/* read VGA register */
#define vgar(ba, reg) (*(((volatile unsigned char *)ba)+reg))

/* write VGA register */
#define vgaw(ba, reg, val) \
	*(((volatile unsigned char *)ba)+reg) = ((unsigned char)val)

/*
 * defines for the used register addresses (mw)
 *
 * NOTE: there are some registers that have different addresses when
 *       in mono or color mode. We only support color mode, and thus
 *       some addresses won't work in mono-mode!
 *
 * General and VGA-registers taken from retina driver. Fixed a few
 * bugs in it. (SR and GR read address is Port + 1, NOT Port)
 *
 */

/* General Registers: */
#define GREG_STATUS0_R		0x03C2
#define GREG_STATUS1_R		0x03DA
#define GREG_MISC_OUTPUT_R	0x03CC
#define GREG_MISC_OUTPUT_W	0x03C2	
#define GREG_FEATURE_CONTROL_R	0x03CA
#define GREG_FEATURE_CONTROL_W	0x03DA
#define GREG_POS		0x0102
#define	GREG_HERCULESCOMPAT	0x03BF
#define	GREG_VIDEOSYSENABLE	0x03C3
#define	GREG_DISPMODECONTROL	0x03D8
#define	GREG_COLORSELECT	0x03D9
#define	GREG_ATNTMODECONTROL	0x03DE
#define	GREG_SEGMENTSELECT	0x03CD

/* ETW32 special */
#define W32mappedRegs 0xfff00
    
/* MMU */
#define MMU_APERTURE0 0x80000
#define MMU_APERTURE1 0xa0000
#define MMU_APERTURE2 0xc0000

/* Accellerator */

/* Attribute Controller: */
#define ACT_ADDRESS		0x03C0
#define ACT_ADDRESS_R		0x03C1
#define ACT_ADDRESS_W		0x03C0
#define ACT_ADDRESS_RESET	0x03DA
#define ACT_ID_PALETTE0		0x00
#define ACT_ID_PALETTE1		0x01
#define ACT_ID_PALETTE2		0x02
#define ACT_ID_PALETTE3		0x03
#define ACT_ID_PALETTE4		0x04
#define ACT_ID_PALETTE5		0x05
#define ACT_ID_PALETTE6		0x06
#define ACT_ID_PALETTE7		0x07
#define ACT_ID_PALETTE8		0x08
#define ACT_ID_PALETTE9		0x09
#define ACT_ID_PALETTE10	0x0A
#define ACT_ID_PALETTE11	0x0B
#define ACT_ID_PALETTE12	0x0C
#define ACT_ID_PALETTE13	0x0D
#define ACT_ID_PALETTE14	0x0E
#define ACT_ID_PALETTE15	0x0F
#define ACT_ID_ATTR_MODE_CNTL	0x10
#define ACT_ID_OVERSCAN_COLOR	0x11
#define ACT_ID_COLOR_PLANE_ENA	0x12
#define ACT_ID_HOR_PEL_PANNING	0x13
#define ACT_ID_COLOR_SELECT	0x14
#define	ACT_ID_MISCELLANEOUS	0x16

/* Graphics Controller: */
#define GCT_ADDRESS		0x03CE
#define GCT_ADDRESS_R		0x03CF
#define GCT_ADDRESS_W		0x03CF
#define GCT_ID_SET_RESET	0x00
#define GCT_ID_ENABLE_SET_RESET	0x01
#define GCT_ID_COLOR_COMPARE	0x02
#define GCT_ID_DATA_ROTATE	0x03
#define GCT_ID_READ_MAP_SELECT	0x04
#define GCT_ID_GRAPHICS_MODE	0x05
#define GCT_ID_MISC		0x06
#define GCT_ID_COLOR_XCARE	0x07
#define GCT_ID_BITMASK		0x08

/* Sequencer: */
#define SEQ_ADDRESS		0x03C4
#define SEQ_ADDRESS_R		0x03C5
#define SEQ_ADDRESS_W		0x03C5
#define SEQ_ID_RESET		0x00
#define SEQ_ID_CLOCKING_MODE	0x01
#define SEQ_ID_MAP_MASK		0x02
#define SEQ_ID_CHAR_MAP_SELECT	0x03
#define SEQ_ID_MEMORY_MODE	0x04
#define	SEQ_ID_STATE_CONTROL	0x06
#define	SEQ_ID_AUXILIARY_MODE	0x07

/* don't know about them right now...
#define TEXT_PLANE_CHAR	    0x01
#define TEXT_PLANE_ATTR	    0x02
#define TEXT_PLANE_FONT	    0x04
*/

/* CRT Controller: */
#define CRT_ADDRESS		0x03D4
#define CRT_ADDRESS_R		0x03D5
#define CRT_ADDRESS_W		0x03D5
#define CRT_ID_HOR_TOTAL	0x00
#define CRT_ID_HOR_DISP_ENA_END	0x01
#define CRT_ID_START_HOR_BLANK	0x02
#define CRT_ID_END_HOR_BLANK	0x03
#define CRT_ID_START_HOR_RETR	0x04
#define CRT_ID_END_HOR_RETR	0x05
#define CRT_ID_VER_TOTAL	0x06
#define CRT_ID_OVERFLOW		0x07
#define CRT_ID_PRESET_ROW_SCAN	0x08
#define	CRT_ID_MAX_ROW_ADDRESS	0x09
#define CRT_ID_CURSOR_START	0x0A
#define CRT_ID_CURSOR_END	0x0B
#define CRT_ID_START_ADDR_HIGH	0x0C
#define CRT_ID_START_ADDR_LOW	0x0D
#define CRT_ID_CURSOR_LOC_HIGH	0x0E
#define CRT_ID_CURSOR_LOC_LOW	0x0F
#define CRT_ID_START_VER_RETR	0x10
#define CRT_ID_END_VER_RETR	0x11
#define CRT_ID_VER_DISP_ENA_END	0x12
#define CRT_ID_OFFSET		0x13
#define CRT_ID_UNDERLINE_LOC	0x14
#define CRT_ID_START_VER_BLANK	0x15
#define CRT_ID_END_VER_BLANK	0x16
#define CRT_ID_MODE_CONTROL	0x17
#define CRT_ID_LINE_COMPARE	0x18

#define	CRT_ID_SEGMENT_COMP	0x30
#define	CRT_ID_GENERAL_PURPOSE	0x31
#define	CRT_ID_RASCAS_CONFIG	0x32
#define	CTR_ID_EXT_START	0x33
#define	CRT_ID_6845_COMPAT	0x34
#define	CRT_ID_OVERFLOW_HIGH	0x35
#define	CRT_ID_VIDEO_CONFIG1	0x36
#define	CRT_ID_VIDEO_CONFIG2	0x37
#define	CRT_ID_HOR_OVERFLOW	0x3f

/* IMAGE port */
#define IMA_ADDRESS		0x217a
#define IMA_ADDRESS_R		0x217b
#define IMA_ADDRESS_W		0x217b
#define IMA_STARTADDRESSLOW     0xf0
#define IMA_STARTADDRESSMIDDLE  0xf1
#define IMA_STARTADDRESSHIGH    0xf2
#define IMA_TRANSFERLENGTHLOW   0xf3
#define IMA_TRANSFERLENGTHHIGH  0xf4
#define IMA_ROWOFFSETLOW        0xf5
#define IMA_ROWOFFSETHIGH       0xf6
#define IMA_PORTCONTROL         0xf7

/* Pass-through */
#define PASS_ADDRESS		0x8000
#define PASS_ADDRESS_W		0x8000

/* Video DAC */
#define VDAC_ADDRESS		0x03c8
#define VDAC_ADDRESS_W		0x03c8
#define VDAC_ADDRESS_R		0x03c7
#define VDAC_STATE		0x03c7
#define VDAC_DATA		0x03c9
#define VDAC_MASK		0x03c6
#define HDR			0x03c6	/* Hidden DAC register, 4 reads to access */

#define VDAC_COMMAND 0x03c6
#define VDAC_XINDEX 0x03c7
#define VDAC_XDATA 0x03c8

#define MERLIN_VDAC_INDEX 0x01
#define MERLIN_VDAC_COLORS 0x05
#define MERLIN_VDAC_SPRITE 0x09
#define MERLIN_VDAC_DATA 0x19
#define MERLIN_SWITCH_REG 0x0401

#define WGfx(ba, idx, val) \
	do { vgaw(ba, GCT_ADDRESS, idx); vgaw(ba, GCT_ADDRESS_W , val); } while (0)

#define WSeq(ba, idx, val) \
	do { vgaw(ba, SEQ_ADDRESS, idx); vgaw(ba, SEQ_ADDRESS_W , val); } while (0)

#define WCrt(ba, idx, val) \
	do { vgaw(ba, CRT_ADDRESS, idx); vgaw(ba, CRT_ADDRESS_W , val); } while (0)

#define WIma(ba, idx, val) \
	do { vgaw(ba, IMA_ADDRESS, idx); vgaw(ba, IMA_ADDRESS_W , val); } while (0)

#define WAttr(ba, idx, val) \
	do {	\
		if(vgar(ba, GREG_STATUS1_R));\
		vgaw(ba, ACT_ADDRESS_W, idx);\
		vgaw(ba, ACT_ADDRESS_W, val);\
	} while (0)

#define SetTextPlane(ba, m) \
	do { \
		WGfx(ba, GCT_ID_READ_MAP_SELECT, m & 3 );\
		WSeq(ba, SEQ_ID_MAP_MASK, (1 << (m & 3)));\
	} while (0)

#define setMerlinDACmode(ba, mode) \
	do { \
		vgaw(ba, VDAC_MASK,  mode | (vgar(ba, VDAC_MASK) & 0x0f));\
	} while (0)

/* Special wakeup/passthrough registers on graphics boards
 *
 * The methods have diverged a bit for each board, so
 * WPass(P) has been converted into a set of specific
 * inline functions.
 */
static inline void RegWakeup(volatile void *ba) {
    	extern int ettype;

    	switch (ettype) { 
    	case OMNIBUS: 
    	    	vgaw(ba, PASS_ADDRESS_W, 0x00); 
    	    	break; 
/*
    	case DOMINO: 
    	    	vgaw(ba, PASS_ADDRESS_W, 0x00); 
    	    	break; 
    	case MERLIN: 
    	    	break; 
*/
    	} 
    	delay(200000);
}


static inline void RegOnpass(volatile void *ba) {
    	extern int ettype;
	extern unsigned char pass_toggle;
	extern unsigned char Merlin_switch;

    	switch (ettype) { 
    	case OMNIBUS: 
	  vgaw(ba, PASS_ADDRESS_W, 0x00); 
	  break; 
    	case DOMINO: 
	  vgaw(ba, PASS_ADDRESS_W, 0x00); 
	  break; 
    	case MERLIN:
	  Merlin_switch &= 0xfe;
	  vgaw(ba, MERLIN_SWITCH_REG, Merlin_switch);
    	    	break;
    	} 
    	pass_toggle = 1;
    	delay(200000);
}


static inline void RegOffpass(volatile void *ba) {
    	extern int ettype;
    	extern unsigned char pass_toggle;
	extern unsigned char Merlin_switch;

    	switch (ettype) { 
    	case OMNIBUS: 
    	    	vgaw(ba, PASS_ADDRESS_W, 0x01); 
    	    	break; 
    	case DOMINO: 
    	    	vgaw(ba, PASS_ADDRESS_W, 0x00); 
    	    	break; 
    	case MERLIN:
	  Merlin_switch |= 0x01;
	  vgaw(ba, MERLIN_SWITCH_REG, Merlin_switch);
    	    	break;
    	} 
    	pass_toggle = 0;
    	delay(200000);
}

static inline unsigned char RAttr(volatile void * ba, short idx) {
	if(vgar(ba, GREG_STATUS1_R));
	vgaw(ba, ACT_ADDRESS_W, idx);
	return vgar (ba, ACT_ADDRESS_R);
}

static inline unsigned char RSeq(volatile void * ba, short idx) {
	vgaw (ba, SEQ_ADDRESS, idx);
	return vgar (ba, SEQ_ADDRESS_R);
}

static inline unsigned char RCrt(volatile void * ba, short idx) {
	vgaw (ba, CRT_ADDRESS, idx);
	return vgar (ba, CRT_ADDRESS_R);
}

static inline unsigned char RGfx(volatile void * ba, short idx) {
	vgaw(ba, GCT_ADDRESS, idx);
	return vgar (ba, GCT_ADDRESS_R);
}

int et_mode __P((register struct grf_softc *gp, u_long cmd, void *arg, u_long a2, int a3));
int et_load_mon __P((struct grf_softc *gp, struct grfettext_mode *gv)); 
int grfet_cnprobe __P((void));
void grfet_iteinit __P((struct grf_softc *gp));

#endif /* _GRF_ETREG_H */
