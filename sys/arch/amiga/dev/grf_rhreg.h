/*	$NetBSD: grf_rhreg.h,v 1.5 1995/08/20 02:54:36 chopps Exp $	*/

/*
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
#ifndef _GRF_RHREG_H
#define _GRF_RHREG_H

#define EMPTY_ALPHA 0x2010 /* this is the char and the attribute
                              that AlphaErase will fill into the
                              text-screen  */

#define MEMSIZE 4        /* Set this to 1 or 4 (MB), according to the
                            RAM on your Retina BLT Z3 board */
/*
 * The following definitions are places in the frame-buffer memory
 * which are used for special purposes. While the displayed screen
 * itself is always beginning at the start of the frame-buffer
 * memory, the following special places are located at the end
 * of the memory to keep free as much space as possible for the
 * screen - the user might want to use monitor-definitions with
 * huge logical dimensions (e.g. 2048x2000 ?). This way of defining
 * special locations in the frame-buffer memory is far from being
 * elegant - you may want to use you own, real memory-management...
 * but remember that some routines in RZ3_BSD.cc REALLY NEED those
 * memory locations to function properly, so if you manage the
 * frame-buffer memory on your own, make sure to change those
 * definitions appropriately.
 */

/* reserve some space for one pattern line */
#define PAT_MEM_SIZE 16*3
#define PAT_MEM_OFF  (MEMSIZE*1024*1024 - PAT_MEM_SIZE)

/* reserve some space for the hardware cursor (up to 64x64 pixels) */
#define HWC_MEM_SIZE 1024
#define HWC_MEM_OFF  ((PAT_MEM_OFF - HWC_MEM_SIZE) & 0xffffff00)

/*
 * The following structure is passed to RZ3Init() and holds the
 * monitor-definition. You may either use one of the ready-made
 * definitions in RZ3_monitors.cc or you can define them on your
 * own, take a look at RZ3_monitors.cc for more information.
 */
struct MonDef {
	
	/* first the general monitor characteristics */
	
	unsigned long  FQ;
	unsigned char  FLG;
	
	unsigned short MW;  /* physical screen width in pixels    */
	                    /* has to be at least a multiple of 8 */
	unsigned short MH;  /* physical screen height in pixels   */
	
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
	
	unsigned short DEP;  /* Color-depth, 4 enables text-mode  */
	                     /* 8 enables 256-color graphics-mode, */
	                     /* 16 and 24bit gfx not supported yet */
	
	unsigned char * PAL; /* points to 16*3 byte RGB-palette data   */
	                     /* use LoadPalette() to set colors 0..255 */
	                     /* in 256-color-gfx mode */
	
	/*
	 * all following entries are font-specific in
	 * text-mode. Make sure your monitor
	 * parameters are calculated for the
	 * appropriate font width and height!
	 */
	
	unsigned short  TX;     /* Text-mode (DEP=4):          */
	                        /* screen-width  in characters */
	                        
	                        /* Gfx-mode (DEP > 4)          */
	                        /* "logical" screen-width,     */
	                        /* use values > MW to allow    */
	                        /* hardware-panning            */
	                        
	unsigned short  TY;     /* Text-mode:                  */
	                        /* screen-height in characters */
	
	                        /* Gfx-mode: "logical" screen  */
	                        /* height for panning          */
	
	/* the following values are currently unused for gfx-mode */
	
	unsigned short  XY;     /* TX*TY (speeds up some calcs.) */
	
	unsigned short  FX;     /* font-width (valid values: 4,7-16) */
	unsigned short  FY;     /* font-height (valid range: 1-32) */
	unsigned char * FData;  /* pointer to the font-data */
	
	/*
	 * The font data is simply an array of bytes defining
	 * the chars in ascending order, line by line. If your
	 * font is wider than 8 pixel, FData has to be an
	 * array of words.
	 */
	
	unsigned short  FLo;    /* lowest character defined */
	unsigned short  FHi;    /* highest char. defined */ 
	
};


/*
 * The following are the prototypes for the low-level
 * routines you may want to call.
 */

#if 0

#ifdef __GNUG__

/* The prototypes for C++, prototypes for C (with explanations) below */

"C" unsigned char * RZ3Init         (volatile void * HardWareAdress, struct MonDef * md);
"C" void            RZ3SetCursorPos (unsigned short pos);
"C" void            RZ3AlphaErase   (unsigned short xd, unsigned short yd,
                                            unsigned short  w, unsigned short  h );
"C" void            RZ3AlphaCopy    (unsigned short xs, unsigned short ys,
                                            unsigned short xd, unsigned short yd,
                                            unsigned short  w, unsigned short  h  );
"C" void            RZ3BitBlit      (struct grf_bitblt * gbb );
"C" void            RZ3BitBlit16    (struct grf_bitblt * gbb );
"C" void            RZ3LoadPalette  (unsigned char * pal, unsigned char firstcol, unsigned char colors);
"C" void            RZ3SetPalette   (unsigned char colornum, unsigned char red, unsigned char green, unsigned char blue);
"C" void            RZ3SetPanning   (unsigned short xoff, unsigned short yoff);
"C" void            RZ3SetupHWC     (unsigned char col1, unsigned col2,
                                            unsigned char hsx, unsigned char hsy,
                                            const unsigned long * data);
"C" void            RZ3DisableHWC   (void);
"C" void            RZ3SetHWCloc    (unsigned short x, unsigned short y);
#else

/* The prototypes for C */
/* with a little explanation */

	unsigned char * RZ3Init(volatile void * BoardAdress, struct MonDef * md);

/*
 * This routine initialises the Retina Z3 hardware, opens a
 * text- or gfx-mode screen, depending on the the value of
 * MonDef.DEP, and sets the cursor to position 0. 
 * It takes as arguments a pointer to the hardware-base
 * address as it is denoted in the DevConf structure
 * of the AmigaDOS, and a pointer to a struct MonDef
 * which describes the screen-mode parameters.
 * 
 * The routine returns 0 if it was unable to open the screen,
 * or an unsigned char * to the display memory when it
 * succeeded.
 *
 * The organisation of the display memory in text-mode is a
 * little strange (Intel-typically...) :
 * 
 * Byte  00    01    02    03    04     05    06   etc.
 *     Char0  Attr0  --    --   Char1 Attr1   --   etc.
 *     
 * You may set a character and its associated attribute byte
 * with a single word-access, or you may perform to byte writes
 * for the char and attribute. Each 2. word has no meaning,
 * and writes to theese locations are ignored.
 * 
 * The attribute byte for each character has the following
 * structure:
 * 
 * Bit  7     6     5     4     3     2     1     0
 *    BLINK BACK2 BACK1 BACK0 FORE3 FORE2 FORE1 FORE0
 *    
 * Were FORE is the foreground-color index (0-15) and
 * BACK is the background color index (0-7). BLINK 
 * enables blinking for the associated character.
 * The higher 8 colors in the standard palette are
 * lighter than the lower 8, so you may see FORE3 as
 * an intensity bit. If FORE == 1 or FORE == 9 and
 * BACK == 0 the character is underlined. Since I don't
 * think this looks good, it will probably change in a
 * future release.
 *
 * There's no routine "SetChar" or "SetAttr" provided,
 * because I think it's so trivial... a function call
 * would be pure overhead. As an example, a routine
 * to set the char code and attribute at position x,y:
 * (assumed the value returned by RZ3Init was stored
 *  into "DispMem", the actual MonDef struct * is hold
 *  in "MDef")
 * 
 * void SetChar(unsigned char chr, unsigned char attr,
 *              unsigned short x, unsigned short y) {
 *    
 *    unsigned struct MonDef * md = MDef;
 *    unsigned char * c = DispMem + x*4 + y*md->TX*4;
 *    
 *    *c++ = chr;
 *    *c   = attr;
 *    
 * }
 * 
 * In gfx-mode, the memory organisation is rather simple,
 * 1 byte per pixel in 256-color mode, one pixel after
 * each other, line by line.
 *
 * When 16-bits per pixel are used, each two bytes represent
 * one pixel. The meaning of the bits is the following:
 *
 * Bit       15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00
 * Component g2 g1 g0 b4 b3 b2 b1 b0 r4 r3 r2 r1 r0 g5 g4 g3
 *
 * Please note that the memory layout in gfx-mode depends
 * on the logical screen-size, panning does only affect
 * the appearance of the physical screen.
 * 
 * Currently, RZ3Init() disables the Retina Z3 VBLANK IRQ,
 * but beware: When running the Retina WB-Emu under
 * AmigaDOS, the VBLANK IRQ is ENABLED...
 * 
 */

	void RZ3LoadPalette(unsigned char * pal, unsigned char firstcol, unsigned char colors);

/*
 * Loads the palette-registers. "pal" points to an array of unsigned char
 * triplets, for the red, green and blue component. "firstcol" determines the
 * number of the first palette-register to load (256 available). "colors" is
 * the number of colors you want to put in the palette registers.
 */

	void RZ3SetPalette(unsigned char colornum, unsigned char red, unsigned char green, unsigned char blue);

/*
 * Allows you to set a single color in the palette, "colornum" is the number
 * of the palette entry (256 available), "red", "green" and "blue" are the
 * three components.
 */

	void RZ3SetCursorPos(unsigned short pos);

/*
 * This routine sets the text-mode hardware-cursor position to the screen
 * location pos. pos can be calculated as (x + y * md->TY).
 * Text-mode only!
 */

	void RZ3AlphaCopy (unsigned short xs, unsigned short ys,
                      unsigned short xd, unsigned short yd,
                      unsigned short  w, unsigned short  h  );

/*
 * This Routine utilizes the blitter to perform fast copies
 * in the text-display. The paramters are:
 *  xs - source x-coordinate
 *  ys - source y-coordinate
 *  xd - destination x-coordinate
 *  yd - destination y-coordinate
 *  w  - the width of the area to copy
 *  h  - the height of the area to copy
 * All coordinates are in characters. RZ3AlphaCopy does not
 * check for boundaries - you've got to make sure that the
 * parameters have sensible values. Text-mode only!
 */


	void RZ3AlphaErase (unsigned short xd, unsigned short yd,
                       unsigned short  w, unsigned short  h );

/*
 * RZ3AlphaErase utilizes the blitter to erase portions of
 * the text-display. The parameters are:
 *  xd - destination x-coordinate
 *  yd - destination y-coordinate
 *  w  - the width of the area to erase
 *  h  - the height of the area to erase
 * All coordinates are in characters. RZ3AlphaCopy does not
 * check for boundaries - you've got to make sure that the
 * parameters have sensible values. Text-mode only!
 * 
 * Since the blitter is unable to use a mask-pattern and a
 * certain fill-value at the same time, this routine uses
 * a simple trick: RZ3Init() clears a memory area twice as
 * large as the text-display needs, and RZ3AlphaErase then
 * simply uses RZ3AlphaCopy to copy the desired area from
 * the empty text-screen to the actually displayed screen.
 */

	void RZ3BitBlit (struct grf_bitblt * gbb );

/*
 * RZ3BitBlit utilizes the blitter to perform one of 16
 * available logical operations on the display memory,
 * among them ordinary fill- and copy operations.
 * The only parameter is a pointer to a struct grf_bitblt:
 * 
 * struct grf_bitblt {
 *   unsigned short op;              see above definitions of GRFBBOPxxx
 *   unsigned short src_x, src_y;    upper left corner of source-region
 *   unsigned short dst_x, dst_y;    upper left corner of dest-region
 *   unsigned short w, h;            width, height of region 
 *   unsigned short mask;            bitmask to apply
 * };
 * 
 * All coordinates are in pixels. RZ3BitBlit does not
 * check for boundaries - you've got to make sure that the
 * parameters have sensible values. 8 bit gfx-mode only!
 *
 * The blitter has a lot more capabilities, which aren't
 * currently used by theese routines, among them color-expanded
 * and text-blits, which can speed up GUIs like X11 a lot.
 * If you've got any idea how to make use of them within
 * your routines, contact me, and I'll implement the necessary
 * blit-operations.
 */

	void RZ3BitBlit16( struct grf_bitblt * gbb );

/* Does the same as RZ3BitBlit(), but for 16-bit screens */

	void RZ3SetPanning(unsigned short xoff, unsigned short yoff);

/*
 * Moves the logical coordinate (xoff, yoff) to the upper left corner
 * of your screen. Of course, you shouldn't specify excess values that would
 * show garbage in the lower right area of your screen... SetPanning()
 * does NOT check for boundaries.
 * Please read the documentation of RZ3SetHWCloc, too.
 */

	void RZ3SetupHWC (unsigned char col1, unsigned col2,
                     unsigned char hsx, unsigned char hsy,
                     const unsigned long * data);

/*
 * Initializes and switches on the hardware-cursor sprite.
 * The parameters are:
 * col1     - the first color
 * col2     - the second color
 * hsx      - hot-spot location offset x
 * hsy      - hot-spot location offset y
 * data     - a pointer to the bitmap data to be used for the sprite
 *
 * The organization of the data is - as always with MSDOS related
 * products - rather strange: The first and the second long-word
 * represent bitplane0 for the first 64 pixels. The following two
 * long-words represent bitplane1 for the first 64 pixels. But
 * the long-words are organized in Intel-fashion, beginning with
 * the least significant byte, ending with the most significant
 * one. The most significant bit of each byte is the leftmost,
 * as one would expect it. Now the weird color-assignments:
 *
 * bitplane0 bitplane1       result
 *     0         0            col2
 *     0         1            col1
 *     1         0          transparent
 *     1         1     background-color XOR 0xff
 *     
 * The size of the data has to be 64*64*2/8 = 1024 byte,
 * obviously, the size of the sprite is 64x64 pixels.
 */


	void RZ3DisableHWC (void);

/* simply disables the hardware-cursor sprite */

	void RZ3SetHWCloc (unsigned short x, unsigned short y);

/*
 * sets the location of the hardwar-cursor sprite to x,y
 * relative to the logical screen beginning.
 * IMPORTANT: If you use RZ3SetHWCloc() to set the position
 * of the hardware-cursor sprite, all necessary panning is
 * done automatically - you can treat the display without
 * even knowing about the physical screen size that is
 * displayed. 
 */

#endif

#endif /* RZ3_BSD_h */


/* -------------- START OF CODE -------------- */

/* read VGA register */
#define vgar(ba, reg) (*(((volatile unsigned char *)ba)+reg))

/* write VGA register */
#define vgaw(ba, reg, val) \
	*(((volatile unsigned char *)ba)+reg) = val

/*
 * defines for the used register addresses (mw)
 *
 * NOTE: there are some registers that have different addresses when
 *       in mono or color mode. We only support color mode, and thus
 *       some addresses won't work in mono-mode!
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
#define ACT_ADDRESS_R		0x03C0
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
#define GCT_ADDRESS_R		0x03CE
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
#define SEQ_ADDRESS_R		0x03C4
#define SEQ_ADDRESS_W		0x03C5
#define SEQ_ID_RESET		0x00
#define SEQ_ID_CLOCKING_MODE	0x01
#define SEQ_ID_MAP_MASK		0x02
#define SEQ_ID_CHAR_MAP_SELECT	0x03
#define SEQ_ID_MEMORY_MODE	0x04
#define SEQ_ID_EXTENDED_ENABLE	0x05	/* down from here, all seq registers are NCR extensions */
#define SEQ_ID_UNKNOWN1         0x06
#define SEQ_ID_UNKNOWN2         0x07
#define SEQ_ID_CHIP_ID		0x08
#define SEQ_ID_UNKNOWN3         0x09
#define SEQ_ID_CURSOR_COLOR1	0x0A
#define SEQ_ID_CURSOR_COLOR0	0x0B
#define SEQ_ID_CURSOR_CONTROL	0x0C
#define SEQ_ID_CURSOR_X_LOC_HI	0x0D
#define SEQ_ID_CURSOR_X_LOC_LO	0x0E
#define SEQ_ID_CURSOR_Y_LOC_HI	0x0F
#define SEQ_ID_CURSOR_Y_LOC_LO	0x10
#define SEQ_ID_CURSOR_X_INDEX	0x11
#define SEQ_ID_CURSOR_Y_INDEX	0x12
#define SEQ_ID_CURSOR_STORE_HI	0x13	/* manual still wrong here.. argl! */
#define SEQ_ID_CURSOR_STORE_LO	0x14	/* downto 0x16 */
#define SEQ_ID_CURSOR_ST_OFF_HI	0x15
#define SEQ_ID_CURSOR_ST_OFF_LO	0x16
#define SEQ_ID_CURSOR_PIXELMASK	0x17
#define SEQ_ID_PRIM_HOST_OFF_HI	0x18
#define SEQ_ID_PRIM_HOST_OFF_LO	0x19
#define SEQ_ID_LINEAR_0		0x1A
#define SEQ_ID_LINEAR_1		0x1B
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
#define SEQ_ID_COLOR_KEY_MATCH0	0x29
#define SEQ_ID_COLOR_KEY_MATCH1 0x2A
#define SEQ_ID_COLOR_KEY_MATCH2 0x2B
#define SEQ_ID_UNKNOWN6         0x2C
#define SEQ_ID_CRC_CONTROL	0x2D
#define SEQ_ID_CRC_DATA_LOW	0x2E
#define SEQ_ID_CRC_DATA_HIGH	0x2F
#define SEQ_ID_MEMORY_MAP_CNTL	0x30
#define SEQ_ID_ACM_APERTURE_1	0x31
#define SEQ_ID_ACM_APERTURE_2	0x32
#define SEQ_ID_ACM_APERTURE_3	0x33
#define SEQ_ID_BIOS_UTILITY_0	0x3e
#define SEQ_ID_BIOS_UTILITY_1	0x3f

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
#define CRT_ID_MONITOR_POWER	0x34

/* PLL chip  (clock frequency synthesizer) I'm guessing here... */
#define PLL_ADDRESS		0x83c8
#define PLL_ADDRESS_W		0x83c9


/* Video DAC */
#define VDAC_ADDRESS		0x03c8
#define VDAC_ADDRESS_W		0x03c8
#define VDAC_ADDRESS_R		0x03c7
#define VDAC_STATE		0x03c7
#define VDAC_DATA		0x03c9
#define VDAC_MASK		0x03c6


/* Accelerator Control Menu (memory mapped registers, includes blitter) */
#define ACM_PRIMARY_OFFSET	0x00
#define ACM_SECONDARY_OFFSET	0x04
#define ACM_MODE_CONTROL	0x08
#define ACM_CURSOR_POSITION	0x0c
#define ACM_START_STATUS	0x30
#define ACM_CONTROL		0x34
#define ACM_RASTEROP_ROTATION	0x38
#define ACM_BITMAP_DIMENSION	0x3c
#define ACM_DESTINATION		0x40
#define ACM_SOURCE		0x44
#define ACM_PATTERN		0x48
#define ACM_FOREGROUND		0x4c
#define ACM_BACKGROUND		0x50


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

#define WPLL(ba, idx, val) \
	do { 	vgaw(ba, PLL_ADDRESS, idx);\
	vgaw(ba, PLL_ADDRESS_W, (val & 0xff));\
	vgaw(ba, PLL_ADDRESS_W, (val >> 8)); } while (0)


static inline unsigned char RAttr(volatile void * ba, short idx) {
	vgaw (ba, ACT_ADDRESS, idx);
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

void RZ3DisableHWC __P((struct grf_softc *gp));
void RZ3SetupHWC __P((struct grf_softc *gp, unsigned char col1, unsigned int col2, unsigned char hsx, unsigned char hsy, const long unsigned int *data));
void RZ3AlphaErase __P((struct grf_softc *gp, short unsigned int xd, short unsigned int yd, short unsigned int w, short unsigned int h));
void RZ3AlphaCopy __P((struct grf_softc *gp, short unsigned int xs, short unsigned int ys, short unsigned int xd, short unsigned int yd, short unsigned int w, short unsigned int h));
void RZ3BitBlit __P((struct grf_softc *gp, struct grf_bitblt *gbb));
void RZ3BitBlit16 __P((struct grf_softc *gp, struct grf_bitblt *gbb));
void RZ3SetCursorPos __P((struct grf_softc *gp, short unsigned int pos));
void RZ3LoadPalette __P((struct grf_softc *gp, unsigned char *pal, unsigned char firstcol, unsigned char colors));
void RZ3SetPalette __P((struct grf_softc *gp, unsigned char colornum, unsigned char red, unsigned char green, unsigned char blue));
void RZ3SetPanning __P((struct grf_softc *gp, short unsigned int xoff, short unsigned int yoff));
void RZ3SetHWCloc __P((struct grf_softc *gp, short unsigned int x, short unsigned int y));
int rh_mode __P((register struct grf_softc *gp, int cmd, void *arg, int a2, int a3));
int rh_ioctl __P((register struct grf_softc *gp, u_long cmd, void *data));
int rh_getcmap __P((struct grf_softc *gfp, struct grf_colormap *cmap));
int rh_putcmap __P((struct grf_softc *gfp, struct grf_colormap *cmap));
int rh_getspritepos __P((struct grf_softc *gp, struct grf_position *pos));
int rh_setspritepos __P((struct grf_softc *gp, struct grf_position *pos));
int rh_getspriteinfo __P((struct grf_softc *gp, struct grf_spriteinfo *info));
int rh_setspriteinfo __P((struct grf_softc *gp, struct grf_spriteinfo *info));
int rh_getspritemax __P((struct grf_softc *gp, struct grf_position *pos));
int rh_bitblt __P((struct grf_softc *gp, struct grf_bitblt *bb));


struct ite_softc;
void rh_init __P((struct ite_softc *));
void rh_cursor __P((struct ite_softc *, int));
void rh_deinit __P((struct ite_softc *));
void rh_putc __P((struct ite_softc *, int, int, int, int));
void rh_clear __P((struct ite_softc *, int, int, int, int));
void rh_scroll __P((struct ite_softc *, int, int, int, int));

#endif /* _GRF_RHREG_H */
