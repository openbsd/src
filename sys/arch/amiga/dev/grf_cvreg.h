/*	$NetBSD: grf_cvreg.h,v 1.3 1995/12/27 07:15:55 chopps Exp $	*/

/*
 * Copyright (c) 1995 Michael Teske
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
 *      This product includes software developed by Ezra Story, by Kari
 *      Mettinen and by Bernd Ernesti.
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

#ifndef _GRF_CVREG_H
#define _GRF_CVREG_H

/*
 * This is derived from ciruss driver source
 */

/* Extension to grfvideo_mode to support text modes.
 * This can be passed to both text & gfx functions
 * without worry.  If gv.depth == 4, then the extended
 * fields for a text mode are present.
 */

struct grfcvtext_mode {
	struct grfvideo_mode gv;
	unsigned short	fx;	/* font x dimension */
	unsigned short	fy;	/* font y dimension */
	unsigned short	cols;	/* screen dimensions */
	unsigned short	rows;
	void		*fdata;	/* font data */
	unsigned short	fdstart;
	unsigned short	fdend;
};


/* read VGA register */
#define vgar(ba, reg) (*(((volatile caddr_t)ba)+reg))

/* write VGA register */
#define vgaw(ba, reg, val) \
	*(((volatile caddr_t)ba)+reg) = ((val) & 0xff)

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
#define GREG_MISC_OUTPUT_R	0x03CC
#define GREG_MISC_OUTPUT_W	0x03C2	
#define GREG_FEATURE_CONTROL_R	0x03CA
#define GREG_FEATURE_CONTROL_W	0x03DA
#define GREG_INPUT_STATUS0_R	0x03C2
#define GREG_INPUT_STATUS1_R	0x03DA

/* Setup Registers: */
#define SREG_OPTION_SELECT	0x0102
#define SREG_VIDEO_SUBS_ENABLE	0x46E8

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
#define SEQ_ID_UNKNOWN1		0x05
#define SEQ_ID_UNKNOWN2		0x06
#define SEQ_ID_UNKNOWN3		0x07
/* S3 extensions */
#define SEQ_ID_UNLOCK_EXT	0x08
#define SEQ_ID_EXT_SEQ_REG9	0x09
#define SEQ_ID_BUS_REQ_CNTL	0x0A
#define SEQ_ID_EXT_MISC_SEQ	0x0B
#define SEQ_ID_UNKNOWN4		0x0C
#define SEQ_ID_EXT_SEQ		0x0D
#define SEQ_ID_UNKNOWN5		0x0E
#define SEQ_ID_UNKNOWN6		0x0F
#define SEQ_ID_MCLK_LO		0x10
#define SEQ_ID_MCLK_HI		0x11
#define SEQ_ID_DCLK_LO		0x12
#define SEQ_ID_DCLK_HI		0x13
#define SEQ_ID_CLKSYN_CNTL_1	0x14
#define SEQ_ID_CLKSYN_CNTL_2	0x15
#define SEQ_ID_CLKSYN_TEST_HI	0x16	/* reserved for S3 testing of the */
#define SEQ_ID_CLKSYN_TEST_LO	0x17	/*   internal clock synthesizer   */
#define SEQ_ID_RAMDAC_CNTL	0x18
#define SEQ_ID_MORE_MAGIC	0x1A

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
#define CRT_ID_MAX_SCAN_LINE	0x09
#define CRT_ID_CURSOR_START	0x0A
#define CRT_ID_CURSOR_END	0x0B
#define CRT_ID_START_ADDR_HIGH	0x0C
#define CRT_ID_START_ADDR_LOW	0x0D
#define CRT_ID_CURSOR_LOC_HIGH	0x0E
#define CRT_ID_CURSOR_LOC_LOW	0x0F
#define CRT_ID_START_VER_RETR	0x10
#define CRT_ID_END_VER_RETR	0x11
#define CRT_ID_VER_DISP_ENA_END	0x12
#define CRT_ID_SCREEN_OFFSET	0x13
#define CRT_ID_UNDERLINE_LOC	0x14
#define CRT_ID_START_VER_BLANK	0x15
#define CRT_ID_END_VER_BLANK	0x16
#define CRT_ID_MODE_CONTROL	0x17
#define CRT_ID_LINE_COMPARE	0x18
#define CRT_ID_GD_LATCH_RBACK	0x22
#define CRT_ID_ACT_TOGGLE_RBACK	0x24
#define CRT_ID_ACT_INDEX_RBACK	0x26
/* S3 extensions: S3 VGA Registers */
#define CRT_ID_DEVICE_HIGH	0x2D
#define CRT_ID_DEVICE_LOW	0x2E
#define CRT_ID_REVISION 	0x2F
#define CRT_ID_CHIP_ID_REV	0x30
#define CRT_ID_MEMORY_CONF	0x31
#define CRT_ID_BACKWAD_COMP_1	0x32
#define CRT_ID_BACKWAD_COMP_2	0x33
#define CRT_ID_BACKWAD_COMP_3	0x34
#define CRT_ID_REGISTER_LOCK	0x35
#define CRT_ID_CONFIG_1 	0x36
#define CRT_ID_CONFIG_2 	0x37
#define CRT_ID_REGISTER_LOCK_1	0x38
#define CRT_ID_REGISTER_LOCK_2	0x39
#define CRT_ID_MISC_1		0x3A
#define CRT_ID_DISPLAY_FIFO	0x3B
#define CRT_ID_LACE_RETR_START	0x3C
/* S3 extensions: System Control Registers  */
#define CRT_ID_SYSTEM_CONFIG	0x40
#define CRT_ID_BIOS_FLAG	0x41
#define CRT_ID_LACE_CONTROL	0x42
#define CRT_ID_EXT_MODE 	0x43
#define CRT_ID_HWGC_MODE	0x45	/* HWGC = Hardware Graphics Cursor */
#define CRT_ID_HWGC_ORIGIN_X_HI	0x46
#define CRT_ID_HWGC_ORIGIN_X_LO	0x47
#define CRT_ID_HWGC_ORIGIN_Y_HI	0x48
#define CRT_ID_HWGC_ORIGIN_Y_LO	0x49
#define CRT_ID_HWGC_FG_STACK	0x4A
#define CRT_ID_HWGC_BG_STACK	0x4B
#define CRT_ID_HWGC_START_AD_HI	0x4C
#define CRT_ID_HWGC_START_AD_LO	0x4D
#define CRT_ID_HWGC_DSTART_X	0x4E
#define CRT_ID_HWGC_DSTART_Y	0x4F
/* S3 extensions: System Extension Registers  */
#define CRT_ID_EXT_SYS_CNTL_1	0x50
#define CRT_ID_EXT_SYS_CNTL_2	0x51
#define CRT_ID_EXT_BIOS_FLAG_1	0x52
#define CRT_ID_EXT_MEM_CNTL_1	0x53
#define CRT_ID_EXT_MEM_CNTL_2	0x54
#define CRT_ID_EXT_DAC_CNTL	0x55
#define CRT_ID_EX_SYNC_1	0x56
#define CRT_ID_EX_SYNC_2	0x57
#define CRT_ID_LAW_CNTL		0x58	/* LAW = Linear Address Window */
#define CRT_ID_LAW_POS_HI	0x59
#define CRT_ID_LAW_POS_LO	0x5A
#define CRT_ID_GOUT_PORT	0x5C
#define CRT_ID_EXT_HOR_OVF	0x5D
#define CRT_ID_EXT_VER_OVF	0x5E
#define CRT_ID_EXT_MEM_CNTL_3	0x60
#define CRT_ID_EX_SYNC_3	0x63
#define CRT_ID_EXT_MISC_CNTL	0x65
#define CRT_ID_EXT_MISC_CNTL_1	0x66
#define CRT_ID_EXT_MISC_CNTL_2	0x67
#define CRT_ID_CONFIG_3 	0x68
#define CRT_ID_EXT_SYS_CNTL_3	0x69
#define CRT_ID_EXT_SYS_CNTL_4	0x6A
#define CRT_ID_EXT_BIOS_FLAG_3	0x6B
#define CRT_ID_EXT_BIOS_FLAG_4	0x6C

/* Enhanced Commands Registers: */
#define ECR_SUBSYSTEM_STAT	0x42E8
#define ECR_SUBSYSTEM_CNTL	0x42E8
#define ECR_ADV_FUNC_CNTL	0x4AE8
#define ECR_CURRENT_Y_POS	0x82E8
#define ECR_CURRENT_Y_POS2	0x82EA	/* Trio64 only */
#define ECR_CURRENT_X_POS	0x86E8
#define ECR_CURRENT_X_POS2	0x86EA	/* Trio64 only */
#define ECR_DEST_Y__AX_STEP	0x8AE8
#define ECR_DEST_Y2__AX_STEP2	0x8AEA	/* Trio64 only */
#define ECR_DEST_X__DIA_STEP	0x8EE8
#define ECR_DEST_X2__DIA_STEP2	0x8EEA	/* Trio64 only */
#define ECR_ERR_TERM		0x92E8
#define ECR_ERR_TERM2		0x92EA	/* Trio64 only */
#define ECR_MAJ_AXIS_PIX_CNT	0x96E8
#define ECR_MAJ_AXIS_PIX_CNT2	0x96EA	/* Trio64 only */
#define ECR_GP_STAT		0x9AE8	/* GP = Graphics Processor */
#define ECR_DRAW_CMD		0x9AE8
#define ECR_DRAW_CMD2		0x9AEA	/* Trio64 only */
#define ECR_SHORT_STROKE	0x9EE8
#define ECR_BKGD_COLOR		0xA2E8	/* BKGD = Background */
#define ECR_FRGD_COLOR		0xA6E8	/* FRGD = Foreground */
#define ECR_BITPLANE_WRITE_MASK	0xAAE8
#define ECR_BITPLANE_READ_MASK	0xAEE8
#define ECR_COLOR_COMPARE	0xB2E8
#define ECR_BKGD_MIX		0xB6E8
#define ECR_FRGD_MIX		0xBAE8
#define ECR_READ_REG_DATA	0xBEE8
#define ECR_ID_MIN_AXIS_PIX_CNT	0x00
#define ECR_ID_SCISSORS_TOP	0x01
#define ECR_ID_SCISSORS_LEFT	0x02
#define ECR_ID_SCISSORS_BUTTOM	0x03
#define ECR_ID_SCISSORS_RIGHT	0x04
#define ECR_ID_PIX_CNTL		0x0A
#define ECR_ID_MULT_CNTL_MISC_2	0x0D
#define ECR_ID_MULT_CNTL_MISC	0x0E
#define ECR_ID_READ_SEL		0x0F
#define ECR_PIX_TRANS		0xE2E8
#define ECR_PIX_TRANS_EXT	0xE2EA
#define ECR_PATTERN_Y		0xEAE8	/* Trio64 only */
#define ECR_PATTERN_X		0xEAEA	/* Trio64 only */


/* Pass-through */
#define PASS_ADDRESS		0x40001
#define PASS_ADDRESS_W		0x40001

/* Video DAC */
#define VDAC_ADDRESS		0x03c8
#define VDAC_ADDRESS_W		0x03c8
#define VDAC_ADDRESS_R		0x03c7
#define VDAC_STATE		0x03c7
#define VDAC_DATA		0x03c9
#define VDAC_MASK		0x03c6


#define WGfx(ba, idx, val) \
	do { vgaw(ba, GCT_ADDRESS, idx); vgaw(ba, GCT_ADDRESS_W , val); } while (0)

#define WSeq(ba, idx, val) \
	do { vgaw(ba, SEQ_ADDRESS, idx); vgaw(ba, SEQ_ADDRESS_W , val); } while (0)

#define WCrt(ba, idx, val) \
	do { vgaw(ba, CRT_ADDRESS, idx); vgaw(ba, CRT_ADDRESS_W , val); } while (0)

#define WAttr(ba, idx, val) \
	do {	\
		unsigned char tmp;\
		tmp = vgar(ba, ACT_ADDRESS_RESET);\
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

static inline unsigned char
RAttr(ba, idx)
	volatile caddr_t ba;
	short idx;
{

	vgaw(ba, ACT_ADDRESS_W, idx);
	delay(0);
	return vgar(ba, ACT_ADDRESS_R);
}

static inline unsigned char
RSeq(ba, idx)
	volatile caddr_t ba;
	short idx;
{
	vgaw(ba, SEQ_ADDRESS, idx);
	return vgar(ba, SEQ_ADDRESS_R);
}

static inline unsigned char
RCrt(ba, idx)
	volatile caddr_t ba;
	short idx;
{
	vgaw(ba, CRT_ADDRESS, idx);
	return vgar(ba, CRT_ADDRESS_R);
}

static inline unsigned char
RGfx(ba, idx)
	volatile caddr_t ba; 
	short idx;
{
	vgaw(ba, GCT_ADDRESS, idx);
	return vgar(ba, GCT_ADDRESS_R);
}


static inline void
cv_write_port(bits, BoardAddr)
	unsigned short bits;
	volatile caddr_t BoardAddr;
{
	volatile char *addr;
	static unsigned char CVPortBits = 0;	/* mirror port bits here */

	addr = BoardAddr + 0x40001;
	if (bits & 0x8000)
		CVPortBits |= bits&0xFF;	/* Set bits */
	else {
		bits = bits & 0xFF;
		bits = (~bits) & 0xFF ;
		CVPortBits &= bits;	/* Clear bits */
	}

	*addr = CVPortBits;
}

#define set_port_bits(b, ba) cv_write_port((unsigned short )b | 0x8000,ba)
#define clear_port_bits(b, ba) cv_write_port((unsigned short )b & 0xff,ba)


/*
 *  Monitor Switch
 *  0 = CyberVision Signal
 *  1 = Amiga Signal,
 * ba = boardaddr
 */

static inline void
cvscreen(toggle, ba)
	char *toggle;
	volatile caddr_t ba;
{

	if (toggle)
		cv_write_port (0x10, ba);
	else
		cv_write_port (0x8010, ba);
}

/* 0 = on, 1= off */
/* ba= registerbase */
static inline void
gfx_on_off(toggle, ba)
	int toggle;
	volatile caddr_t ba;
{
	int r;

	toggle &= 0x1;
	toggle = toggle << 5;

	r = RSeq(ba, SEQ_ID_CLOCKING_MODE);
	r &= 0xdf;	/* set Bit 5 to 0 */

	WSeq(ba, SEQ_ID_CLOCKING_MODE, r | toggle);
}

#if 0
int grfcv_cnprobe __P((void));
void grfcv_iteinit __P((struct grf_softc *gp));
#endif

static unsigned char clocks[]={
0x13, 0x61, 0x6b, 0x6d, 0x51, 0x69, 0x54, 0x69, 
0x4f, 0x68, 0x6b, 0x6b, 0x18, 0x61, 0x7b, 0x6c, 
0x51, 0x67, 0x24, 0x62, 0x56, 0x67, 0x77, 0x6a, 
0x1d, 0x61, 0x53, 0x66, 0x6b, 0x68, 0x79, 0x69, 
0x7c, 0x69, 0x7f, 0x69, 0x22, 0x61, 0x54, 0x65, 
0x56, 0x65, 0x58, 0x65, 0x67, 0x66, 0x41, 0x63, 
0x27, 0x61, 0x13, 0x41, 0x37, 0x62, 0x6b, 0x4d, 
0x23, 0x43, 0x51, 0x49, 0x79, 0x66, 0x54, 0x49, 
0x7d, 0x66, 0x34, 0x56, 0x4f, 0x63, 0x1f, 0x42, 
0x6b, 0x4b, 0x7e, 0x4d, 0x18, 0x41, 0x2a, 0x43, 
0x7b, 0x4c, 0x74, 0x4b, 0x51, 0x47, 0x65, 0x49, 
0x24, 0x42, 0x68, 0x49, 0x56, 0x47, 0x75, 0x4a, 
0x77, 0x4a, 0x31, 0x43, 0x1d, 0x41, 0x71, 0x49, 
0x53, 0x46, 0x29, 0x42, 0x6b, 0x48, 0x1f, 0x41, 
0x79, 0x49, 0x6f, 0x48, 0x7c, 0x49, 0x38, 0x43, 
0x7f, 0x49, 0x5d, 0x46, 0x22, 0x41, 0x53, 0x45,
0x54, 0x45, 0x55, 0x45, 0x56, 0x45, 0x57, 0x45, 
0x58, 0x45, 0x25, 0x41, 0x67, 0x46, 0x5b, 0x45, 
0x41, 0x43, 0x78, 0x47, 0x27, 0x41, 0x51, 0x44, 
0x13, 0x21, 0x7d, 0x47, 0x37, 0x42, 0x71, 0x46, 
0x6b, 0x2d, 0x14, 0x21, 0x23, 0x23, 0x7d, 0x2f, 
0x51, 0x29, 0x61, 0x2b, 0x79, 0x46, 0x1d, 0x22, 
0x54, 0x29, 0x45, 0x27, 0x7d, 0x46, 0x7f, 0x46, 
0x4f, 0x43, 0x2f, 0x41, 0x1f, 0x22, 0x6a, 0x2b, 
0x6b, 0x2b, 0x5b, 0x29, 0x7e, 0x2d, 0x65, 0x44, 
0x18, 0x21, 0x5e, 0x29, 0x2a, 0x23, 0x45, 0x26, 
0x7b, 0x2c, 0x19, 0x21, 0x74, 0x2b, 0x75, 0x2b, 
0x51, 0x27, 0x3f, 0x25, 0x65, 0x29, 0x40, 0x25, 
0x24, 0x22, 0x41, 0x25, 0x68, 0x29, 0x42, 0x25, 
0x56, 0x27, 0x7e, 0x2b, 0x75, 0x2a, 0x1c, 0x21, 
0x77, 0x2a, 0x4f, 0x26, 0x31, 0x23, 0x6f, 0x29, 
0x1d, 0x21, 0x32, 0x23, 0x71, 0x29, 0x72, 0x29, 
0x53, 0x26, 0x69, 0x28, 0x29, 0x22, 0x75, 0x29, 
0x6b, 0x28, 0x1f, 0x21, 0x1f, 0x21, 0x6d, 0x28, 
0x79, 0x29, 0x2b, 0x22, 0x6f, 0x28, 0x59, 0x26, 
0x7c, 0x29, 0x7d, 0x29, 0x38, 0x23, 0x21, 0x21, 
0x7f, 0x29, 0x39, 0x23, 0x5d, 0x26, 0x75, 0x28, 
0x22, 0x21, 0x77, 0x28, 0x53, 0x25, 0x6c, 0x27, 
0x54, 0x25, 0x61, 0x26, 0x55, 0x25, 0x30, 0x22, 
0x56, 0x25, 0x63, 0x26, 0x57, 0x25, 0x71, 0x27, 
0x58, 0x25, 0x7f, 0x28, 0x25, 0x21, 0x74, 0x27, 
0x67, 0x26, 0x40, 0x23, 0x5b, 0x25, 0x26, 0x21, 
0x41, 0x23, 0x34, 0x22, 0x78, 0x27, 0x6b, 0x26, 
0x27, 0x21, 0x35, 0x22, 0x51, 0x24, 0x7b, 0x27, 
0x13, 0x1,  0x13, 0x1,  0x7d, 0x27, 0x4c, 0x9, 
0x37, 0x22, 0x5b, 0xb,  0x71, 0x26, 0x5c, 0xb, 
0x6b, 0xd,  0x47, 0x23, 0x14, 0x1,  0x4f, 0x9, 
0x23, 0x3,  0x75, 0x26, 0x7d, 0xf,  0x1c, 0x2,
0x51, 0x9,  0x59, 0x24, 0x61, 0xb,  0x69, 0x25, 
0x79, 0x26, 0x34, 0x5,  0x1d, 0x2,  0x6b, 0x25, 
0x54, 0x9,  0x35, 0x5,  0x45, 0x7,  0x6d, 0x25, 
0x7d, 0x26, 0x16, 0x1,  0x7f, 0x26, 0x77, 0xd, 
0x4f, 0x23, 0x78, 0xd,  0x2f, 0x21, 0x27, 0x3, 
0x1f, 0x2,  0x59, 0x9,  0x6a, 0xb,  0x73, 0x25, 
0x6b, 0xb,  0x63, 0x24, 0x5b, 0x9,  0x20, 0x2, 
0x7e, 0xd,  0x4b, 0x7,  0x65, 0x24, 0x43, 0x22, 
0x18, 0x1,  0x6f, 0xb,  0x5e, 0x9,  0x70, 0xb, 
0x2a, 0x3,  0x33, 0x4,  0x45, 0x6,  0x60, 0x9, 
0x7b, 0xc,  0x19, 0x1,  0x19, 0x1,  0x7d, 0xc, 
0x74, 0xb,  0x50, 0x7,  0x75, 0xb,  0x63, 0x9, 
0x51, 0x7,  0x23, 0x2,  0x3f, 0x5,  0x1a, 0x1, 
0x65, 0x9,  0x2d, 0x3,  0x40, 0x5,  0x0,  0x0, 
};

#endif /* _GRF_RHREG_H */

