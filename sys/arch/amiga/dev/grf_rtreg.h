/*	$OpenBSD: grf_rtreg.h,v 1.3 1998/04/17 15:55:52 niklas Exp $	*/
/*	$NetBSD: grf_rtreg.h,v 1.7 1996/04/21 21:11:23 veego Exp $	*/

/*
 * Copyright (c) 1993 Markus Wild
 * Copyright (c) 1993 Lutz Vieweg
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
#ifndef _GRF_RTREG_H
#define _GRF_RTREG_H

/*
 * This driver for the MacroSystem Retina board was only possible,
 * because MacroSystem provided information about the pecularities
 * of the board. THANKS! Competition in Europe among gfx board 
 * manufacturers is rather tough, so Lutz Vieweg, who wrote the
 * initial driver, has made an agreement with MS not to document
 * the driver source (see also his comment below).
 * -> ALL comments and register defines after
 * -> " -------------- START OF CODE -------------- "
 * -> have been added by myself (mw) from studying the publically
 * -> available "NCR 77C22E+" Data Manual
 */	 
/*
 * This code offers low-level routines to access the Retina graphics-board
 * manufactured by MS MacroSystem GmbH from within NetBSD for the Amiga.
 * 
 * Thanks to MacroSystem for providing me with the neccessary information
 * to create theese routines. The sparse documentation of this code
 * results from the agreements between MS and me.
 */

#if 0
/* these are in dev/devices.h */

/* definitions to find the autoconfig-board under
   AmigaDOS */

#define RETINA_MANUFACTURER     0x4754  
#define RETINA_PRODUCT          6
#define RETINA_SERIALNUMBER     1
#endif


/*
   For more information on the structure entries take a look at
   grf_rt.cc and ite_rt.cc.
*/

struct MonDef {
	
	/* first the general monitor characteristics */
	
	unsigned long  FQ;
	unsigned char  FLG;
	
	unsigned short MW;  /* screen width in pixels */
                            /* has to be at least a multiple of 8, */
                            /* has to be a multiple of 64 in 256-color mode */
                            /* if you want to use some great tricks */
                            /* to speed up the vertical scrolling */
	unsigned short MH;  /* screen height in pixels */
	
	unsigned short HBS;
	unsigned short HSS;
	unsigned short HSE;
	unsigned short HBE;
	unsigned short HT;
	unsigned short VBS;
	unsigned short VSS;
	unsigned short VSE;
	unsigned short VBE;
	unsigned short VT;
	
	unsigned short DEP;  /* Color-depth, 4 for text-mode */
                             /* 8 enables 256-color graphics-mode, */
                             /* 16 and 24bit gfx not supported yet */
	
	unsigned char * PAL; /* points to 16*3 byte RGB-palette data */
                             /* use LoadPalette() to set colors 0..255 */
                             /* in 256-color-gfx mode */
	
	/* all following entries are font-specific in
	   text mode. Make sure your monitor
	   parameters are calculated for the
	   appropriate font width and height!
	*/
	
       unsigned short  TX;     /* Text-mode (DEP=4):          */
                               /* screen-width  in characters */
                               /* currently, TX has to be a   */
                               /* multiple of 16!             */
                               
                               /* Gfx-mode (DEP > 4)          */
                               /* "logical" screen-width,     */
                               /* use values > MW to allow    */
                               /* hardware-panning            */
                               /* has to be a multiple of 8   */
                               
       unsigned short  TY;     /* Text-mode:                  */
                               /* screen-height in characters */
       
                               /* Gfx-mode: "logical" screen  */
                               /* height for panning          */
       
       /* the following values are currently unused for gfx-mode */
       
	unsigned short  XY;     /* TX*TY (speeds up some calcs.) */
	
	unsigned short  FX;     /* font-width (valid values: 4,7-16) */
	unsigned short  FY;     /* font-height (valid range: 1-32) */
	unsigned char * FData;  /* pointer to the font-data */
	
	/* The font data is simply an array of bytes defining
	   the chars in ascending order, line by line. If your
	   font is wider than 8 pixel, FData has to be an
	   array of words. */
	
	unsigned short  FLo;    /* lowest character defined */
	unsigned short  FHi;    /* highest char. defined */ 
	
};


#if 0
/* Some ready-made MonDef structures are available in grf_rt.cc */

extern struct MonDef MON_640_512_60;
extern struct MonDef MON_768_600_60;
extern struct MonDef MON_768_600_80;

/* text-screen resolutions wider than 1024 are currently damaged.
   The VRAM access seems to become unstable at higher resolutions.
   This may hopefully be subject of change.
*/

extern struct MonDef MON_1024_768_80;
extern struct MonDef MON_1024_1024_59;

/* WARNING: THE FOLLOWING MONITOR MODES EXCEED THE 90-MHz LIMIT THE PROCESSOR
            HAS BEEN SPECIFIED FOR. USE AT YOUR OWN RISK (AND THINK ABOUT
            MOUNTING SOME COOLING DEVICE AT THE PROCESSOR AND RAMDAC)!     */
extern struct MonDef MON_1280_1024_60;
extern struct MonDef MON_1280_1024_69;

/* Default monitor (change if this is too much for your monitor :-)) */
#define DEFAULT_MONDEF	MON_768_600_80

#else

/* nothing exported for now... */

#endif

/* a standard 16-color palette is available in grf_rt.cc
   and used by the standard monitor-definitions above */
extern unsigned char NCRStdPalette[];

/* The prototypes for C
   with a little explanation

	unsigned char * InitNCR(volatile void * BoardAdress, struct MonDef * md = &MON_640_512_60);

   This routine initialises the Retina hardware, opens a
   text- or gfx-mode screen, depending on the the value of MonDef.DEP,
   and sets the cursor to position 0. 
   It takes as arguments a pointer to the hardware-base
   address as it is denoted in the DevConf structure
   of the AmigaDOS, and a pointer to a struct MonDef
   which describes the screen-mode parameters.
   
   The routine returns 0 if it was unable to open the screen,
   or an unsigned char * to the display/attribute memory
   when it succeeded. The organisation of the display memory
   is a little strange in text-mode (Intel-typically...) :
   
   Byte  00    01    02    03    04     05    06   etc.
       Char0  Attr0  --    --   Char1 Attr1   --   etc.
       
   You may set a character and its associated attribute byte
   with a single word-access, or you may perform to byte writes
   for the char and attribute. Each 2. word has no meaning,
   and writes to theese locations are ignored.
   
   The attribute byte for each character has the following
   structure:
   
   Bit  7     6     5     4     3     2     1     0
      BLINK BACK2 BACK1 BACK0 FORE3 FORE2 FORE1 FORE0
      
   Were FORE is the foreground-color index (0-15) and
   BACK is the background color index (0-7). BLINK 
   enables blinking for the associated character.
   The higher 8 colors in the standard palette are
   lighter than the lower 8, so you may see FORE3 as
   an intensity bit. If FORE == 1 or FORE == 9 and
   BACK == 0 the character is underlined. Since I don't
   think this looks good, it will probably change in a
   future release.

   There's no routine "SetChar" or "SetAttr" provided,
   because I think it's so trivial... a function call
   would be pure overhead. As an example, a routine
   to set the char code and attribute at position x,y:
   (assumed the value returned by InitNCR was stored
    into "DispMem", the actual MonDef struct * is hold
    in "MDef")
   
   void SetChar(unsigned char chr, unsigned char attr,
                unsigned short x, unsigned short y) {
      
      unsigned struct MonDef * md = MDef;
      unsigned char * c = DispMem + x*4 + y*md->TX*4;
      
      *c++ = chr;
      *c   = attr;
      
   }
   
   In Gfx-mode, the memory organisation is rather simple,
   1 byte per pixel in 256-color mode, one pixel after
   each other, line by line.
   
   Currently, InitNCR() disables the Retina VBLANK IRQ,
   but beware: When running the Retina WB-Emu under
   AmigaDOS, the VBLANK IRQ is ENABLED.
   
	void SetCursorPos(unsigned short pos);

   This routine sets the hardware-cursor position 
   to the screen location pos. pos can be calculated
   as (x + y * md->TY).
   Text-mode only!

	void ScreenUp(void);

   A somewhat optimized routine that scrolls the whole
   screen up one row. A good idea to compile this piece
   of code with optimization enabled.
   Text-mode only!

	void ScreenDown(void);

   A somewhat optimized routine that scrolls the whole
   screen down one row. A good idea to compile this piece
   of code with optimization enabled.
   Text-mode only!

	unsigned char * SetSegmentPtr(unsigned long adress);

   Sets the beginning of the 64k-memory segment to the
   adress specified by the unsigned long. If adress MOD 64
   is != 0, the return value will point to the segments
   start in the Amiga adress space + (adress MOD 64).
   Don't use more than (65536-64) bytes in the segment
   you set if you aren't sure that (adress MOD 64) == 0.
   See retina.doc from MS for further information.

	void ClearScreen(unsigned char color);

   Fills the whole screen with "color" - 256-color mode only!

	void LoadPalette(unsigned char * pal, unsigned char firstcol, 
	                 unsigned char colors);

   Loads the palette-registers. "pal" points to an array of unsigned char 
   triplets, for the red, green and blue component. "firstcol" determines the 
   number of the first palette-register to load (256 available). "colors" 
   is the number of colors you want to put in the palette registers.

	void SetPalette(unsigned char colornum, unsigned char red, 
	                unsigned char green, unsigned char blue);

   Allows you to set a single color in the palette, "colornum" is the number 
   of the palette entry (256 available), "red", "green" and "blue" are the 
   three components.

	void SetPanning(unsigned short xoff, unsigned short yoff);

   Moves the logical coordinate (xoff, yoff) to the upper left corner
   of your screen. Of course, you shouldn't specify excess values that would
   show garbage in the lower right area of your screen... SetPanning()
   does NOT check for boundaries.
*/

/* -------------- START OF CODE -------------- */

/* read VGA register */
#define vgar(ba, reg) (*(((volatile unsigned char *)ba)+reg))

/* write VGA register */
#define vgaw(ba, reg, val) \
	*(((volatile unsigned char *)ba)+reg) = val

/* defines for the used register addresses (mw)

   NOTE: there are some registers that have different addresses when
         in mono or color mode. We only support color mode, and thus
         some addresses won't work in mono-mode! */

/* General Registers: */
#define GREG_STATUS0_R		0x43C2
#define GREG_STATUS1_R		0x43DA
#define GREG_MISC_OUTPUT_R	0x43CC
#define GREG_MISC_OUTPUT_W	0x43C2	
#define GREG_FEATURE_CONTROL_R	0x43CA
#define GREG_FEATURE_CONTROL_W	0x43DA
#define GREG_POS		0x4102

/* Attribute Controller: */
#define ACT_ADDRESS		0x43C0
#define ACT_ADDRESS_R		0x03C0
#define ACT_ADDRESS_W		0x43C0
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
#define GCT_ADDRESS		0x43CE
#define GCT_ADDRESS_R		0x03CE
#define GCT_ADDRESS_W		0x03CE
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
#define SEQ_ADDRESS		0x43C4
#define SEQ_ADDRESS_R		0x03C4
#define SEQ_ADDRESS_W		0x03C4
#define SEQ_ID_RESET		0x00
#define SEQ_ID_CLOCKING_MODE	0x01
#define SEQ_ID_MAP_MASK		0x02
#define SEQ_ID_CHAR_MAP_SELECT	0x03
#define SEQ_ID_MEMORY_MODE	0x04
#define SEQ_ID_EXTENDED_ENABLE	0x05	/* down from here, all seq registers are NCR extensions */
#define SEQ_ID_UNKNOWN1         0x06	/* it does exist so it's probably usefull */
#define SEQ_ID_UNKNOWN2         0x07	/* it does exist so it's probably usefull */
#define SEQ_ID_CHIP_ID		0x08
#define SEQ_ID_UNKNOWN3         0x09	/* it does exist so it's probably usefull */
#define SEQ_ID_CURSOR_COLOR1	0x0A
#define SEQ_ID_CURSOR_COLOR0	0x0B
#define SEQ_ID_CURSOR_CONTROL	0x0C
#define SEQ_ID_CURSOR_X_LOC_HI	0x0D
#define SEQ_ID_CURSOR_X_LOC_LO	0x0E
#define SEQ_ID_CURSOR_Y_LOC_HI	0x0F
#define SEQ_ID_CURSOR_Y_LOC_LO	0x10
#define SEQ_ID_CURSOR_X_INDEX	0x11
#define SEQ_ID_CURSOR_Y_INDEX	0x12
#define SEQ_ID_CURSOR_STORE_HI	0x13	/* printed manual is wrong about these.. */
#define SEQ_ID_CURSOR_STORE_LO	0x14
#define SEQ_ID_CURSOR_ST_OFF_HI	0x15
#define SEQ_ID_CURSOR_ST_OFF_LO	0x16
#define SEQ_ID_CURSOR_PIXELMASK	0x17
#define SEQ_ID_PRIM_HOST_OFF_HI	0x18
#define SEQ_ID_PRIM_HOST_OFF_LO	0x19
#define SEQ_ID_DISP_OFF_HI	0x1A
#define SEQ_ID_DISP_OFF_LO	0x1B
#define SEQ_ID_SEC_HOST_OFF_HI	0x1C
#define SEQ_ID_SEC_HOST_OFF_LO	0x1D
#define SEQ_ID_EXTENDED_MEM_ENA	0x1E
#define SEQ_ID_EXT_CLOCK_MODE	0x1F
#define SEQ_ID_EXT_VIDEO_ADDR	0x20
#define SEQ_ID_EXT_PIXEL_CNTL	0x21
#define SEQ_ID_BUS_WIDTH_FEEDB	0x22
#define SEQ_ID_PERF_SELECT	0x23
#define SEQ_ID_COLOR_EXP_WFG	0x24
#define SEQ_ID_COLOR_EXP_WBG	0x25
#define SEQ_ID_EXT_RW_CONTROL	0x26
#define SEQ_ID_MISC_FEATURE_SEL	0x27
#define SEQ_ID_COLOR_KEY_CNTL	0x28
#define SEQ_ID_COLOR_KEY_MATCH	0x29
#define SEQ_ID_UNKNOWN4         0x2A
#define SEQ_ID_UNKNOWN5         0x2B
#define SEQ_ID_UNKNOWN6         0x2C
#define SEQ_ID_CRC_CONTROL	0x2D
#define SEQ_ID_CRC_DATA_LOW	0x2E
#define SEQ_ID_CRC_DATA_HIGH	0x2F

/* CRT Controller: */
#define CRT_ADDRESS		0x43D4
#define CRT_ADDRESS_R		0x03D4
#define CRT_ADDRESS_W		0x03D4
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
#define CRT_ID_OFFSET		0x13
#define CRT_ID_UNDERLINE_LOC	0x14
#define CRT_ID_START_VER_BLANK	0x15
#define CRT_ID_END_VER_BLANK	0x16
#define CRT_ID_MODE_CONTROL	0x17
#define CRT_ID_LINE_COMPARE	0x18
#define CRT_ID_UNKNOWN1         0x19	/* are these register really void ? */
#define CRT_ID_UNKNOWN2         0x1A
#define CRT_ID_UNKNOWN3         0x1B
#define CRT_ID_UNKNOWN4         0x1C
#define CRT_ID_UNKNOWN5         0x1D
#define CRT_ID_UNKNOWN6         0x1E
#define CRT_ID_UNKNOWN7         0x1F
#define CRT_ID_UNKNOWN8         0x20
#define CRT_ID_UNKNOWN9         0x21
#define CRT_ID_UNKNOWN10      	0x22
#define CRT_ID_UNKNOWN11      	0x23
#define CRT_ID_UNKNOWN12      	0x24
#define CRT_ID_UNKNOWN13      	0x25
#define CRT_ID_UNKNOWN14      	0x26
#define CRT_ID_UNKNOWN15      	0x27
#define CRT_ID_UNKNOWN16      	0x28
#define CRT_ID_UNKNOWN17      	0x29
#define CRT_ID_UNKNOWN18      	0x2A
#define CRT_ID_UNKNOWN19      	0x2B
#define CRT_ID_UNKNOWN20      	0x2C
#define CRT_ID_UNKNOWN21      	0x2D
#define CRT_ID_UNKNOWN22      	0x2E
#define CRT_ID_UNKNOWN23      	0x2F
#define CRT_ID_EXT_HOR_TIMING1	0x30	/* down from here, all crt registers are NCR extensions */
#define CRT_ID_EXT_START_ADDR	0x31
#define CRT_ID_EXT_HOR_TIMING2	0x32
#define CRT_ID_EXT_VER_TIMING	0x33

/* Video DAC (these are *pure* guesses from the usage of these registers, 
   I don't have a data sheet for this chip:-/) */
#define VDAC_REG_D		0x800d	/* well.. */
#define VDAC_REG_SELECT		0x8001	/* perhaps.. */
#define VDAC_REG_DATA		0x8003	/* dito.. */

#define WGfx(ba, idx, val) \
	do { vgaw(ba, GCT_ADDRESS, idx); vgaw(ba, GCT_ADDRESS_W , val); } while (0)

#define WSeq(ba, idx, val) \
	do { vgaw(ba, SEQ_ADDRESS, idx); vgaw(ba, SEQ_ADDRESS_W , val); } while (0)

#define WCrt(ba, idx, val) \
	do { vgaw(ba, CRT_ADDRESS, idx); vgaw(ba, CRT_ADDRESS_W , val); } while (0)

#define WAttr(ba, idx, val) \
	do { vgaw(ba, ACT_ADDRESS, idx); vgaw(ba, ACT_ADDRESS_W, val); } while (0)

#define Map(m) \
	do { WGfx(ba, GCT_ID_READ_MAP_SELECT, m & 3 ); WSeq(ba, SEQ_ID_MAP_MASK, (1 << (m & 3))); } while (0)

static __inline unsigned char RAttr(volatile void * ba, short idx) {
	vgaw (ba, ACT_ADDRESS, idx);
	return vgar (ba, ACT_ADDRESS_R);
}

static __inline unsigned char RSeq(volatile void * ba, short idx) {
	vgaw (ba, SEQ_ADDRESS, idx);
	return vgar (ba, SEQ_ADDRESS_R);
}

static __inline unsigned char RCrt(volatile void * ba, short idx) {
	vgaw (ba, CRT_ADDRESS, idx);
	return vgar (ba, CRT_ADDRESS_R);
}

static __inline unsigned char RGfx(volatile void * ba, short idx) {
	vgaw(ba, GCT_ADDRESS, idx);
	return vgar (ba, GCT_ADDRESS_R);
}

int grfrt_cnprobe __P((void));
void grfrt_iteinit __P((struct grf_softc *));
#endif /* _GRF_RTREG_H */
