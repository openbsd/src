/*	$NetBSD: grf_rt.c,v 1.23 1996/01/28 19:19:12 chopps Exp $	*/

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
#include "grfrt.h"
#if NGRFRT > 0

/* Graphics routines for the Retina board, 
   using the NCR 77C22E+ VGA controller. */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <amiga/amiga/device.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/dev/grfioctl.h>
#include <amiga/dev/grfvar.h>
#include <amiga/dev/grf_rtreg.h>

int rt_ioctl __P((struct grf_softc *gp, u_long, void *));

/*
 * marked true early so that retina_cnprobe() can tell if we are alive. 
 */
int retina_inited;


/*
 * This driver for the MacroSystem Retina board was only possible,
 * because MacroSystem provided information about the pecularities
 * of the board. THANKS! Competition in Europe among gfx board 
 * manufacturers is rather tough, so Lutz Vieweg, who wrote the
 * initial driver, has made an agreement with MS not to document
 * the driver source (see also his comment below).
 * -> ALL comments after 
 * -> "/* -------------- START OF CODE -------------- * /"
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

extern unsigned char kernel_font_8x8_width, kernel_font_8x8_height;
extern unsigned char kernel_font_8x8_lo, kernel_font_8x8_hi;
extern unsigned char kernel_font_8x8[];


#define MDF_DBL 1      
#define MDF_LACE 2     
#define MDF_CLKDIV2 4  


/* standard-palette definition */

unsigned char NCRStdPalette[16*3] = {
/*   R   G   B  */
	  0,  0,  0,
	192,192,192,
	128,  0,  0,  
	  0,128,  0,
	  0,  0,128,
	128,128,  0,  
	  0,128,128,  
	128,  0,128,
	 64, 64, 64, /* the higher 8 colors have more intensity for  */
	255,255,255, /* compatibility with standard attributes       */
	255,  0,  0,  
	  0,255,  0,
	  0,  0,255,
	255,255,  0,  
	  0,255,255,  
	255,  0,255  
};


/* The following structures are examples for monitor-definitions. To make one
   of your own, first use "DefineMonitor" and create the 8-bit monitor-mode of
   your dreams. Then save it, and make a structure from the values provided in
   the file DefineMonitor stored - the labels in the comment above the
   structure definition show where to put what value.
   
   Then you'll need to adapt your monitor-definition to the font you want to
   use. Be FX the width of the font, then the following modifications have to
   be applied to your values:
   
   HBS = (HBS * 4) / FX
   HSS = (HSS * 4) / FX
   HSE = (HSE * 4) / FX
   HBE = (HBE * 4) / FX
   HT  = (HT  * 4) / FX

   Make sure your maximum width (MW) and height (MH) are even multiples of
   the fonts' width and height.
*/

#if 0
/* horizontal 31.5 kHz */

/*                                      FQ     FLG    MW   MH   HBS HSS HSE HBE  HT  VBS  VSS  VSE  VBE   VT  */
   struct MonDef MON_640_512_60  = { 50000000,  28,  640, 512,   81, 86, 93, 98, 95, 513, 513, 521, 535, 535,
   /* Depth,           PAL, TX,  TY,    XY,FontX, FontY,    FontData,  FLo,  Fhi */
          4, NCRStdPalette, 80,  64,  5120,    8,     8, kernel_font_8x8,   32,  255};      

 struct MonDef MON_640_480_62_G  = { 50000000,   4,  640, 480,  161,171,184,196,195, 481, 484, 492, 502, 502,
          8, NCRStdPalette,640,480,  5120,    8,     8, kernel_font_8x8,   32,  255};      
/* Enter higher values here ^   ^ for panning! */

/* horizontal 38kHz */

   struct MonDef MON_768_600_60  = { 75000000,  28,  768, 600,   97, 99,107,120,117, 601, 615, 625, 638, 638,
          4, NCRStdPalette, 96,  75,  7200,    8,     8, kernel_font_8x8,   32,  255};      

/* horizontal 64kHz */

   struct MonDef MON_768_600_80  = { 50000000, 24,  768, 600,   97,104,112,122,119, 601, 606, 616, 628, 628,
          4, NCRStdPalette, 96,  75,  7200,    8,     8, kernel_font_8x8,   32,  255};      

   struct MonDef MON_1024_768_80 = { 90000000, 24, 1024, 768,  129,130,141,172,169, 769, 770, 783, 804, 804,
          4, NCRStdPalette,128,  96, 12288,    8,     8, kernel_font_8x8,   32,  255};      

/*                                     FQ     FLG    MW   MH   HBS HSS HSE HBE  HT  VBS  VSS  VSE  VBE   VT  */
 struct MonDef MON_1024_768_80_G = { 90000000, 0,  1024, 768,  257,258,280,344,343, 769, 770, 783, 804, 804,
          8, NCRStdPalette, 1024, 768, 12288,    8,     8, kernel_font_8x8,   32,  255};      

   struct MonDef MON_1024_1024_59= { 90000000, 24, 1024,1024,  129,130,141,173,170,1025,1059,1076,1087,1087,
          4, NCRStdPalette,128, 128, 16384,    8,     8, kernel_font_8x8,   32,  255};      

/* WARNING: THE FOLLOWING MONITOR MODES EXCEED THE 90-MHz LIMIT THE PROCESSOR
            HAS BEEN SPECIFIED FOR. USE AT YOUR OWN RISK (AND THINK ABOUT
            MOUNTING SOME COOLING DEVICE AT THE PROCESSOR AND RAMDAC)!     */

   struct MonDef MON_1280_1024_60= {110000000,  24, 1280,1024,  161,162,176,211,208,1025,1026,1043,1073,1073,
          4, NCRStdPalette,160, 128, 20480,    8,     8, kernel_font_8x8,   32,  255};      

 struct MonDef MON_1280_1024_60_G= {110000000,   0, 1280,1024,  321,322,349,422,421,1025,1026,1043,1073,1073,
          8, NCRStdPalette,1280,1024, 20480,    8,     8, kernel_font_8x8,   32,  255};      

/* horizontal 75kHz */

   struct MonDef MON_1280_1024_69= {120000000,  24, 1280,1024,  161,162,175,200,197,1025,1026,1043,1073,1073, 
          4, NCRStdPalette,160, 128, 20480,    8,     8, kernel_font_8x8,   32,  255};      

#else

struct MonDef monitor_defs[] = {
/* horizontal 31.5 kHz */

   { 50000000,  28,  640, 512,   81, 86, 93, 98, 95, 513, 513, 521, 535, 535,
          4, NCRStdPalette, 80,  64,  5120,    8,     8, kernel_font_8x8,   32,  255},

/* horizontal 38kHz */

   { 75000000,  28,  768, 600,   97, 99,107,120,117, 601, 615, 625, 638, 638,
          4, NCRStdPalette, 96,  75,  7200,    8,     8, kernel_font_8x8,   32,  255},

/* horizontal 64kHz */

   { 50000000, 24,  768, 600,   97,104,112,122,119, 601, 606, 616, 628, 628,
          4, NCRStdPalette, 96,  75,  7200,    8,     8, kernel_font_8x8,   32,  255},
   
   { 90000000, 24, 1024, 768,  129,130,141,172,169, 769, 770, 783, 804, 804,
          4, NCRStdPalette,128,  96, 12288,    8,     8, kernel_font_8x8,   32,  255},

   /* GFX modes */

/* horizontal 31.5 kHz */

   { 50000000,   4,  640, 480,  161,171,184,196,195, 481, 484, 492, 502, 502,
          8, NCRStdPalette,640, 480,  5120,    8,     8, kernel_font_8x8,   32,  255},

/* horizontal 64kHz */

   { 90000000, 0,  1024, 768,  257,258,280,344,343, 769, 770, 783, 804, 804,
          8, NCRStdPalette, 1024, 768, 12288,    8,     8, kernel_font_8x8,   32,  255},

/* WARNING: THE FOLLOWING MONITOR MODES EXCEED THE 90-MHz LIMIT THE PROCESSOR
            HAS BEEN SPECIFIED FOR. USE AT YOUR OWN RISK (AND THINK ABOUT
            MOUNTING SOME COOLING DEVICE AT THE PROCESSOR AND RAMDAC)!     */

   {110000000,   0, 1280,1024,  321,322,349,422,421,1025,1026,1043,1073,1073,
          8, NCRStdPalette,1280,1024, 20480,    8,     8, kernel_font_8x8,   32,  255},
};

static const char *monitor_descr[] = {
  "80x64 (640x512) 31.5kHz",
  "96x75 (768x600) 38kHz",
  "96x75 (768x600) 64kHz",
  "128x96 (1024x768) 64kHz",

  "GFX (640x480) 31.5kHz",
  "GFX (1024x768) 64kHz",
  "GFX (1280x1024) 64kHz ***EXCEEDS CHIP LIMIT!!!***", 
};

int retina_mon_max = sizeof (monitor_defs)/sizeof (monitor_defs[0]);

/* patchable */
int retina_default_mon = 0;
int retina_default_gfx = 4;
    
#endif


static struct MonDef *current_mon;

/* -------------- START OF CODE -------------- */


static const long FQTab[16] =
{ 25175000,  28322000,  36000000,  65000000,
  44900000,  50000000,  80000000,  75000000,
  56644000,  63000000,  72000000, 130000000,
  90000000, 100000000, 110000000, 120000000 };


/*--------------------------------------------------*/
/*--------------------------------------------------*/

#if 0
static struct MonDef *default_monitor = &DEFAULT_MONDEF;
#endif

/*
 * used to query the retina to see if its alive (?)
 */
int
retina_alive(mdp)
	struct MonDef *mdp;
{
	short clksel;

	for (clksel = 15; clksel; clksel--) {
		if (FQTab[clksel] == mdp->FQ)
			break;
	}
	if (clksel < 0) 
		return(0);
	if (mdp->DEP != 4)
		return(1);
	if (mdp->FX == 4 || (mdp->FX >= 7 && mdp->FX <= 16))
		return(1);
	return(0);
}

static int
rt_load_mon(gp, md)
	struct grf_softc *gp;
	struct MonDef *md;
{
	struct grfinfo *gi = &gp->g_display;
	volatile unsigned char *ba;
	volatile unsigned char *fb;
	short FW, clksel, HDE, VDE;

	for (clksel = 15; clksel; clksel--) {
		if (FQTab[clksel] == md->FQ) break;
	}
	if (clksel < 0)
		return(0);
	
	ba = gp->g_regkva;;
	fb = gp->g_fbkva;

	FW = 0;
	if (md->DEP == 4) {
		switch (md->FX) {
		case 4:
			FW = 0;
			break;
		case 7:
			FW = 1;
			break;
		case 8:
			FW = 2;
			break;
		case 9:
			FW = 3;
			break;
		case 10:
			FW = 4;
			break;
		case 11:
			FW = 5;
			break;
		case 12:
			FW = 6;
			break;
		case 13:
			FW = 7;
			break;
		case 14:
			FW = 8;
			break;
		case 15:
			FW = 9;
			break;
		case 16:
			FW = 11;
			break;
		default:
			return(0);
			break;
		};
	}
	
        if (md->DEP == 4) HDE = (md->MW+md->FX-1)/md->FX;
        else              HDE = (md->MW+3)/4;
	VDE = md->MH-1;
	
	/* hmm... */
	fb[0x8000] = 0;

		/* enable extension registers */
	WSeq (ba, SEQ_ID_EXTENDED_ENABLE,	0x05);

#if 0
	/* program the clock oscillator */	
	vgaw (ba, GREG_MISC_OUTPUT_W, 0xe3 | ((clksel & 3) * 0x04));
	vgaw (ba, GREG_FEATURE_CONTROL_W, 0x00);
	
	/* XXXX according to the NCR specs, this register should be set to 1
	   XXXX before doing the MISC_OUTPUT setting and CLOCKING_MODE
	   XXXX setting. */
	WSeq (ba, SEQ_ID_RESET, 		0x03);  

	WSeq (ba, SEQ_ID_CLOCKING_MODE, 	0x01 | ((md->FLG & MDF_CLKDIV2)/ MDF_CLKDIV2 * 8));  
	WSeq (ba, SEQ_ID_MAP_MASK, 		0x0f);  
	WSeq (ba, SEQ_ID_CHAR_MAP_SELECT, 	0x00);
		/* odd/even write select + extended memory */  
	WSeq (ba, SEQ_ID_MEMORY_MODE, 	0x06);
	/* XXXX I think this order of setting RESET is wrong... */
	WSeq (ba, SEQ_ID_RESET, 		0x01);  
	WSeq (ba, SEQ_ID_RESET, 		0x03);  
#else
	WSeq (ba, SEQ_ID_RESET, 		0x01);  

		/* set font width + rest of clocks */
	WSeq (ba, SEQ_ID_EXT_CLOCK_MODE,	0x30 | (FW & 0x0f) | ((clksel & 4) / 4 * 0x40) );  
		/* another clock bit, plus hw stuff */  
	WSeq (ba, SEQ_ID_MISC_FEATURE_SEL,	0xf4 | (clksel & 8) );  

	/* program the clock oscillator */	
	vgaw (ba, GREG_MISC_OUTPUT_W, 		0xe3 | ((clksel & 3) * 0x04));
	vgaw (ba, GREG_FEATURE_CONTROL_W, 	0x00);
	
	WSeq (ba, SEQ_ID_CLOCKING_MODE, 	0x01 | ((md->FLG & MDF_CLKDIV2)/ MDF_CLKDIV2 * 8));  
	WSeq (ba, SEQ_ID_MAP_MASK, 		0x0f);  
	WSeq (ba, SEQ_ID_CHAR_MAP_SELECT, 	0x00);
		/* odd/even write select + extended memory */  
	WSeq (ba, SEQ_ID_MEMORY_MODE, 		0x06);
	WSeq (ba, SEQ_ID_RESET, 		0x03);  
#endif
	
		/* monochrome cursor */
	WSeq (ba, SEQ_ID_CURSOR_CONTROL,	0x00);  
		/* bank0 */
	WSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI,	0x00);  
	WSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO,	0x00);  
	WSeq (ba, SEQ_ID_DISP_OFF_HI , 		0x00);
	WSeq (ba, SEQ_ID_DISP_OFF_LO , 		0x00);
		/* bank0 */
	WSeq (ba, SEQ_ID_SEC_HOST_OFF_HI,	0x00);  
	WSeq (ba, SEQ_ID_SEC_HOST_OFF_LO,	0x00);  
		/* 1M-chips + ena SEC + ena EMem + rw PrimA0/rw Sec/B0 */
	WSeq (ba, SEQ_ID_EXTENDED_MEM_ENA,	0x3 | 0x4 | 0x10 | 0x40);
#if 0
		/* set font width + rest of clocks */
	WSeq (ba, SEQ_ID_EXT_CLOCK_MODE,	0x30 | (FW & 0x0f) | ((clksel & 4) / 4 * 0x40) );  
#endif
	if (md->DEP == 4) {
			/* no ext-chain4 + no host-addr-bit-16 */
		WSeq (ba, SEQ_ID_EXT_VIDEO_ADDR,	0x00);  
			/* no packed/nibble + no 256bit gfx format */
		WSeq (ba, SEQ_ID_EXT_PIXEL_CNTL,	0x00);
	}
	else {
		WSeq (ba, SEQ_ID_EXT_VIDEO_ADDR,	0x02);  
			/* 256bit gfx format */
		WSeq (ba, SEQ_ID_EXT_PIXEL_CNTL,	0x01);
	}
		/* AT-interface */
	WSeq (ba, SEQ_ID_BUS_WIDTH_FEEDB,	0x06);  
		/* see fg/bg color expansion */
	WSeq (ba, SEQ_ID_COLOR_EXP_WFG,		0x01); 
	WSeq (ba, SEQ_ID_COLOR_EXP_WBG,		0x00);
	WSeq (ba, SEQ_ID_EXT_RW_CONTROL,	0x00);
#if 0
		/* another clock bit, plus hw stuff */  
	WSeq (ba, SEQ_ID_MISC_FEATURE_SEL,	0xf4 | (clksel & 8) );  
#endif
		/* don't tristate PCLK and PIX */
	WSeq (ba, SEQ_ID_COLOR_KEY_CNTL,	0x40 ); 
		/* reset CRC circuit */
	WSeq (ba, SEQ_ID_CRC_CONTROL,		0x00 ); 
		/* set RAS/CAS swap */
	WSeq (ba, SEQ_ID_PERF_SELECT,		0x20);  
	
	WCrt (ba, CRT_ID_END_VER_RETR,		(md->VSE & 0xf ) | 0x20); 
	WCrt (ba, CRT_ID_HOR_TOTAL,		md->HT   & 0xff);
	WCrt (ba, CRT_ID_HOR_DISP_ENA_END,	(HDE-1)  & 0xff);
	WCrt (ba, CRT_ID_START_HOR_BLANK,	md->HBS  & 0xff);
	WCrt (ba, CRT_ID_END_HOR_BLANK,		(md->HBE & 0x1f) | 0x80);
	
	WCrt (ba, CRT_ID_START_HOR_RETR,	md->HSS  & 0xff);
	WCrt (ba, CRT_ID_END_HOR_RETR,		(md->HSE & 0x1f) | ((md->HBE & 0x20)/ 0x20 * 0x80));
	WCrt (ba, CRT_ID_VER_TOTAL,		(md->VT  & 0xff));
	WCrt (ba, CRT_ID_OVERFLOW,		(( (md->VSS  & 0x200) / 0x200 * 0x80) 
						 | ((VDE     & 0x200) / 0x200 * 0x40) 
						 | ((md->VT  & 0x200) / 0x200 * 0x20)
						 | 				0x10
						 | ((md->VBS & 0x100) / 0x100 * 8   )
						 | ((md->VSS & 0x100) / 0x100 * 4   ) 
						 | ((VDE     & 0x100) / 0x100 * 2   ) 
						 | ((md->VT  & 0x100) / 0x100       )));
	WCrt (ba, CRT_ID_PRESET_ROW_SCAN,	0x00);
	
	if (md->DEP == 4) {
		WCrt (ba, CRT_ID_MAX_SCAN_LINE, 	((  (md->FLG & MDF_DBL)/ MDF_DBL * 0x80) 
							 | 				   0x40 
							 | ((md->VBS & 0x200)/0x200	 * 0x20) 
							 | ((md->FY-1) 			 & 0x1f)));
	}
	else {
		WCrt (ba, CRT_ID_MAX_SCAN_LINE, 	((  (md->FLG & MDF_DBL)/ MDF_DBL * 0x80) 
							 | 				   0x40 
							 | ((md->VBS & 0x200)/0x200	 * 0x20) 
							 | (0	 			 & 0x1f)));
	}
	
	WCrt (ba, CRT_ID_CURSOR_START,		(md->FY & 0x1f) - 2);
	WCrt (ba, CRT_ID_CURSOR_END,		(md->FY & 0x1f) - 1);
	
	WCrt (ba, CRT_ID_START_ADDR_HIGH,	0x00);
	WCrt (ba, CRT_ID_START_ADDR_LOW,	0x00);
	
	WCrt (ba, CRT_ID_CURSOR_LOC_HIGH,	0x00);
	WCrt (ba, CRT_ID_CURSOR_LOC_LOW,	0x00);
	
	WCrt (ba, CRT_ID_START_VER_RETR,	md->VSS    & 0xff);
	WCrt (ba, CRT_ID_END_VER_RETR,		(md->VSE   & 0x0f) | 0x80 | 0x20); 
	WCrt (ba, CRT_ID_VER_DISP_ENA_END,	VDE        & 0xff);
	if (md->DEP == 4)
		WCrt (ba, CRT_ID_OFFSET,	(HDE / 2)  & 0xff);
	else
		WCrt (ba, CRT_ID_OFFSET,	(md->TX / 8)  & 0xff);

	WCrt (ba, CRT_ID_UNDERLINE_LOC,		(md->FY-1) & 0x1f);
	WCrt (ba, CRT_ID_START_VER_BLANK,	md->VBS    & 0xff);
	WCrt (ba, CRT_ID_END_VER_BLANK,		md->VBE    & 0xff);
		/* byte mode + wrap + select row scan counter + cms */
	WCrt (ba, CRT_ID_MODE_CONTROL,		0xe3); 
	WCrt (ba, CRT_ID_LINE_COMPARE,		0xff); 
	
		/* enable extended end bits + those bits */
	WCrt (ba, CRT_ID_EXT_HOR_TIMING1,	(           				 0x20 
						 | ((md->FLG & MDF_LACE)  / MDF_LACE   * 0x10) 
						 | ((md->HT  & 0x100) / 0x100          * 0x01) 
						 | (((HDE-1) & 0x100) / 0x100 	       * 0x02) 
						 | ((md->HBS & 0x100) / 0x100 	       * 0x04) 
						 | ((md->HSS & 0x100) / 0x100 	       * 0x08)));
	             
	if (md->DEP == 4)
		WCrt (ba, CRT_ID_EXT_START_ADDR,	(((HDE / 2) & 0x100)/0x100 * 16)); 
	else
		WCrt (ba, CRT_ID_EXT_START_ADDR,	(((md->TX / 8) & 0x100)/0x100 * 16)); 
	
	WCrt (ba, CRT_ID_EXT_HOR_TIMING2, 	(  ((md->HT  & 0x200)/ 0x200  	       * 0x01) 
						 | (((HDE-1) & 0x200)/ 0x200 	       * 0x02) 
						 | ((md->HBS & 0x200)/ 0x200 	       * 0x04)
						 | ((md->HSS & 0x200)/ 0x200 	       * 0x08) 
						 | ((md->HBE & 0xc0) / 0x40  	       * 0x10)
						 | ((md->HSE & 0x60) / 0x20  	       * 0x40)));
	             
	WCrt (ba, CRT_ID_EXT_VER_TIMING,	(  ((md->VSE & 0x10) / 0x10  	       * 0x80) 
						 | ((md->VBE & 0x300)/ 0x100 	       * 0x20) 
						 |				         0x10 
						 | ((md->VSS & 0x400)/ 0x400 	       * 0x08) 
						 | ((md->VBS & 0x400)/ 0x400 	       * 0x04) 
						 | ((VDE     & 0x400)/ 0x400 	       * 0x02) 
						 | ((md->VT  & 0x400)/ 0x400           * 0x01)));
	 
	WGfx (ba, GCT_ID_SET_RESET,		0x00); 
	WGfx (ba, GCT_ID_ENABLE_SET_RESET,	0x00); 
	WGfx (ba, GCT_ID_COLOR_COMPARE,		0x00); 
	WGfx (ba, GCT_ID_DATA_ROTATE,		0x00); 
	WGfx (ba, GCT_ID_READ_MAP_SELECT,	0x00); 
	WGfx (ba, GCT_ID_GRAPHICS_MODE,		0x00);
	if (md->DEP == 4)
		WGfx (ba, GCT_ID_MISC,			0x04);
	else
		WGfx (ba, GCT_ID_MISC,			0x05);
	WGfx (ba, GCT_ID_COLOR_XCARE,		0xff); 
	WGfx (ba, GCT_ID_BITMASK,		0xff); 
	
	/* reset the Attribute Controller flipflop */
	vgar (ba, GREG_STATUS1_R);
	WAttr (ba, ACT_ID_PALETTE0,		0x00);     
	WAttr (ba, ACT_ID_PALETTE1,		0x01);  
	WAttr (ba, ACT_ID_PALETTE2,		0x02);  
	WAttr (ba, ACT_ID_PALETTE3,		0x03);  
	WAttr (ba, ACT_ID_PALETTE4,		0x04);  
	WAttr (ba, ACT_ID_PALETTE5,		0x05);  
	WAttr (ba, ACT_ID_PALETTE6,		0x06);  
	WAttr (ba, ACT_ID_PALETTE7,		0x07);  
	WAttr (ba, ACT_ID_PALETTE8,		0x08);  
	WAttr (ba, ACT_ID_PALETTE9,		0x09);  
	WAttr (ba, ACT_ID_PALETTE10,		0x0a);  
	WAttr (ba, ACT_ID_PALETTE11,		0x0b);  
	WAttr (ba, ACT_ID_PALETTE12,		0x0c);  
	WAttr (ba, ACT_ID_PALETTE13,		0x0d);  
	WAttr (ba, ACT_ID_PALETTE14,		0x0e);  
	WAttr (ba, ACT_ID_PALETTE15,		0x0f);  
	
	vgar (ba, GREG_STATUS1_R);
	if (md->DEP == 4)
		WAttr (ba, ACT_ID_ATTR_MODE_CNTL,	0x08);  
	else
		WAttr (ba, ACT_ID_ATTR_MODE_CNTL,	0x09);
	
	WAttr (ba, ACT_ID_OVERSCAN_COLOR,	0x00);  
	WAttr (ba, ACT_ID_COLOR_PLANE_ENA,	0x0f);  
	WAttr (ba, ACT_ID_HOR_PEL_PANNING,	0x00);  
	WAttr (ba, ACT_ID_COLOR_SELECT,	0x00);  
	
	vgar (ba, GREG_STATUS1_R);
		/* I have *NO* idea what strobing reg-0x20 might do... */
	vgaw (ba, ACT_ADDRESS_W, 0x20); 
	
	if (md->DEP == 4)
		WCrt (ba, CRT_ID_MAX_SCAN_LINE,	( ((md->FLG & MDF_DBL)/ MDF_DBL * 0x80) 
						|	                          0x40 
						| ((md->VBS & 0x200)/0x200	* 0x20) 
						| ((md->FY-1) 			& 0x1f)));
	else
		WCrt (ba, CRT_ID_MAX_SCAN_LINE,	( ((md->FLG & MDF_DBL)/ MDF_DBL * 0x80) 
						|	                          0x40 
						| ((md->VBS & 0x200)/0x200	* 0x20) 
						| (0	 			& 0x1f)));


	/* not it's time for guessing... */

	vgaw (ba, VDAC_REG_D, 	   0x02); 
	
		/* if this does what I think it does, it selects DAC 
		   register 0, and writes the palette in subsequent
		   registers, thus it works similar to the WD33C93 
		   select/data mechanism */
	vgaw (ba, VDAC_REG_SELECT, 0x00);
	
	{ 
		
		short x = 15;
		const unsigned char * col = md->PAL;
		do {
			
			vgaw (ba, VDAC_REG_DATA, *col++);
			vgaw (ba, VDAC_REG_DATA, *col++);
			vgaw (ba, VDAC_REG_DATA, *col++);
			
			
		} while (x--);

		if (md->DEP != 4) {
			short x = 256-17;
			unsigned char col = 16;
			do {
				
				vgaw(ba, VDAC_REG_DATA, col);
				vgaw(ba, VDAC_REG_DATA, col);
				vgaw(ba, VDAC_REG_DATA, col);
				col++;
				
			} while (x--);
		}
	}


	/* now load the font into maps 2 (and 3 for fonts wider than 8 pixels) */
	if (md->DEP == 4) {
		
		/* first set the whole font memory to a test-pattern, so we 
		   can see if something that shouldn't be drawn IS drawn.. */
		{
			volatile unsigned char * c = fb;
			long x;
			Map(2);
			
			for (x = 0; x < 65536; x++) {
				*c++ = (x & 1)? 0xaa : 0x55;
			}
		}
		
		{
			volatile unsigned char * c = fb;
			long x;
			Map(3);
			
			for (x = 0; x < 65536; x++) {
				*c++ = (x & 1)? 0xaa : 0x55;
			}
		}
		
		{
		  /* ok, now position at first defined character, and
		     copy over the images */
		  volatile unsigned char * c = fb + md->FLo * 32;
		  const unsigned char * f = md->FData;
		  unsigned short z;
		
		  Map(2);
		  for (z = md->FLo; z <= md->FHi; z++) {
			
			short y = md->FY-1;
			if (md->FX > 8){
				do {
					*c++ = *f;
					f += 2;
				} while (y--);
			}
			else {
				do {
					*c++ = *f++;
				} while (y--);
			}
			
			c += 32-md->FY;
			
		  }
		
		  if (md->FX > 8) {
			unsigned short z;
		
			Map(3);
			c = fb + md->FLo*32;
			f = md->FData+1;
			for (z = md->FLo; z <= md->FHi; z++) {
				
				short y = md->FY-1;
				do {
					*c++ = *f;
					f += 2;
				} while (y--);
				
				c += 32-md->FY;
				
			}
		  }
		}
		
	}
	
		/* select map 0 */
	WGfx (ba, GCT_ID_READ_MAP_SELECT,	0);
	if (md->DEP == 4)
			/* allow writes into maps 0 and 1 */
		WSeq (ba, SEQ_ID_MAP_MASK,		3);
	else
			/* allow writes into all maps */
		WSeq (ba, SEQ_ID_MAP_MASK,		0x0f);

		/* select extended chain4 addressing:
		    !A0/!A1	map 0	character to be displayed
		    !A1/ A1	map 1	attribute of that character
		     A0/!A1	map 2	not used (masked out, ignored)
		     A0/ A1 	map 3	not used (masked out, ignored) */
	WSeq (ba, SEQ_ID_EXT_VIDEO_ADDR,	RSeq(ba, SEQ_ID_EXT_VIDEO_ADDR) | 0x02);
	
	if (md->DEP == 4) {
		/* position in display memory */
		unsigned short * c = (unsigned short *) fb;
		
		/* fill with blank, white on black */
		const unsigned short fill_val = 0x2010;
		short x = md->XY;
		do {
			*c = fill_val;
			c += 2; } while (x--);
		
		/* I won't comment this :-)) */
		c = (unsigned short *) fb;
		c += (md->TX-6)*2;
		{
		  unsigned short init_msg[6] = {0x520a, 0x450b, 0x540c, 0x490d, 0x4e0e, 0x410f};
		  unsigned short * f = init_msg;
		  x = 5;
		  do {
			*c = *f++;
			c += 2;
	 	  } while (x--);
	 	}
	}
	else if (md->DEP == 8) {
		/* could clear the gfx screen here, but that's what the X server does anyway */
	        ;
	}

	gp->g_data	= (caddr_t)md;
	gi->gd_regaddr  = (caddr_t)ztwopa(ba);
	gi->gd_regsize  = 64*1024;

	gi->gd_fbaddr   = (caddr_t)ztwopa(fb);
#ifdef BANKEDDEVPAGER
	gi->gd_fbsize	= 4*1024*1024;  /* XXX */
	gi->gd_bank_size = 64*1024;
#else
	gi->gd_fbsize   = 64*1024;	/* larger, but that's whats mappable */
#endif
  
	gi->gd_colors   = 1 << md->DEP;
	gi->gd_planes   = md->DEP;
  
	gi->gd_fbwidth  = md->MW;
	gi->gd_fbheight = md->MH;
	gi->gd_fbx	= 0;
	gi->gd_fby	= 0;
	gi->gd_dwidth   = md->TX * md->FX;
	gi->gd_dheight  = md->TY * md->FY;
	gi->gd_dx	= 0;
	gi->gd_dy	= 0;
  
	/* initialized, works, return 1 */
	return(1);
}

int rt_mode __P((struct grf_softc *, int, void *, int , int));

void grfrtattach __P((struct device *, struct device *, void *));
int grfrtprint __P((void *, char *));
int grfrtmatch __P((struct device *, struct cfdata *, void *));
 
struct cfdriver grfrtcd = {
	NULL, "grfrt", (cfmatch_t)grfrtmatch, grfrtattach, 
	DV_DULL, sizeof(struct grf_softc), NULL, 0 };

/*
 * only used in console init
 */
static struct cfdata *cfdata;

/*
 * we make sure to only init things once.  this is somewhat
 * tricky regarding the console.
 */
int 
grfrtmatch(pdp, cfp, auxp)
	struct device *pdp;
	struct cfdata *cfp;
	void *auxp;
{
#ifdef RETINACONSOLE
	static int rtconunit = -1;
#endif
	struct zbus_args *zap;

	zap = auxp;

	/*
	 * allow only one retina console
	 */
	if (amiga_realconfig == 0)
#ifdef RETINACONSOLE
		if (rtconunit != -1)
#endif
			return(0);
	/*
	 * check that this is a retina board.
	 */
	if (zap->manid != 18260 || zap->prodid != 6)
		return(0);

#ifdef RETINACONSOLE
	if (amiga_realconfig == 0 || rtconunit != cfp->cf_unit) {
#endif
		if ((unsigned)retina_default_mon >= retina_mon_max ||
		    monitor_defs[retina_default_mon].DEP == 8)
			retina_default_mon = 0;

		current_mon = monitor_defs + retina_default_mon;
		if (retina_alive(current_mon) == 0)
			return(0);
#ifdef RETINACONSOLE
		if (amiga_realconfig == 0) {
			rtconunit = cfp->cf_unit;
			cfdata = cfp;
		}
	}
#endif
	return(1);
}

/* 
 * attach to the grfbus (zbus)
 */
void
grfrtattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	static struct grf_softc congrf;
	static int coninited;
	struct zbus_args *zap;
	struct grf_softc *gp;

	zap = auxp;
	
	if (dp == NULL) 
		gp = &congrf;
	else
		gp = (struct grf_softc *)dp;

	if (dp != NULL && congrf.g_regkva != 0) {
		/*
		 * we inited earlier just copy the info
		 * take care not to copy the device struct though.
		 */
		bcopy(&congrf.g_display, &gp->g_display, 
		    (char *)&gp[1] - (char *)&gp->g_display);
	} else {
		gp->g_regkva = (volatile caddr_t)zap->va;
		gp->g_fbkva = (volatile caddr_t)zap->va + 64 * 1024;
		gp->g_unit = GRF_RETINAII_UNIT;
		gp->g_flags = GF_ALIVE;
		gp->g_mode = rt_mode;
		gp->g_conpri = grfrt_cnprobe();
		grfrt_iteinit(gp);
		(void)rt_load_mon(gp, current_mon);
	}
	if (dp != NULL)
		printf("\n");
	/*
	 * attach grf
	 */
	amiga_config_found(cfdata, &gp->g_device, gp, grfrtprint);
}

int
grfrtprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp)
		printf("grf%d at %s", ((struct grf_softc *)auxp)->g_unit,
			pnp);
	return(UNCONF);
}

static int 
rt_getvmode (gp, vm)
     struct grf_softc *gp;
     struct grfvideo_mode *vm;
{
  struct MonDef *md;

  if (vm->mode_num && vm->mode_num > retina_mon_max)
    return EINVAL;

  if (! vm->mode_num)
    vm->mode_num = (current_mon - monitor_defs) + 1;

  md = monitor_defs + (vm->mode_num - 1);
  strncpy (vm->mode_descr, monitor_descr + (vm->mode_num - 1), 
	   sizeof (vm->mode_descr));
  vm->pixel_clock  = md->FQ;
  vm->disp_width   = md->MW;
  vm->disp_height  = md->MH;
  vm->depth        = md->DEP;

  /*
   * From observation of the monitor definition table above, I guess that
   * the horizontal timings are in units of longwords. Hence, I get the
   * pixels by multiplication with 32 and division by the depth.
   * The text modes, apparently marked by depth == 4, are even more wierd.
   * According to a comment above, they are computed from a depth==8 mode
   * (thats for us: * 32 / 8) by applying another factor of 4 / font width.
   * Reverse applying the latter formula most of the constants cancel
   * themselves and we are left with a nice (* font width).
   * That is, internal timings are in units of longwords for graphics  
   * modes, or in units of characters widths for text modes.
   * We better don't WRITE modes until this has been real live checked.
   * 			- Ignatios Souvatzis
   */

  if (md->DEP == 4) {
	vm->hblank_start = md->HBS * 32 / md->DEP;
	vm->hblank_stop  = md->HBE * 32 / md->DEP;
	vm->hsync_start  = md->HSS * 32 / md->DEP;
	vm->hsync_stop   = md->HSE * 32 / md->DEP;
	vm->htotal       = md->HT * 32 / md->DEP;
  } else {
	vm->hblank_start = md->HBS * md->FX;
	vm->hblank_stop  = md->HBE * md->FX;
	vm->hsync_start  = md->HSS * md->FX;
	vm->hsync_stop   = md->HSE * md->FX;
	vm->htotal       = md->HT * md->FX;
  }
  vm->vblank_start = md->VBS;
  vm->vblank_stop  = md->VBE;
  vm->vsync_start  = md->VSS;
  vm->vsync_stop   = md->VSE;
  vm->vtotal       = md->VT;

  return 0;
}


static int 
rt_setvmode (gp, mode, txtonly)
     struct grf_softc *gp;
     unsigned mode;
     int txtonly;
{
  struct MonDef *md;
  int error;

  if (!mode || mode > retina_mon_max)
    return EINVAL;

  if (txtonly && monitor_defs[mode-1].DEP == 8)
    return EINVAL;

  current_mon = monitor_defs + (mode - 1);

  error = rt_load_mon (gp, current_mon) ? 0 : EINVAL;

  return error;
}


/*
 * Change the mode of the display.
 * Return a UNIX error number or 0 for success.
 */
int
rt_mode(gp, cmd, arg, a2, a3)
	struct grf_softc *gp;
	int cmd;
	void *arg;
	int a2, a3;
{
  /* implement these later... */

  switch (cmd)
    {
    case GM_GRFON:
      rt_setvmode (gp, retina_default_gfx + 1, 0);
      return 0;
      
    case GM_GRFOFF:
      rt_setvmode (gp, retina_default_mon + 1, 0);
      return 0;
      
    case GM_GRFCONFIG:
      return 0;

    case GM_GRFGETVMODE:
      return rt_getvmode (gp, (struct grfvideo_mode *) arg);

    case GM_GRFSETVMODE:
      return rt_setvmode (gp, *(unsigned *) arg, 1);

    case GM_GRFGETNUMVM:
      *(int *)arg = retina_mon_max;
      return 0;

#ifdef BANKEDDEVPAGER
    case GM_GRFGETBANK:
      *(int *)arg = rt_getbank (gp, a2, a3);
      return 0;

    case GM_GRFGETCURBANK:
      *(int *)arg = rt_getcurbank (gp);
      return 0;

    case GM_GRFSETBANK:
      return rt_setbank (gp, arg);
#endif
    case GM_GRFIOCTL:
      return rt_ioctl (gp, (u_long)arg, (void *)a2);

    default:
      break;
    }
    
  return EINVAL;
}

int
rt_ioctl (gp, cmd, data)
	register struct grf_softc *gp;
	u_long cmd;
	void *data;
{
  switch (cmd)
    {
    case GRFIOCGSPRITEPOS:
      return rt_getspritepos (gp, (struct grf_position *) data);

    case GRFIOCSSPRITEPOS:
      return rt_setspritepos (gp, (struct grf_position *) data);

    case GRFIOCSSPRITEINF:
      return rt_setspriteinfo (gp, (struct grf_spriteinfo *) data);

    case GRFIOCGSPRITEINF:
      return rt_getspriteinfo (gp, (struct grf_spriteinfo *) data);

    case GRFIOCGSPRITEMAX:
      return rt_getspritemax (gp, (struct grf_position *) data);

    case GRFIOCGETCMAP:
      return rt_getcmap (gp, (struct grf_colormap *) data);

    case GRFIOCPUTCMAP:
      return rt_putcmap (gp, (struct grf_colormap *) data);

    case GRFIOCBITBLT:
      return rt_bitblt (gp, (struct grf_bitblt *) data);
    }

  return EINVAL;
}     

#ifdef BANKEDDEVPAGER

/* Retina banks can overlap. Don't use this information (yet?), and
   only switch 64k sized banks. */

int
rt_getbank (gp, offs, prot)
     struct grf_softc *gp;
     off_t offs;
     int prot;
{
  /* XXX */
  if (offs <  0 || offs >= 4*1024*1024)
    return -1;
  else
    return offs >> 16;
}

int
rt_getcurbank (gp)
     struct grf_softc *gp;
{
  struct grfinfo *gi = &gp->g_display;
  volatile unsigned char *ba;
  int bank;

  ba = gp->g_regkva;
  bank = RSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO) | (RSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI) << 8);

  /* bank register is multiple of 64 byte, make this multiple of 64k */
  bank >>= 10;
  return bank;
}

int
rt_setbank (gp, bank)
     struct grf_softc *gp;
     int bank;
{
  volatile unsigned char *ba;

  ba = gp->g_regkva;
  /* bank register is multiple of 64 byte, make this multiple of 64k */
  bank <<= 10;
  WSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO, (unsigned char) bank);
  bank >>= 8;
  WSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI, (unsigned char) bank);

  return 0;
}

#endif

int
rt_getcmap (gfp, cmap)
     struct grf_softc *gfp;
     struct grf_colormap *cmap;
{
  volatile unsigned char *ba;
  u_char red[256], green[256], blue[256], *rp, *gp, *bp;
  short x;
  int error;

  if (cmap->count == 0 || cmap->index >= 256)
    return 0;

  if (cmap->index + cmap->count > 256)
    cmap->count = 256 - cmap->index;

  ba = gfp->g_regkva;
  /* first read colors out of the chip, then copyout to userspace */
  vgaw (ba, VDAC_REG_SELECT, cmap->index);
  x = cmap->count - 1;
  rp = red + cmap->index; 
  gp = green + cmap->index; 
  bp = blue + cmap->index;
  do
    {
      *rp++ = vgar (ba, VDAC_REG_DATA);
      *gp++ = vgar (ba, VDAC_REG_DATA);
      *bp++ = vgar (ba, VDAC_REG_DATA);
    }
  while (x--);

  if (!(error = copyout (red + cmap->index, cmap->red, cmap->count))
      && !(error = copyout (green + cmap->index, cmap->green, cmap->count))
      && !(error = copyout (blue + cmap->index, cmap->blue, cmap->count)))
    return 0;

  return error;
}

int
rt_putcmap (gfp, cmap)
     struct grf_softc *gfp;
     struct grf_colormap *cmap;
{
  volatile unsigned char *ba;
  u_char red[256], green[256], blue[256], *rp, *gp, *bp;
  short x;
  int error;

  if (cmap->count == 0 || cmap->index >= 256)
    return 0;

  if (cmap->index + cmap->count > 256)
    cmap->count = 256 - cmap->index;

  /* first copy the colors into kernelspace */
  if (!(error = copyin (cmap->red, red + cmap->index, cmap->count))
      && !(error = copyin (cmap->green, green + cmap->index, cmap->count))
      && !(error = copyin (cmap->blue, blue + cmap->index, cmap->count)))
    {
      ba = gfp->g_regkva;
      vgaw (ba, VDAC_REG_SELECT, cmap->index);
      x = cmap->count - 1;
      rp = red + cmap->index; 
      gp = green + cmap->index; 
      bp = blue + cmap->index;
      do
	{
	  vgaw (ba, VDAC_REG_DATA, *rp++);
	  vgaw (ba, VDAC_REG_DATA, *gp++);
	  vgaw (ba, VDAC_REG_DATA, *bp++);
	}
      while (x--);
      return 0;
    }
  else
    return error;
}

int
rt_getspritepos (gp, pos)
     struct grf_softc *gp;
     struct grf_position *pos;
{
  volatile unsigned char *ba;

  ba = gp->g_regkva;
  pos->x = vgar (ba, SEQ_ID_CURSOR_X_LOC_LO) | (vgar (ba, SEQ_ID_CURSOR_X_LOC_HI) << 8);
  pos->y = vgar (ba, SEQ_ID_CURSOR_Y_LOC_LO) | (vgar (ba, SEQ_ID_CURSOR_Y_LOC_HI) << 8);
  return 0;
}

int
rt_setspritepos (gp, pos)
     struct grf_softc *gp;
     struct grf_position *pos;
{
  volatile unsigned char *ba;

  ba = gp->g_regkva;
  vgaw (ba, SEQ_ID_CURSOR_X_LOC_LO, pos->x & 0xff);
  vgaw (ba, SEQ_ID_CURSOR_X_LOC_HI, (pos->x >> 8) & 0x07);
  vgaw (ba, SEQ_ID_CURSOR_Y_LOC_LO, pos->y & 0xff);
  vgaw (ba, SEQ_ID_CURSOR_Y_LOC_HI, (pos->y >> 8) & 0x07);
  return 0;
}

/* assume an at least 2M retina (XXX), sprite is last in memory.
   According to the bogus docs, the cursor can be at most 128 lines
   in height, and the x-hostspot can be placed at most at pos 31,
   this gives width of a long */
#define SPRITE_ADDR (2*1024*1024 - 128*4)

int
rt_getspriteinfo (gp, info)
     struct grf_softc *gp;
     struct grf_spriteinfo *info;
{
  volatile unsigned char *ba, *fb;

  ba = gp->g_regkva;
  fb = gp->g_fbkva;
  if (info->set & GRFSPRSET_ENABLE)
    info->enable = vgar (ba, SEQ_ID_CURSOR_CONTROL) & 0x01;
  if (info->set & GRFSPRSET_POS)
    rt_getspritepos (gp, &info->pos);
  if (info->set & GRFSPRSET_HOT)
    {
      info->hot.x = vgar (ba, SEQ_ID_CURSOR_X_INDEX) & 0x1f;
      info->hot.y = vgar (ba, SEQ_ID_CURSOR_Y_INDEX) & 0x7f;
    }
  if (info->set & GRFSPRSET_CMAP)
    {
      struct grf_colormap cmap;
      int index;
      cmap.index = 0;
      cmap.count = 256;
      rt_getcmap (gp, &cmap);
      index = vgar (ba, SEQ_ID_CURSOR_COLOR0);
      info->cmap.red[0] = cmap.red[index];
      info->cmap.green[0] = cmap.green[index];
      info->cmap.blue[0] = cmap.blue[index];
      index = vgar (ba, SEQ_ID_CURSOR_COLOR1);
      info->cmap.red[1] = cmap.red[index];
      info->cmap.green[1] = cmap.green[index];
      info->cmap.blue[1] = cmap.blue[index];
    }
  if (info->set & GRFSPRSET_SHAPE)
    {
      int saved_bank_lo = RSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO);
      int saved_bank_hi = RSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI);
      int last_bank = SPRITE_ADDR >> 6;
      int last_bank_lo = last_bank & 0xff;
      int last_bank_hi = last_bank >> 8;
      u_char mask;
      WSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO, last_bank_lo);
      WSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI, last_bank_hi);
      copyout (fb, info->image, 128*4);
      mask = RSeq (ba, SEQ_ID_CURSOR_PIXELMASK);
      WSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO, saved_bank_lo);
      WSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI, saved_bank_hi);
      copyout (&mask, info->mask, 1);
      info->size.x = 32; /* ??? */
      info->size.y = (RSeq (ba, SEQ_ID_CURSOR_CONTROL) & 6) << 4;
    }

}

int
rt_setspriteinfo (gp, info)
     struct grf_softc *gp;
     struct grf_spriteinfo *info;
{
  volatile unsigned char *ba, *fb;
  u_char control;

  ba = gp->g_regkva;
  fb = gp->g_fbkva;
  control = vgar (ba, SEQ_ID_CURSOR_CONTROL);
  if (info->set & GRFSPRSET_ENABLE)
    {
      if (info->enable)
	control |= 1;
      else
	control &= ~1;
      vgaw (ba, SEQ_ID_CURSOR_CONTROL, control);
    }
  if (info->set & GRFSPRSET_POS)
    rt_setspritepos (gp, &info->pos);
  if (info->set & GRFSPRSET_HOT)
    {
      vgaw (ba, SEQ_ID_CURSOR_X_INDEX, info->hot.x & 0x1f);
      vgaw (ba, SEQ_ID_CURSOR_Y_INDEX, info->hot.y & 0x7f);
    }
  if (info->set & GRFSPRSET_CMAP)
    {
      /* hey cheat a bit here.. XXX */
      vgaw (ba, SEQ_ID_CURSOR_COLOR0, 0);
      vgaw (ba, SEQ_ID_CURSOR_COLOR1, 1);
    }
  if (info->set & GRFSPRSET_SHAPE)
    {
      int saved_bank_lo = RSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO);
      int saved_bank_hi = RSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI);
      int last_bank = SPRITE_ADDR >> 6;
      int last_bank_lo = last_bank & 0xff;
      int last_bank_hi = last_bank >> 8;
      u_char mask;
      WSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO, last_bank_lo);
      WSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI, last_bank_hi);
      copyin (info->image, fb, 128*4);
      WSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO, saved_bank_lo);
      WSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI, saved_bank_hi);
      copyin (info->mask, &mask, 1);
      WSeq (ba, SEQ_ID_CURSOR_PIXELMASK, mask);
      /* info->size.x = 32; *//* ??? */

      info->size.y = (RSeq (ba, SEQ_ID_CURSOR_CONTROL) & 6) << 4;
      control = (control & ~6) | ((info->size.y >> 4) & 6);
      vgaw (ba, SEQ_ID_CURSOR_CONTROL, control);

      /* sick intel bull-addressing.. */
      WSeq (ba, SEQ_ID_CURSOR_STORE_LO, SPRITE_ADDR & 0x0f);
      WSeq (ba, SEQ_ID_CURSOR_STORE_HI, 0);
      WSeq (ba, SEQ_ID_CURSOR_ST_OFF_LO, (SPRITE_ADDR >> 4) & 0xff);
      WSeq (ba, SEQ_ID_CURSOR_ST_OFF_HI, ((SPRITE_ADDR >> 4) >> 8) & 0xff);
    }
  
  return 0;
}

int
rt_getspritemax (gp, pos)
     struct grf_softc *gp;
     struct grf_position *pos;
{
  pos->x = 32;
  pos->y = 128;

  return 0;
}


/*
 * !!! THIS AREA UNDER CONSTRUCTION !!!
 */

int
rt_bitblt (gp, bb)
     struct grf_softc *gp;
     struct grf_bitblt *bb;
{
  return EINVAL;


#if 0
  volatile unsigned char *ba, *fb;
  u_char control;
  u_char saved_bank_lo;
  u_char saved_bank_hi;
  u_char src_bank_lo, src_bank_hi;
  u_char dst_bank_lo, dst_bank_hi;
  u_long src_offset, dst_offset;
  u_short src_bank, dst_bank;
  u_char *srcp, *dstp;
  short x, y;
  u_long tot;

  ba = gp->g_regkva;
  fb = gp->g_fbkva;

  saved_bank_lo = RSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO);
  saved_bank_hi = RSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI);

  /* for now, only GRFBBcopy is supported, and only for depth 8. No
     clipping is performed, either... */

  if (bb->op != GRFBBcopy && gp->g_display.gd_planes != 8)
    return EINVAL;

  src_offset = op->src_x + op->src_y * gp->g_display.gd_fbwidth;
  dst_offset = op->dst_x + op->dst_y * gp->g_display.gd_fbwidth;
  tot = op->w * op->h;

  /* set write mode 1, "[...] data in the read latches is written
     to memory during CPU memory write cycles. [...]" */
  WGfx (ba, GCT_ID_GRAPHICS_MODE, (RGfx(ba, GCT_ID_GRAPHICS_MODE) & 0xfc) | 1); 
  /* write to primary, read from secondary */
  WSeq (ba, SEQ_ID_EXTENDED_MEM_ENA, (RSeq(ba, SEQ_ID_EXTENDED_MEM_ENA) & 0x1f) | 0 ); 

  if (src_offset < dst_offset)
    {
      /* start at end */
      src_offset += tot;
      dst_offset += tot;
    }

  src_bank_lo = (src_offset >> 6) & 0xff;
  src_bank_hi = (src_offset >> 14) & 0xff;
  dst_bank_lo = (dst_offset >> 6) & 0xff;
  dst_bank_hi = (dst_offset >> 14) & 0xff;

  while (tot)
    {
      WSeq (ba, SEQ_ID_SEC_HOST_OFF_LO, src_bank_lo);
      WSeq (ba, SEQ_ID_SEC_HOST_OFF_HI, src_bank_hi);
      WSeq (ba, SEQ_ID_PRIM_HOST_OFF_LO, dst_bank_lo);
      WSeq (ba, SEQ_ID_PRIM_HOST_OFF_HI, dst_bank_hi);
      
      if (src_offset < dst_offset)
	{
	  
	  
	}
      else
	{
	  
	}
    }
  

#endif
}


#endif	/* NGRF */
