
/*
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

#ifndef _GRF_CLREG_H
#define _GRF_CLREG_H

/*
 * Written & Copyright by Kari Mettinen, Ezra Story.
 *
 * This is derived from retina driver source
 */

/* Extension to grfvideo_mode to support text modes.
 * This can be passed to both text & gfx functions 
 * without worry.  If gv.depth == 4, then the extended
 * fields for a text mode are present.
 */
struct grfcltext_mode {
    	struct grfvideo_mode gv;
    	unsigned short	fx; 	    /* font x dimension */
    	unsigned short	fy; 	    /* font y dimension */
    	unsigned short  cols;       /* screen dimensions */
    	unsigned short	rows;
    	void	    	*fdata;     /* font data */
    	unsigned short	fdstart;
    	unsigned short	fdend;
};


/* 5426 boards types, stored in  cltype in grf_cl.c .
 * used to decide how to handle SR7 and Pass-through 
 */

#define PICASSO		2167
#define SPECTRUM 	2193
#define PICCOLO		2195

/* read VGA register */
#define vgar(ba, reg) (*(((volatile unsigned char *)ba)+reg))

/* write VGA register */
#define vgaw(ba, reg, val) \
	*(((volatile unsigned char *)ba)+reg) = ((val) & 0xff)

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

/* Graphics Controller: */
#define GCT_ADDRESS		0x03CE
#define GCT_ADDRESS_R		0x03CF
#define GCT_ADDRESS_W		0x03CF
#define GCT_ID_SET_RESET	0x00
#define GCT_WR5_BG_EXT		0x00
#define GCT_ID_ENABLE_SET_RESET	0x01
#define GCT_ID_WR45_FG_EXT	0x01
#define GCT_ID_COLOR_COMPARE	0x02
#define GCT_ID_DATA_ROTATE	0x03
#define GCT_ID_READ_MAP_SELECT	0x04
#define GCT_ID_GRAPHICS_MODE	0x05
#define GCT_ID_MISC		0x06
#define GCT_ID_COLOR_XCARE	0x07
#define GCT_ID_BITMASK		0x08
#define GCT_ID_OFFSET_0		0x09
#define GCT_ID_OFFSET_1		0x0A
#define GCT_ID_MODE_EXT	0x0B
#define GCT_ID_COLOR_KEY	0x0C
#define GCT_ID_COLOR_KEY_MASK	0x0D
#define GCT_ID_MISC_CNTL	0x0E
#define GCT_ID_16BIT_BG_HIGH	0x10
#define GCT_ID_16BIT_FG_HIGH	0x11
#define GCT_ID_BLT_WIDTH_LOW	0x20
#define GCT_ID_BLT_WIDTH_HIGH	0x21
#define GCT_ID_BLT_HEIGHT_LOW	0x22
#define GCT_ID_BLT_HEIGHT_HIGH	0x23
#define GCT_ID_DST_PITCH_LOW	0x24
#define GCT_ID_DST_PITCH_HIGH	0x25
#define GCT_ID_SRC_PITCH_LOW	0x26
#define GCT_ID_SRC_PITCH_HIGH	0x27
#define GCT_ID_DST_START_LOW	0x28
#define GCT_ID_DST_START_MID	0x29
#define GCT_ID_DST_START_HIGH	0x2A
#define GCT_ID_SRC_START_LOW	0x2C
#define GCT_ID_SRC_START_MID	0x2D
#define GCT_ID_SRC_START_HIGH	0x2E
#define GCT_ID_BLT_MODE		0x30
#define GCT_ID_BLT_STAT_START	0x31
#define GCT_ID_BLT_ROP		0x32
#define GCT_ID_TRP_COL_LOW	0x34	/* transparent color */
#define GCT_ID_TRP_COL_HIGH	0x35
#define GCT_ID_TRP_MASK_LOW	0x38
#define GCT_ID_TRP_MASK_HIGH	0x39


/* Sequencer: */
#define SEQ_ADDRESS		0x03C4
#define SEQ_ADDRESS_R		0x03C5
#define SEQ_ADDRESS_W		0x03C5
#define SEQ_ID_RESET		0x00
#define SEQ_ID_CLOCKING_MODE	0x01
#define SEQ_ID_MAP_MASK		0x02
#define SEQ_ID_CHAR_MAP_SELECT	0x03

#define TEXT_PLANE_CHAR	    0x01
#define TEXT_PLANE_ATTR	    0x02
#define TEXT_PLANE_FONT	    0x04

#define SEQ_ID_MEMORY_MODE	0x04
#define SEQ_ID_UNLOCK_EXT	0x06	/* down from here, all seq registers are Cirrus extensions */
#define SEQ_ID_EXT_SEQ_MODE     0x07
#define SEQ_ID_EEPROM_CNTL	0x08
#define SEQ_ID_SCRATCH_0        0x09
#define SEQ_ID_SCRATCH_1	0x0A
#define SEQ_ID_VCLK_0_NUM	0x0B
#define SEQ_ID_VCLK_1_NUM	0x0C
#define SEQ_ID_VCLK_2_NUM	0x0D
#define SEQ_ID_VCLK_3_NUM	0x0E
#define SEQ_ID_DRAM_CNTL	0x0F
#define SEQ_ID_CURSOR_X		0x10	/* Cursor position can't be set with WSeq
*/
#define SEQ_ID_CURSOR_Y		0x11
#define SEQ_ID_CURSOR_ATTR	0x12
#define SEQ_ID_CURSOR_STORE	0x13
#define SEQ_ID_SCRATCH_2	0x14
#define SEQ_ID_SCRATCH_3	0x15
#define SEQ_ID_PERF_TUNE	0x16
#define SEQ_ID_CONF_RBACK	0x17
#define SEQ_ID_SIG_CNTL		0x18
#define SEQ_ID_SIG_RES_LOW	0x19
#define SEQ_ID_SIG_RES_HIGH	0x1A
#define SEQ_ID_VCLK_0_DENOM	0x1B
#define SEQ_ID_VCLK_1_DENOM	0x1C
#define SEQ_ID_VCLK_2_DENOM	0x1D
#define SEQ_ID_VCLK_3_DENOM	0x1E
#define SEQ_ID_MCLK_SELECT	0x1F

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
#define CRT_ID_CHAR_HEIGHT	0x09	/* was MAX_SCANLINES on retina, weird, eh? */
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
#define CRT_ID_LACE_END         0x19
#define CRT_ID_LACE_CNTL        0x1A
#define CRT_ID_EXT_DISP_CNTL    0x1B

#define CRT_ID_GD_LATCH_RBACK	0x22

#define CRT_ID_ACT_TOGGLE_RBACK	0x24
#define CRT_ID_ACT_INDEX_RBACK 	0x26

/* Pass-through */
#define PASS_ADDRESS		0x8000
#define PASS_ADDRESS_W		0x8000
/* Special Picasso Address */
#define PASS_ADDRESS_WP		0x9000

/* Video DAC */
#define VDAC_ADDRESS		0x03c8
#define VDAC_ADDRESS_W		0x03c8
#define VDAC_ADDRESS_R		0x03c7
#define VDAC_STATE		0x03c7
#define VDAC_DATA		0x03c9
#define VDAC_MASK		0x03c6
#define HDR			0x03c6	/* Hidden DAC register, 4 reads to access */


#define WGfx(ba, idx, val) \
	do { vgaw(ba, GCT_ADDRESS, idx); vgaw(ba, GCT_ADDRESS_W , val); } while (0)

#define WSeq(ba, idx, val) \
	do { vgaw(ba, SEQ_ADDRESS, idx); vgaw(ba, SEQ_ADDRESS_W , val); } while (0)

#define WCrt(ba, idx, val) \
	do { vgaw(ba, CRT_ADDRESS, idx); vgaw(ba, CRT_ADDRESS_W , val); } while (0)

#define WAttr(ba, idx, val) \
	do {	\
		unsigned char tmp;\
		vgaw(ba, CRT_ADDRESS, CRT_ID_ACT_TOGGLE_RBACK);\
		tmp = vgar(ba, CRT_ADDRESS_R);\
		if(tmp & 0x80)vgaw(ba,ACT_ADDRESS_W,vgar(ba,ACT_ADDRESS_R));\
		vgaw(ba, ACT_ADDRESS_W, idx);\
		vgaw(ba, ACT_ADDRESS_W, val);\
	} while (0)

#define SetTextPlane(ba, m) \
	do { \
    	     WGfx(ba, GCT_ID_READ_MAP_SELECT, m & 3 );\
	     WSeq(ba, SEQ_ID_MAP_MASK, (1 << (m & 3)));\
        } while (0)

/* Special wakeup/passthrough registers on graphics boards
 *
 * The methods have diverged a bit for each board, so
 * WPass(P) has been converted into a set of specific
 * inline functions.
 */
static inline void RegWakeup(volatile void *ba) {
    	extern int cltype;
    	extern unsigned char pass_toggle;

    	switch (cltype) { 
    	case SPECTRUM: 
    	    	vgaw(ba, PASS_ADDRESS_W, 0x1f); 
    	    	break; 
    	case PICASSO: 
    	    	vgaw(ba, PASS_ADDRESS_W, 0xff); 
    	    	break; 
    	case PICCOLO: 
    	    	vgaw(ba, PASS_ADDRESS_W, vgar(ba, PASS_ADDRESS) | 0x10); 
    	    	break; 
    	} 
    	delay(200000);
}
static inline void RegOnpass(volatile void *ba) {
    	extern int cltype;
    	extern unsigned char pass_toggle;

    	switch (cltype) { 
    	case SPECTRUM:
    	    	vgaw(ba, PASS_ADDRESS_W, 0x4f); 
    	    	break; 
    	case PICASSO: 
    	    	vgaw(ba, PASS_ADDRESS_WP, 0x01); 
    	    	break; 
    	case PICCOLO: 
    	    	vgaw(ba, PASS_ADDRESS_W, vgar(ba, PASS_ADDRESS) & 0xdf); 
    	    	break; 
    	} 
    	pass_toggle = 1;
    	delay(200000);
}
static inline void RegOffpass(volatile void *ba) {
    	extern int cltype;
    	extern unsigned char pass_toggle;

    	switch (cltype) { 
    	case SPECTRUM: 
    	    	vgaw(ba, PASS_ADDRESS_W, 0x6f); 
    	    	break; 
    	case PICASSO: 
    	    	vgaw(ba, PASS_ADDRESS_W, 0xff); 
    	    	delay(200000); 
    	    	vgaw(ba, PASS_ADDRESS_W, 0xff); 
    	    	break; 
    	case PICCOLO: 
    	    	vgaw(ba, PASS_ADDRESS_W, vgar(ba, PASS_ADDRESS) | 0x20); 
    	    	break; 
    	} 
    	pass_toggle = 0;
    	delay(200000);
}

static inline unsigned char RAttr(volatile void * ba, short idx) {

	unsigned char tmp;
	vgaw(ba, CRT_ADDRESS, CRT_ID_ACT_TOGGLE_RBACK);
	tmp = vgar(ba, CRT_ADDRESS_R);
	if(tmp & 0x80)vgar(ba,ACT_ADDRESS_R);
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

int cl_mode __P((register struct grf_softc *gp, int cmd, void *arg, int a2, int a3));
int cl_load_mon __P((struct grf_softc *gp, struct grfcltext_mode *gv)); 
int grfcl_cnprobe __P((void));
void grfcl_iteinit __P((struct grf_softc *gp));

#endif /* _GRF_RHREG_H */

