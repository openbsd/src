/* $NetBSD: vidcconsole.c,v 1.7 1996/03/28 21:18:40 mark Exp $ */

/*
 * Copyright (c) 1996 Robert Black
 * Copyright (c) 1994-1995 Melvyn Tang-Richardson
 * Copyright (c) 1994-1995 RiscBSD kernel team
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
 *	This product includes software developed by the RiscBSD kernel team
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE RISCBSD TEAM ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * vidcconsole.c
 *
 * Console assembly functions
 *
 * Created      : 17/09/94
 * Last updated : 07/02/96
 */

/* woo */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/device.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/syslog.h>
#include <sys/resourcevar.h>
#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/param.h>
#include <machine/katelib.h>
#include <machine/cpu.h>
#include <machine/bootconfig.h>
#include <machine/iomd.h>
#include <machine/irqhandler.h>
#include <machine/pmap.h>
#include <machine/vidc.h>
#include <machine/vconsole.h>

#include <arm32/dev/console/fonts/font_normal.h>
#include <arm32/dev/console/fonts/font_bold.h>
#include <arm32/dev/console/fonts/font_italic.h>


#define BCOPY bcopy

#ifndef DEBUGTERM
#define dprintf(x)	;
#endif

/* Options ************************************/
#define ACTIVITY_WARNING
#define SCREENBLANKER
#undef INVERSE_CONSOLE
/**********************************************/

/* Internal defines only **********************/
#define DEVLOPING
#undef SELFTEST
#undef SILLIES
/**********************************************/

#ifdef SILLIES
#define PRETTYCURSOR
#endif

extern int physcon_major;
extern struct vconsole *vconsole_default;

extern videomemory_t videomemory;

extern font_struct font_terminal_14normal;
extern font_struct font_terminal_14bold;
extern font_struct font_terminal_14italic;

#define font_normal	font_terminal_14normal
#define font_bold	font_terminal_14bold
#define font_italic	font_terminal_14italic

#define VIDC_ENGINE_NAME "VIDC"
#define R_DATA ((struct vidc_info *)vc->r_data)
#define MODE (R_DATA->mode)

static int cold_init = 0;

extern struct vconsole *vconsole_master;
extern struct vconsole *vconsole_current;
static struct vidc_mode vidc_initialmode;
static struct vidc_mode *vidc_currentmode;

unsigned int dispstart;
unsigned int dispsize;
unsigned int dispbase;
unsigned int dispend;
unsigned int ptov;
unsigned int transfersize;
unsigned int vmem_base;
unsigned int phys_base;
int	flash;
int	cursor_flash;
char *cursor_normal;
char *cursor_transparent;
int p_cursor_normal;
int p_cursor_transparent;

/* Local function prototypes */
static void	vidcconsole_cls		__P(( struct vconsole */*vc*/ ));
static int	vidc_cursor_init	__P(( struct vconsole */*vc*/ ));
static int	vidcconsole_cursorintr	__P(( struct vconsole */*vc*/ ));
int		vidcconsole_flashintr	__P(( struct vconsole */*vc*/ ));
static int	vidcconsole_textpalette	__P(( struct vconsole */*vc*/ ));
static void	vidcconsole_render	__P(( struct vconsole */*vc*/, char /*c*/ ));
static void	vidcconsole_mode	__P(( struct vconsole */*vc*/, struct vidc_mode */*mode*/ ));
int		vidcconsole_flash	__P(( struct vconsole */*vc*/, int /*flash*/ ));
int		vidcconsole_cursorflash	__P(( struct vconsole */*vc*/, int /*flash*/ ));
int		vidcconsole_flash_go	__P(( struct vconsole */*vc*/ ));
int		vidcconsole_blank	__P(( struct vconsole */*vc*/, int /*type*/ ));
void 		vidcconsole_putchar	__P(( dev_t dev, char c, struct vconsole *vc));
extern int	vidcconsolemc_cls	__P(( unsigned char *, unsigned char *, int ));

void		(*line_cpfunc) 		__P(( char *, char * ));

/*
 * This will be called while still in the mode that we were left
 * in after exiting from riscos
 */

static irqhandler_t cursor_ih;
irqhandler_t flash_ih;


#ifndef HARDCODEDMODES
/* The table of modes is separately compiled */
extern struct vidc_mode vidcmodes[];
#else /* HARDCODEDMODES */
#ifdef RC7500
static struct vidc_mode vidcmodes[] = {
	{31500,/**/48,  84, 30, 640,  30, 0,/**/3, 28, 0, 480, 0, 9,/**/0,/**/3},
	{36000,/**/72,  84, 34, 800,  34, 0,/**/2, 22, 0, 600, 0, 1,/**/0,/**/3},
};
#else /* RC7500 */
/* We have a hard compiled table of modes and a list of definable modes */
static struct vidc_mode vidcmodes[] = {
	{32500,/**/52,  64, 30, 640,  30, 14,/**/3, 28, 0, 480, 0, 9,/**/0,/**/3},
	{65000,/**/128, 36, 60, 1024, 60 ,36,/**/6, 29, 0, 768, 0, 3,/**/0,/**/3},
};
#endif /* RC7500 */
#endif /* HARDCODEDMODES */

#ifdef RC7500
struct vfreq {
	u_int frqcon;
	int freq;
};

static struct vfreq vfreq[] = {
	{ VIDFREQ_25_18, 25175},
	{ VIDFREQ_25_18, 25180},
	{ VIDFREQ_28_32, 28320},
	{ VIDFREQ_31_50, 31500},
	{ VIDFREQ_36_00, 36000},
	{ VIDFREQ_40_00, 40000},
	{ VIDFREQ_44_90, 44900},
	{ VIDFREQ_50_00, 50000},
	{ VIDFREQ_65_00, 65000},
	{ VIDFREQ_72_00, 72000},
	{ VIDFREQ_75_00, 75000},
	{ VIDFREQ_77_00, 77000},
	{ VIDFREQ_80_00, 80000},
	{ VIDFREQ_94_50, 94500},
	{ VIDFREQ_110_0, 110000},
	{ VIDFREQ_120_0, 120000},
	{ VIDFREQ_130_0, 130000}
};

#define NFREQ	(sizeof (vfreq) / sizeof(struct vfreq))
u_int vfreqcon = 0;
#else /* RC7500 */

struct fsyn {
	int r, v, f;
};

static struct fsyn fsyn_pref[] = {
	{ 6,   2,  8000 },
	{ 4,   2, 12000 },
	{ 3,   2, 16000 },
	{ 2,   2, 24000 },
	{ 41, 43, 25171 },
	{ 50, 59, 28320 },
	{ 3,   4, 32000 },
	{ 2,   3, 36000 },
	{ 31, 58, 44903 },
	{ 12, 35, 70000 },
	{ 0,   0, 00000 }
};
#endif /* RC7500 */

static inline int
mod ( int n )
{
	if (n<0) return (-n);
	else     return n;
}

static int
vidcconsole_coldinit(vc)
	struct vconsole *vc;
{
	int found;
	int loop;

	line_cpfunc = NULL;

	/* Do this first so it dont look messy */

	vidc_write ( VIDC_CP1, 0x0 );
	vidc_write ( VIDC_CP2, 0x0 );
	vidc_write ( VIDC_CP3, 0x0 );

    /* Try to determine the current mode */
	vidc_initialmode.hder = bootconfig.width+1;
	vidc_initialmode.vder = bootconfig.height+1;
/*
	vidc_initialmode.hder = 1024;
	vidc_initialmode.vder = 768;
*/
	vidc_initialmode.bitsperpixel = 1 << bootconfig.bitsperpixel;

/* Nut - should be using videomemory.vidm_vbase - mark */

	dispbase = vmem_base = dispstart = bootconfig.display_start;
	phys_base = videomemory.vidm_pbase;

/* Nut - should be using videomemory.vidm_size - mark */

#ifdef RC7500
	dispsize = videomemory.vidm_size;
	transfersize = 16;
#else /* RC7500 */
	dispsize = bootconfig.vram[0].pages * NBPG;
	transfersize = dispsize >> 10;
#endif /* RC7500 */
    
	ptov = dispbase - phys_base;

	dispend = dispstart+dispsize;
 
	vidc_currentmode = &vidcmodes[0];
	loop = 0;
	found = 0;
  
	while (vidcmodes[loop].pixel_rate != 0) {
  		if (vidcmodes[loop].hder == (bootconfig.width + 1)
  		    && vidcmodes[loop].vder == (bootconfig.height + 1)
		    && vidcmodes[loop].frame_rate == bootconfig.framerate) {
			vidc_currentmode = &vidcmodes[loop];
			found = 1;
		}
		++loop;
	}

	if (!found) {
		vidc_currentmode = &vidcmodes[0];
		loop = 0;
		found = 0;

		while (vidcmodes[loop].pixel_rate != 0) {
			if (vidcmodes[loop].hder == (bootconfig.width + 1)
 			    && vidcmodes[loop].vder == (bootconfig.height + 1)) {
 				vidc_currentmode = &vidcmodes[loop];
 				found = 1;
 			}
 			++loop;
 		}
	}

	/* vidc_currentmode = &vidcmodes[0];*/
	vidc_currentmode->bitsperpixel = 1 << bootconfig.bitsperpixel;

	R_DATA->flash = R_DATA->cursor_flash = 0;

	dispstart = dispbase;
	dispend = dispstart+dispsize;
    
	WriteWord ( IOMD_VIDINIT, dispstart-ptov );
	WriteWord ( IOMD_VIDSTART, dispstart-ptov );
	WriteWord ( IOMD_VIDEND, (dispend-transfersize)-ptov );
	return 0;
}

struct vidc_mode newmode;

static const int bpp_mask_table[] = {
	0,  /* 1bpp */
	1,  /* 2bpp */
	2,  /* 4bpp */
	3,  /* 8bpp */
	4,  /* 16bpp */
	6   /* 32bpp */
};

void
vidcconsole_mode(vc, mode)
	struct vconsole *vc;
	struct vidc_mode *mode;
{    
	register int acc;
	int bpp_mask;
        int log_bpp;
        int tmp_bpp;

#ifndef RC7500
	int best_r, best_v, best_match;
#endif

/*
 * Find out what bit mask we need to or with the vidc20 control register
 * in order to generate the desired number of bits per pixel.
 * log_bpp is log base 2 of the number of bits per pixel.
 */

	tmp_bpp = mode->bitsperpixel;
	if (tmp_bpp < 1 || tmp_bpp > 32)
		tmp_bpp = 8; /* Set 8 bpp if we get asked for something silly */

	for (log_bpp = 0; tmp_bpp != 1; tmp_bpp >>= 1)
		log_bpp++;

	bpp_mask = bpp_mask_table[log_bpp];

/*
	printf ( "res = (%d, %d) rate = %d\n", mode->hder, mode->vder, mode->pixel_rate );
*/

	newmode = *mode;
	vidc_currentmode = &newmode;

#ifdef RC7500
	{
		int i;
		int old, new;
		u_int nfreq;

		old = vfreq[0].freq;
		nfreq = vfreq[0].frqcon;
		for (i = 0; i < (NFREQ - 1); i++) {
			new = vfreq[i].freq - mode->pixel_rate;
			if (new < 0)
				new = -new;
			if (new < old) {
				nfreq = vfreq[i].frqcon;
				old = new;
			}
			if (new == 0)
				break;
		}
		nfreq |= (vfreqcon & 0xf0);
		vfreqcon = nfreq;
	}
#else /* RC7500 */
    /* Program the VCO Look up a preferred value before choosing one */
	{
		int least_error = mod (fsyn_pref[0].f - vidc_currentmode->pixel_rate);
		int counter;
		best_r = fsyn_pref[0].r;
		best_match = fsyn_pref[0].f;
		best_v = fsyn_pref[0].v;
        
	/* Look up */
        
		counter=0;
        
		while ( fsyn_pref[counter].r != 0 ) {
			if (least_error > mod (fsyn_pref[counter].f - vidc_currentmode->pixel_rate)) {
				best_match = fsyn_pref[counter].f;
				least_error = mod (fsyn_pref[counter].f - vidc_currentmode->pixel_rate);
				best_r = fsyn_pref[counter].r;
				best_v = fsyn_pref[counter].v;
			}
			counter++;
		}

		if ( least_error > 0) { /* Accuracy of 1000Hz */
			int r, v, f;
			for ( v=63; v>0; v-- )
				for ( r=63; r>0; r-- ) {
					f = (v * VIDC_FREF/1000) / r;
					if (least_error >= mod (f - vidc_currentmode->pixel_rate)) {
						best_match = f;
						least_error = mod (f - vidc_currentmode->pixel_rate);
						best_r = r;
						best_v = v;
					}
				}
		}
    
		if ( best_r>63 ) best_r=63;
		if ( best_v>63 ) best_v=63;
		if ( best_r<1 )  best_r= 1;
		if ( best_v<1 )  best_v= 1;
    
	}
/*
	printf ( "best_v = %d  best_r = %d best_f = %d\n", best_v, best_r, best_match );
*/
#endif /* RC7500 */

	if (vc==vconsole_current) {
#ifdef RC7500
		outb(FREQCON, vfreqcon);
		/*
		 * Need to program the control register first.
		 */
		if ( dispsize>1024*1024 ) {
			if ( vidc_currentmode->hder>=800 )
				vidc_write ( VIDC_CONREG, 7<<8 | bpp_mask<<5);
			else
				vidc_write ( VIDC_CONREG, 6<<8 | bpp_mask<<5);
		} else {
			vidc_write ( VIDC_CONREG, 7<<8 | bpp_mask<<5);
		}

		/*
		 * We don't use VIDC_FSYNREG.  Program it low.
		 */
		vidc_write(VIDC_FSYNREG, 0x2020);
#else /* RC7500 */
		vidc_write ( VIDC_FSYNREG, (best_v-1)<<8 | (best_r-1)<<0 );
#endif /* RC7500 */
		acc=0;
		acc+=vidc_currentmode->hswr;vidc_write(VIDC_HSWR,(acc  - 8  )& (~1)	);
		acc+=vidc_currentmode->hbsr;vidc_write(VIDC_HBSR,(acc  - 12 )& (~1)	);
		acc+=vidc_currentmode->hdsr;vidc_write(VIDC_HDSR,(acc  - 18 )& (~1)	);
		acc+=vidc_currentmode->hder;vidc_write(VIDC_HDER,(acc  - 18 )& (~1)	);
		acc+=vidc_currentmode->hber;vidc_write(VIDC_HBER,(acc  - 12 )& (~1)	);
		acc+=vidc_currentmode->hcr ;vidc_write(VIDC_HCR ,(acc  - 8)&(~3));

		acc=0;
		acc+=vidc_currentmode->vswr;	vidc_write(VIDC_VSWR,(acc  - 1 ));
		acc+=vidc_currentmode->vbsr;	vidc_write(VIDC_VBSR,(acc  - 1 ));
		acc+=vidc_currentmode->vdsr;	vidc_write(VIDC_VDSR,(acc  - 1 ));
		acc+=vidc_currentmode->vder;	vidc_write(VIDC_VDER,(acc  - 1 ));
		acc+=vidc_currentmode->vber;	vidc_write(VIDC_VBER,(acc  - 1 ));
		acc+=vidc_currentmode->vcr;		vidc_write(VIDC_VCR ,(acc  - 1 ));

		WriteWord(IOMD_FSIZE, vidc_currentmode->vcr
		    + vidc_currentmode->vswr
		    + vidc_currentmode->vber
		    + vidc_currentmode->vbsr - 1 );

		if ( dispsize==1024*1024 )
			vidc_write ( VIDC_DCTL, vidc_currentmode->hder>>2 | 1<<16 | 1<<12);
		else
			vidc_write ( VIDC_DCTL, vidc_currentmode->hder>>2 | 3<<16 | 1<<12);

		vidc_write ( VIDC_EREG, 1<<12 );
		if ( dispsize>1024*1024) {
			if ( vidc_currentmode->hder>=800 )
 				vidc_write ( VIDC_CONREG, 7<<8 | bpp_mask<<5);
			else
				vidc_write ( VIDC_CONREG, 6<<8 | bpp_mask<<5);
		} else {
			vidc_write ( VIDC_CONREG, 7<<8 | bpp_mask<<5);
		}
	}

	R_DATA->mode = *vidc_currentmode;
	R_DATA->screensize = R_DATA->XRES * R_DATA->YRES * R_DATA->BITSPERPIXEL/8;
	R_DATA->pixelsperbyte = 8 / R_DATA->BITSPERPIXEL;
	R_DATA->frontporch = MODE.hswr + MODE.hbsr + MODE.hdsr;
	R_DATA->topporch = MODE.vswr + MODE.vbsr + MODE.vdsr;
	R_DATA->bytes_per_line = R_DATA->XRES *
	    R_DATA->font->y_spacing/R_DATA->pixelsperbyte;
	R_DATA->bytes_per_scroll = (vc->ycur % R_DATA->font->y_spacing)
	    * vc->xcur / R_DATA->pixelsperbyte;
	R_DATA->text_width = R_DATA->XRES / R_DATA->font->x_spacing;
	R_DATA->text_height = R_DATA->YRES / R_DATA->font->y_spacing;
	vc->xchars = R_DATA->text_width;
	vc->ychars = R_DATA->text_height;
}

void
physcon_display_base(base)
	u_int base;
{
	dispstart = dispstart-dispbase + base;
	dispbase = vmem_base = base;
	dispend = base + dispsize;
	ptov = dispbase - phys_base;
}

static struct vidc_info masterinfo;
static int cursor_init = 0;

int
vidcconsole_init(vc)
	struct vconsole *vc;
{
	struct vidc_info *new;
	int loop;

	if ( cold_init==0 ) {
		vidcconsole_coldinit ( vc );
	} else {
		if ( cursor_init == 0 )
			vidcconsole_flash_go ( vc );
	}

    /*
     * If vc->r_data is initialised then this means that the previous
     * render engine on this vconsole was not freed properly.  I should
     * not try to clear it up, since I could panic the kernel.  Instead
     * I forget about its memory, which could cause a memory leak, but
     * this would be easily detectable and fixable
     */
     
#ifdef SELFTEST
	if ( vc->r_data != 0 ) {
		printf( "*********************************************************\n" );
		printf( "You have configured SELFTEST mode in the console driver\n" );
		printf( "vc->rdata non zero.  This could mean a new console\n"      );
		printf( "render engine has not freed up its data structure when\n"  );
		printf( "exiting.\n" 						     );
		printf( "DO NOT COMPILE NON DEVELOPMENT KERNELS WITH SELFTEST\n"    );
		printf( "*********************************************************" );
    	}
#endif

	if ( vc==vconsole_master ) {
		vc->r_data = (char *)&masterinfo;
	} else {
		MALLOC ( (vc->r_data), char *, sizeof(struct vidc_info),
		    M_DEVBUF, M_NOWAIT );
	}
    
	if (vc->r_data==0)
		panic ( "render engine initialisation failed.  CLEAN THIS UP!" );

	R_DATA->normalfont = &font_normal;
	R_DATA->italicfont = &font_italic;
	R_DATA->boldfont   = &font_bold;
	R_DATA->font = R_DATA->normalfont;

	vidcconsole_mode ( vc, vidc_currentmode );
	R_DATA->scrollback_end = dispstart;
    
	new = (struct vidc_info *)vc->r_data;

	R_DATA->text_colours = 1 << R_DATA->BITSPERPIXEL;
	if ( R_DATA->text_colours > 8 ) R_DATA->text_colours = 8;
    
#ifdef INVERSE_CONSOLE
	R_DATA->n_backcolour = R_DATA->text_colours - 1;
	R_DATA->n_forecolour = 0;
#else
	R_DATA->n_backcolour = 0;
	R_DATA->n_forecolour = R_DATA->text_colours - 1;
#endif

	R_DATA->backcolour = R_DATA->n_backcolour;
	R_DATA->forecolour = R_DATA->n_forecolour;

	R_DATA->forefillcolour = 0;
	R_DATA->backfillcolour = 0;

	R_DATA->bold = 0;
	R_DATA->reverse = 0;

	for (loop = 0; loop < R_DATA->pixelsperbyte; ++loop) {
		R_DATA->forefillcolour |= (R_DATA->forecolour <<
		    loop * R_DATA->BITSPERPIXEL);
		R_DATA->backfillcolour |= (R_DATA->backcolour <<
		    loop * R_DATA->BITSPERPIXEL);
	}

	R_DATA->fast_render = R_DATA->forecolour | (R_DATA->backcolour<<8) | (R_DATA->font->pixel_height<<16);
	R_DATA->blanked=0;
	vc->BLANK ( vc, BLANK_NONE );
    
	if ( vc == vconsole_current )
		vidcconsole_textpalette ( vc );

	vidc_cursor_init ( vc ) ;

	if ( cold_init == 0 ) {
		vidc_write ( VIDC_CP1, 0x0 );
		vidc_write ( VIDC_CP2, 0x0 );
		vidc_write ( VIDC_CP3, 0x0 );
	}
	cold_init=1;
	return 0;
}

void
vidcconsole_putchar(dev, c, vc)
	dev_t dev;
	char c;
	struct vconsole *vc;
{
	vc->PUTSTRING ( &c, 1, vc );
}

int
vidcconsole_spawn(vc)
	struct vconsole *vc;
{
	vc->xchars = R_DATA->text_width;
	vc->ychars = R_DATA->text_height;
	vc->xcur = 0;
	vc->ycur = 0;
	vc->flags = 0;
	return 0;
}

int
vidcconsole_redraw(vc, x, y, a, b)
	struct vconsole *vc;
	int x, y;
	int a, b;
{
	int xs, ys;
	struct vidc_state vidc;
	font_struct *p_font = R_DATA->font;
	int p_forecol = R_DATA->forecolour;
	int p_backcol = R_DATA->backcolour;
	if (x<0) x=0;
	if (y<0) y=0;
	if (x>(vc->xchars-1)) x=vc->xchars-1;
	if (y>(vc->ychars-1)) x=vc->ychars-1;

	if (a>(vc->xchars-1)) a=vc->xchars-1;
	if (b>(vc->ychars-1)) b=vc->ychars-1;

	if (a<x) a=x;
	if (b<y) b=y;


	vidc = *vidc_current;
	xs=vc->xcur;
	ys=vc->ycur;

	vc->xcur = 0;
	vc->ycur = 0;
 	if ( (vc->flags&LOSSY) == 0 )
	{
                register int c;
                /* This has *GOT* to be turboed */
                for ( vc->ycur=y; vc->ycur<=b; vc->ycur++ )
                {
                        for ( vc->xcur=x; vc->xcur<=a; vc->xcur++ )
                        {
                                c = (vc->charmap)[vc->xcur+vc->ycur*vc->xchars];
            			if ((c&BOLD)!=0)
					R_DATA->font = R_DATA->boldfont;
				else	
					R_DATA->font = R_DATA->normalfont;
R_DATA->fast_render = ((c>>8)&0x7)|(((c>>11)&0x7)<<8)| (R_DATA->font->pixel_height<<16);
if ( c & BLINKING )
 c+=1<<8 | 1;
				if ((c&BLINKING)!=0)
				{
				    R_DATA->forecolour+=16;
				    R_DATA->backcolour+=16;
				}
                                vidcconsole_render( vc, c&0xff );
                        }
                }
	}
	vc->xcur = xs;
	vc->ycur = ys;
	R_DATA->forecolour = p_forecol;
	R_DATA->backcolour = p_backcol;
	R_DATA->font = p_font;
	return 0;
}


int
vidcconsole_swapin(vc)
	struct vconsole *vc;
{
	register int counter;
	int xs, ys;
	struct vidc_state vidc;
	font_struct *p_font = R_DATA->font;
	int p_forecol = R_DATA->forecolour;
	int p_backcol = R_DATA->backcolour;

#ifdef ACTIVITY_WARNING
	vconsole_pending = 0;
#endif
	vidc_write ( VIDC_CP1, 0x0 );

	vidc = *vidc_current;
	vidc_write ( VIDC_PALREG, 0x00000000 );
	for ( counter=0; counter<255; counter++ )
		vidc_write ( VIDC_PALETTE, 0x00000000 );
	xs=vc->xcur;
	ys=vc->ycur;
/*TODO This needs to be vidc_restore (something) */
        vidcconsole_mode ( vc, &MODE );

	vc->xcur = 0;
	vc->ycur = 0;
 	if ( (vc->flags&LOSSY) == 0 )
	{
                register int c;
                /* This has *GOT* to be turboed */
                for ( vc->ycur=0; vc->ycur<vc->ychars; vc->ycur++ )
                {
                        for ( vc->xcur=0; vc->xcur<vc->xchars; vc->xcur++ )
                        {
                                c = (vc->charmap)[vc->xcur+vc->ycur*vc->xchars];
            			if ((c&BOLD)!=0)
					R_DATA->font = R_DATA->boldfont;
				else	
					R_DATA->font = R_DATA->normalfont;
/*
    				R_DATA->forecolour = ((c>>8)&0x7);
 				R_DATA->backcolour = ((c>>11)&0x7);
*/
R_DATA->fast_render = ((c>>8)&0x7)|(((c>>11)&0x7)<<8)| (R_DATA->font->pixel_height<<16);
if ( c & BLINKING )
 c+=1<<8 | 1;
				if ((c&BLINKING)!=0)
				{
				    R_DATA->forecolour+=16;
				    R_DATA->backcolour+=16;
				}
                                vidcconsole_render( vc, c&0xff );
                        }
                }
	}
	else
	{
	    vc->CLS ( vc );
	}

	if ( vc->vtty==1 )
	{
	    vc->xcur = xs;
	    vc->ycur = ys;
	    vidcconsole_textpalette ( vc );
	    vidc_write ( VIDC_CP1, 0xffffff );
 	    R_DATA->forecolour = p_forecol;
	    R_DATA->backcolour = p_backcol;
	    R_DATA->font = p_font;
	}
/* Make the cursor blank */
        WriteWord(IOMD_CURSINIT,p_cursor_transparent);
	return 0;

}

int
vidcconsole_mmap(vc, offset, nprot)
	struct vconsole *vc;
	int offset;
	int nprot;
{
	if (offset > videomemory.vidm_size)
		return (-1);
	return(arm_byte_to_page(((videomemory.vidm_pbase) + (offset))));
}

extern void vidcconsolemc_render __P(( unsigned char *addr, unsigned char *fontaddr,
			      int fast_render, int xres ));

void
vidcconsole_render(vc, c)
	struct vconsole *vc;
	char c;
{
	register unsigned char *fontaddr;
	register unsigned char *addr;

	/* Calculate the font's address */

	fontaddr = R_DATA->font->data
            + ((c-(0x20)) * R_DATA->font->height
            * R_DATA->font->width);

	addr = (unsigned char *)dispstart
            + (vc->xcur * R_DATA->font->x_spacing)
            + (vc->ycur * R_DATA->bytes_per_line);

	vidcconsolemc_render ( addr, fontaddr, R_DATA->fast_render,
    	    R_DATA->XRES );
}

/*
 * Uugh.  vidc graphics dont support scrolling regions so we have to emulate
 * it here.  This would normally require much software scrolling which is
 * horriblly slow, so I'm going to try and do a composite scroll, which
 * causes problems for scrollback but it's less speed critical
 */

void
vidcconsole_scrollup(vc, low, high)
	struct vconsole *vc;
	int low;
	int high;
{
    unsigned char *start, *end;

    if ( ( low==0 ) && ( high==vc->ychars-1 ))
    {
	/* All hardware scroll */
        dispstart+=R_DATA->bytes_per_line;
        if ( dispstart >= dispend )
    	    dispstart -= dispsize;

	high=high+1;	/* Big hack */

        WriteWord(IOMD_VIDINIT, dispstart - ptov );
    }
    else
    {
        char *oldstart=(char *)dispstart;

	/* Composite scroll */

	if ( (high-low) > (vc->ychars>>1) )
	{
	    /* Scroll region greater than half the screen */

            dispstart+=R_DATA->bytes_per_line;
            if ( dispstart >= dispend ) dispstart -= dispsize;

            WriteWord(IOMD_VIDINIT, dispstart - ptov );

	    if ( low!=0 )
	    {
    	        start = (unsigned char *)oldstart;
    	        end=(unsigned char*)oldstart+((low+1) * R_DATA->bytes_per_line);
    	        BCOPY ( start, start+R_DATA->bytes_per_line,
		    end-start-R_DATA->bytes_per_line);
	    }

	    if ( high!=(vc->ychars-1) )
	    {
    	        start =(unsigned char *)dispstart+(high)*R_DATA->bytes_per_line;
    	        end=(unsigned char*)dispstart+((vc->ychars)*R_DATA->bytes_per_line);
    	        BCOPY ( start, start+R_DATA->bytes_per_line,
		    end-start-R_DATA->bytes_per_line);
	    }
	    high++;
	}
	else
	{
	    /* Scroll region less than half the screen */

	    /* NO COMPOSITE SCROLL YET */

	    high++;
	    if (low<0) low=0;
	    if (high>(vc->ychars)) high=vc->ychars;
	    if (low>high) return;	/* yuck */
	    start = (unsigned char *)dispstart + ((low)*R_DATA->bytes_per_line);
	    end = (unsigned char *)dispstart +  ((high)*R_DATA->bytes_per_line);
	    BCOPY ( start+R_DATA->bytes_per_line, start,
		(end-start)-R_DATA->bytes_per_line );
	    R_DATA->scrollback_end = dispstart;
	}
    }
    memset ( (char *) dispstart + ((high-1)*R_DATA->bytes_per_line) ,
	     R_DATA->backfillcolour,
	     R_DATA->bytes_per_line   );
}

void
vidcconsole_scrolldown(vc, low, high)
	struct vconsole *vc;
	int low;
	int high;
{
    unsigned char *start;
    unsigned char *end;

    if ( low<0 ) low = 0;
    if ( high>(vc->ychars-1) ) high=vc->ychars-1;
    
    if ( ( low==0 ) && ( high==vc->ychars-1 ))
    {
        dispstart-=R_DATA->bytes_per_line;

        if ( dispstart < dispbase )
    	    dispstart += dispsize;

        WriteWord(IOMD_VIDINIT, dispstart - ptov );
    }
    else
    {
	if ( ((high-low) > (vc->ychars>>1)) )
	{
high--;
   	    if (high!=(vc->ychars-1))
	    {
	        start =(unsigned char*)dispstart+((high+1)*R_DATA->bytes_per_line);
	        end=(unsigned char*)dispstart+((vc->ychars)*R_DATA->bytes_per_line);
  	        BCOPY ( start+R_DATA->bytes_per_line, start,
	 	    (end-start)-R_DATA->bytes_per_line );
	    }

            dispstart-=R_DATA->bytes_per_line;
            if ( dispstart < dispbase )
    	        dispstart += dispsize;
            WriteWord(IOMD_VIDINIT, dispstart - ptov );
    	    start = (unsigned char *)dispstart + (low * R_DATA->bytes_per_line);

   	    if (low!=0)
	    {
	        end = (unsigned char *)dispstart + ((low+1)*R_DATA->bytes_per_line);
  	        BCOPY ( (char*)(dispstart+R_DATA->bytes_per_line),
			(char *)dispstart,
		    (int)((end-dispstart)-R_DATA->bytes_per_line ));
	    }
	}
        else
	{
    	    start = (unsigned char *)dispstart + (low * R_DATA->bytes_per_line);
    	    end = (unsigned char *)dispstart  + ((high+1) * R_DATA->bytes_per_line);
    	    BCOPY ( start, start+R_DATA->bytes_per_line, end-start-R_DATA->bytes_per_line);
	}

    }
    memset ((char*) dispstart + (low*R_DATA->bytes_per_line) ,
		R_DATA->backfillcolour, R_DATA->bytes_per_line );
}

void
vidcconsole_cls(vc)
	struct vconsole *vc;
{
#ifdef RC7500
	dispstart = dispbase;
	dispend = dispstart+dispsize;
    
	WriteWord ( IOMD_VIDINIT, dispstart-ptov );
	WriteWord ( IOMD_VIDSTART, dispstart-ptov );
	WriteWord ( IOMD_VIDEND, (dispend-transfersize)-ptov );
#endif
    
	vidcconsolemc_cls ( (char *)dispstart, (char *)dispstart+R_DATA->screensize, R_DATA->backfillcolour );
    /*
	memset((char *)dispstart,
		R_DATA->backfillcolour, R_DATA->screensize);
    */  
	vc->xcur = vc->ycur = 0;
}

void
vidcconsole_update(vc)
	struct vconsole *vc;
{
}

static char vidcconsole_name[] = VIDC_ENGINE_NAME;

static int scrollback_ptr = 0;

int
vidcconsole_scrollback(vc)
	struct vconsole *vc;
{
	int temp;

	if (scrollback_ptr==0)
	    scrollback_ptr=dispstart;

	temp = scrollback_ptr;

	scrollback_ptr-=R_DATA->bytes_per_line * (vc->ychars-2);
	
        if ( scrollback_ptr < dispbase )
    	    scrollback_ptr += dispsize;

	if (	(scrollback_ptr>dispstart)&&
		(scrollback_ptr<(dispstart+R_DATA->screensize) ) )
	{
	    scrollback_ptr=temp;
	    return 0;
	}

	vc->r_scrolledback = 1;

        WriteWord(IOMD_VIDINIT, scrollback_ptr - ptov );
	return 0;
}

int
vidcconsole_scrollforward(vc)
	struct vconsole *vc;
{
	register int temp;

	if (scrollback_ptr==0)
	    return 0;
	
	temp = scrollback_ptr;

	scrollback_ptr+=R_DATA->bytes_per_line * (vc->ychars - 2);
	
        if ( scrollback_ptr >= dispend )
    	    scrollback_ptr -= dispsize;

	if ( scrollback_ptr == dispstart )
	{
            WriteWord(IOMD_VIDINIT, scrollback_ptr - ptov );
	    scrollback_ptr=0;
	    vc->r_scrolledback = 0;
	    return 0;
	}

        WriteWord(IOMD_VIDINIT, scrollback_ptr - ptov );
	return 0;
}

int
vidcconsole_scrollbackend(vc)
	struct vconsole *vc;
{
	scrollback_ptr = 0;
        WriteWord(IOMD_VIDINIT, dispstart - ptov );
	vc->r_scrolledback = 0;
	return 0;
}

int
vidcconsole_clreos(vc, code)
	struct vconsole *vc;
	int code;
{
    char *addr;
    char *endofscreen;

    addr = (unsigned char *)dispstart
         + (vc->xcur * R_DATA->font->x_spacing)
         + (vc->ycur * R_DATA->bytes_per_line);

    endofscreen = (unsigned char *)dispstart
         + (vc->xchars * R_DATA->font->x_spacing)
         + (vc->ychars * R_DATA->bytes_per_line);


	switch (code)
	{
		case 0:
    			vidcconsolemc_cls ( addr, 
				(unsigned char *)dispend, 
				R_DATA->backfillcolour );
			if ((unsigned char *)endofscreen > (unsigned char *)dispend) {
				char string[80];
				sprintf(string, "(addr=%08x eos=%08x dispend=%08x dispstart=%08x base=%08x)",
				   (u_int)addr, (u_int)endofscreen, dispend, dispstart, dispbase);
				dprintf(string);
				vidcconsolemc_cls((unsigned char *)dispbase, (unsigned char *)(dispbase + (endofscreen - dispend)), R_DATA->backfillcolour);
			}
			break;

		case 1:
    			vidcconsolemc_cls ( (unsigned char *)dispstart+R_DATA->screensize, 
				addr, 
				R_DATA->backfillcolour );
			break;
			
		case 2:
		default:
			vidcconsole_cls ( vc );
			break;
	}
	return 0;
}

#define VIDC R_DATA->vidc

int
vidcconsole_debugprint(vc)
	struct vconsole *vc;
{
#ifdef	DEVLOPING
    printf ( "VIDCCONSOLE DEBUG INFORMATION\n\n" );
    printf ( "res (%d, %d) charsize (%d, %d) cursor (%d, %d)\n"
							      , R_DATA->XRES, R_DATA->YRES
							      , vc->xchars, vc->ychars, vc->xcur, vc->ycur );
    printf ( "bytes_per_line %d\n"			      , R_DATA->bytes_per_line 		     );
    printf ( "pixelsperbyte  %d\n"			      , R_DATA->pixelsperbyte		     );
    printf ( "dispstart      %08x\n"			      , dispstart 		     );
    printf ( "dispend        %08x\n"			      , dispend 		     );
    printf ( "screensize     %08x\n"			      , R_DATA->screensize 		     );

    printf ( "fontwidth      %08x\n"			      , R_DATA->font->pixel_width 	     );
    printf ( "fontheight     %08x\n"			      , R_DATA->font->pixel_height           );
    printf ( "\n" 										     );
    printf ( "palreg = %08x  bcol    = %08x\n"		      , VIDC.palreg, VIDC.bcol		     );
    printf ( "cp1    = %08x  cp2     = %08x  cp3    = %08x\n" , VIDC.cp1, VIDC.cp2, VIDC.cp3         );
    printf ( "hcr    = %08x  hswr    = %08x  hbsr   = %08x\n" , VIDC.hcr, VIDC.hswr, VIDC.hbsr       );
    printf ( "hder   = %08x  hber    = %08x  hcsr   = %08x\n" , VIDC.hder, VIDC.hber, VIDC.hcsr      );
    printf ( "hir    = %08x\n"				      , VIDC.hir 			     );
    printf ( "vcr    = %08x  vswr    = %08x  vbsr   = %08x\n" , VIDC.vcr, VIDC.vswr, VIDC.vbsr       );
    printf ( "vder   = %08x  vber    = %08x  vcsr   = %08x\n" , VIDC.vder, VIDC.vber, VIDC.vcsr      );
    printf ( "vcer   = %08x\n"				      , VIDC.vcer 			     );
    printf ( "ereg   = %08x  fsynreg = %08x  conreg = %08x\n" , VIDC.ereg, VIDC.fsynreg, VIDC.conreg );
    printf ( "\n" );
    printf ( "flash %08x, cursor_flash %08x", R_DATA->flash, R_DATA->cursor_flash );
#else
    printf ( "VIDCCONSOLE - NO DEBUG INFO\n" );
#endif
    return 0;
}

#ifdef NICE_UPDATE
static int need_update = 0;

void
vidcconsole_updatecursor(arg)
	void *arg;
{
	struct vconsole *vc = vconsole_current;

	vidc_write(VIDC_HCSR, R_DATA->frontporch-17+ (vc->xcur)*R_DATA->font->pixel_width );
	vidc_write(VIDC_VCSR, R_DATA->topporch-2+ (vc->ycur+1)*R_DATA->font->pixel_height-2 + 3
	    - R_DATA->font->pixel_height);
	vidc_write(VIDC_VCER, R_DATA->topporch-2+ (vc->ycur+3)*R_DATA->font->pixel_height+2 + 3 );
	return;
}

int
vidcconsole_cursorupdate(vc)
	struct vconsole *vc;
{
    timeout ( vidcconsole_updatecursor, NULL, 20 );
    return 0;
}

#else

static int
vidcconsole_cursorupdate(vc)
	struct vconsole *vc;
{
	vidc_write(VIDC_HCSR, R_DATA->frontporch-17+ (vc->xcur)*R_DATA->font->pixel_width );
	vidc_write(VIDC_VCSR, R_DATA->topporch-2+ (vc->ycur+1)*R_DATA->font->pixel_height-2 + 3
	    - R_DATA->font->pixel_height);
	vidc_write(VIDC_VCER, R_DATA->topporch-2+ (vc->ycur+3)*R_DATA->font->pixel_height+2 + 3 );
	return (0);
}

#endif

#define DEFAULT_CURSORSPEED (25)

static int CURSORSPEED = DEFAULT_CURSORSPEED;

static int
vidcconsole_cursorflashrate(vc, rate)
	struct vconsole *vc;
	int rate;
{
	CURSORSPEED = 60/rate;
}

static int cursorcounter=DEFAULT_CURSORSPEED;
static int flashcounter=DEFAULT_CURSORSPEED;
#ifdef PRETTYCURSOR
static int pretty=0xff;
#endif

static int cursor_col = 0x0;

static int
vidcconsole_cursorintr(vc)
	struct vconsole *vc;
{
	if ( cursor_flash==0 )
		return 0;

	/*
	 * We don't need this.
	 */
#ifndef RC7500
	vconsole_blankcounter--;

	if ( vconsole_blankcounter<0 ) {
		vconsole_blankcounter=vconsole_blankinit;
		vidcconsole_blank ( vc, BLANK_OFF );
	}
#endif

	cursorcounter--;
	if (cursorcounter<=0) {
		cursorcounter=CURSORSPEED;
		cursor_col = cursor_col ^ 0xffffff;
#ifdef ACTIVITY_WARNING
		if (vconsole_pending)  {
			if ( cursor_col==0 )
				WriteWord(IOMD_CURSINIT,p_cursor_transparent);
		        else
				WriteWord(IOMD_CURSINIT,p_cursor_normal);
			vidc_write ( VIDC_CP1, cursor_col&0xff );
		} else
#endif
		{
			if ( cursor_col==0 )
				WriteWord(IOMD_CURSINIT,p_cursor_transparent);
			else
				WriteWord(IOMD_CURSINIT,p_cursor_normal);
			vidc_write ( VIDC_CP1, 0xffffff );
		}
	}
	return(0);
}

int
vidcconsole_flashintr(vc)
	struct vconsole *vc;
{
	if ( flash==0 )
		return 0;

	flashcounter--;
	if (flashcounter<=0) {
		flashcounter=CURSORSPEED;
		if ( cursor_col == 0 ) {

			vidc_write(VIDC_PALREG, 0x00000010);
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(255,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0, 255,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(255, 255,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0, 255));
			vidc_write(VIDC_PALETTE, VIDC_COL(255,   0, 255));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0, 255, 255));
			vidc_write(VIDC_PALETTE, VIDC_COL(255, 255, 255));
			vidc_write(VIDC_PALETTE, VIDC_COL(128, 128, 128));
			vidc_write(VIDC_PALETTE, VIDC_COL(255, 128, 128));
			vidc_write(VIDC_PALETTE, VIDC_COL(128, 255, 128));
			vidc_write(VIDC_PALETTE, VIDC_COL(255, 255, 128));
			vidc_write(VIDC_PALETTE, VIDC_COL(128, 128, 255));
			vidc_write(VIDC_PALETTE, VIDC_COL(255, 128, 255));
			vidc_write(VIDC_PALETTE, VIDC_COL(255, 255, 255));
		} else {
			vidc_write(VIDC_PALREG, 0x00000010);
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
			vidc_write(VIDC_PALETTE, VIDC_COL(  0,   0,   0));
		}
	}
	return(0);
}

static int
vidc_cursor_init(vc)
	struct vconsole *vc;
{
	extern char *cursor_data;
	int counter;
	int line;

	/* Blank the cursor while initialising it's sprite */

	vidc_write ( VIDC_CP1, 0x0 );
	vidc_write ( VIDC_CP2, 0x0 );
	vidc_write ( VIDC_CP3, 0x0 );

 	cursor_normal       = cursor_data;
	cursor_transparent  = cursor_data + (R_DATA->font->pixel_height *
					R_DATA->font->pixel_width);

 	cursor_transparent += 32;
	cursor_transparent = (char *)((int)cursor_transparent & (~31) );

	for ( line = 0; line<R_DATA->font->pixel_height; ++ line )
	{
	    for ( counter=0; counter<R_DATA->font->pixel_width/4;counter++ )
		cursor_normal[line*R_DATA->font->pixel_width + counter]=0x55;
	    for ( ; counter<8; counter++ )
		cursor_normal[line*R_DATA->font->pixel_width + counter]=0;
	}

	for ( line = 0; line<R_DATA->font->pixel_height; ++ line )
	{
	    for ( counter=0; counter<R_DATA->font->pixel_width/4;counter++ )
		cursor_transparent[line*R_DATA->font->pixel_width + counter]=0x00;
	    for ( ; counter<8; counter++ )
		cursor_transparent[line*R_DATA->font->pixel_width + counter]=0;
	}


	p_cursor_normal = pmap_extract(kernel_pmap,(vm_offset_t)cursor_normal );
	p_cursor_transparent = pmap_extract(kernel_pmap,(vm_offset_t)cursor_transparent);

/*
	memset ( cursor_normal, 0x55,
		R_DATA->font->pixel_width*R_DATA->font->pixel_height );

	memset ( cursor_transparent, 0x55,
		R_DATA->font->pixel_width*R_DATA->font->pixel_height );
*/

	/* Ok, now see the cursor */

	vidc_write ( VIDC_CP1, 0xffffff );
        return 0;
}

int
vidcconsole_setfgcol(vc, col)
	struct vconsole *vc;
	int col;
{
	register int loop;

	if ( R_DATA->forecolour >= 16 )
		R_DATA->forecolour=16;
	else
		R_DATA->forecolour=0;
    
	R_DATA->forefillcolour = 0;
    
	R_DATA->forecolour += col;
    
/*TODO
	if ( R_DATA->forecolour >> 1<<R_DATA->BITSPERPIXEL )
		R_DATA->forecolour>>1;
*/

	for (loop = 0; loop < R_DATA->pixelsperbyte; ++loop) {
		R_DATA->forefillcolour |= (R_DATA->forecolour <<
		    loop * R_DATA->BITSPERPIXEL);
	}
	R_DATA->fast_render = R_DATA->forecolour | (R_DATA->backcolour<<8) | (R_DATA->font->pixel_height<<16);
	return 0;
}

int
vidcconsole_setbgcol(vc, col)
	struct vconsole *vc;
	int col;
{
	register int loop;

	if ( R_DATA->backcolour >= 16 )
		R_DATA->backcolour=16;
	else
		R_DATA->backcolour=0;

	R_DATA->backfillcolour = 0;
	R_DATA->backcolour += col;
 /*TODO   
	if ( R_DATA->backcolour >> 1<<R_DATA->BITSPERPIXEL )
		R_DATA->backcolour>>1;
*/

	for (loop = 0; loop < R_DATA->pixelsperbyte; ++loop) {
		R_DATA->backfillcolour |= (R_DATA->backcolour <<
		    loop * R_DATA->BITSPERPIXEL);
	}
	return 0;
}

int
vidcconsole_textpalette(vc)
	struct vconsole *vc;
{
        R_DATA->forecolour = COLOUR_WHITE_8;
        R_DATA->backcolour = COLOUR_BLACK_8;

        vidc_write( VIDC_PALREG , 0x00000000);
        vidc_write( VIDC_PALETTE , VIDC_COL(  0,   0,   0));
        vidc_write( VIDC_PALETTE , VIDC_COL(255,   0,   0));
        vidc_write( VIDC_PALETTE , VIDC_COL(  0, 255,   0));
        vidc_write( VIDC_PALETTE , VIDC_COL(255, 255,   0));
        vidc_write( VIDC_PALETTE , VIDC_COL(  0,   0, 255));
        vidc_write( VIDC_PALETTE , VIDC_COL(255,   0, 255));
        vidc_write( VIDC_PALETTE , VIDC_COL(  0, 255, 255));
        vidc_write( VIDC_PALETTE , VIDC_COL(255, 255, 255));
        vidc_write( VIDC_PALETTE , VIDC_COL(128, 128, 128));
        vidc_write( VIDC_PALETTE , VIDC_COL(255, 128, 128));
        vidc_write( VIDC_PALETTE , VIDC_COL(128, 255, 128));
        vidc_write( VIDC_PALETTE , VIDC_COL(255, 255, 128));
        vidc_write( VIDC_PALETTE , VIDC_COL(128, 128, 255));
        vidc_write( VIDC_PALETTE , VIDC_COL(255, 128, 255));
        vidc_write( VIDC_PALETTE , VIDC_COL(255, 255, 255));

R_DATA->fast_render = R_DATA->forecolour | (R_DATA->backcolour<<8) | (R_DATA->font->pixel_height<<16);
    return 0;
}

int
vidcconsole_sgr (vc, type)
	struct vconsole *vc;
	int type;
{
    switch ( type )
    {
        case 0: /* Normal */
	    if (R_DATA->BITSPERPIXEL == 8)
      	    {
        	R_DATA->n_forecolour = COLOUR_WHITE_8;
        	R_DATA->n_backcolour = COLOUR_BLACK_8;
      	    }

	    R_DATA->forecolour = R_DATA->n_forecolour;
            R_DATA->backcolour = R_DATA->n_backcolour;
            R_DATA->font = R_DATA->normalfont;
            break;

	case 1: /* bold */
            R_DATA->font = R_DATA->boldfont;
	    break;	

	case 22: /* not bold */
            R_DATA->font = R_DATA->normalfont;
	    break;	

        case 5:	/* blinking */
	    if ( R_DATA->forecolour < 16 )
	    {
		R_DATA->forecolour+=16;
		R_DATA->backcolour+=16;
		R_DATA->n_forecolour+=16;
		R_DATA->n_backcolour+=16;
	    }
	    break;

        case 25: /* not blinking */
	    if ( R_DATA->forecolour >= 16 )
	    {
		R_DATA->forecolour-=16;
		R_DATA->backcolour-=16;
		R_DATA->n_forecolour-=16;
		R_DATA->n_backcolour-=16;
	    }
	    break;

        case 7: /* reverse */
	    R_DATA->forecolour = R_DATA->n_backcolour;
            R_DATA->backcolour = R_DATA->n_forecolour;
    	    break;
            
        case 27: /* not reverse */
	    R_DATA->forecolour = R_DATA->n_forecolour;
            R_DATA->backcolour = R_DATA->n_backcolour;
    	    break;
    }
R_DATA->fast_render = R_DATA->forecolour | (R_DATA->backcolour<<8) | (R_DATA->font->pixel_height<<16);
    return 0;
}

int
vidcconsole_scrollregion(vc, low, high)
	struct vconsole *vc;
	int low;
	int high;
{
    return 0;
}

int
vidcconsole_blank(vc, type)
	struct vconsole *vc;
	int type;
{
	vc->blanked=type;
	switch (type) {
	case 0:
#ifdef RC7500
		vidc_write ( VIDC_EREG, 0x51<<12 );
#else
    		vidc_write ( VIDC_EREG, 1<<12 );
#endif
		break;
		
	case 1: /* not implemented yet */
	case 2:
	case 3:
		vidc_write ( VIDC_EREG, 0 );
		break;
	}
	return 0;
}

extern struct tty *find_tp __P((dev_t dev));

int vidcconsole_ioctl ( struct vconsole *vc, dev_t dev, int cmd, caddr_t data,
			int flag, struct proc *p )
{
	int error;
	struct tty *tp;
	struct winsize ws;
	switch ( cmd )
	{
		case CONSOLE_MODE:
    			tp = find_tp(dev);
			printf ( "mode ioctl called\n" );
			vidcconsole_mode ( vc, (struct vidc_mode *)data );
    			vc->MODECHANGE ( vc );
			ws.ws_row=vc->ychars;
			ws.ws_col=vc->xchars;
			error = (*linesw[tp->t_line].l_ioctl)(tp, TIOCSWINSZ, (char *)&ws, flag, p);
                        error = ttioctl(tp, TIOCSWINSZ, (char *)&ws, flag, p);
			return 0;
			break;

	case CONSOLE_RESETSCREEN:
	{
	extern unsigned int dispbase;
	extern unsigned int dispsize;
	extern unsigned int ptov;
	extern unsigned int transfersize;

	WriteWord ( IOMD_VIDINIT, dispbase-ptov );
	WriteWord ( IOMD_VIDSTART, dispbase-ptov );
	WriteWord ( IOMD_VIDEND, (dispbase+dispsize-transfersize)-ptov );
	return 0;
        }
	case CONSOLE_RESTORESCREEN:
	{
		extern unsigned int dispstart;
		extern unsigned int dispsize;
		extern unsigned int ptov;
		extern unsigned int transfersize;

		WriteWord ( IOMD_VIDINIT, dispstart-ptov );
		WriteWord ( IOMD_VIDSTART, dispstart-ptov );
		WriteWord ( IOMD_VIDEND, (dispstart+dispsize-transfersize)-ptov );
		vidc_stdpalette();
		return 0;
        }
	case CONSOLE_GETINFO:
	{
		extern videomemory_t videomemory;
		register struct console_info *inf = (void *)data;


		inf->videomemory = videomemory;
		inf->width = R_DATA->mode.hder;
		inf->height = R_DATA->mode.vder;
		inf->bpp = R_DATA->mode.bitsperpixel;
		return 0;
	}
	case CONSOLE_PALETTE:
	{
		register struct console_palette *pal = (void *)data;
		vidc_write(VIDC_PALREG, pal->entry);
		vidc_write(VIDC_PALETTE, VIDC_COL(pal->red, pal->green, pal->blue));
 		return 0;
	}
	}
	return -1;
}

int
vidcconsole_attach(vc, parent, self, aux)
	struct vconsole *vc;
	struct device *parent;
	struct device *self;
	void *aux;
{
	vidc_cursor_init ( vc );
	vidcconsole_flash_go ( vc );
	return 0;
}

int
vidcconsole_flash_go(vc)
	struct vconsole *vc;
{
	static lock=0;
	if (lock==1)
		return -1;
	lock=0;

	cursor_ih.ih_func = vidcconsole_cursorintr;
	cursor_ih.ih_arg = vc;
	cursor_ih.ih_level = IPL_TTY;
	cursor_ih.ih_name = "vsync";
	irq_claim ( IRQ_FLYBACK, &cursor_ih );

	cursor_init = 0;
	return 0;
}

/* What does this function do ? */
int 
vidcconsole_flash(vc, flash)
	struct vconsole *vc;
	int flash;
{
	flash = flash;
}

int
vidcconsole_cursorflash(vc, flash)
	struct vconsole *vc;
	int flash;
{
	cursor_flash = flash;
	return(0);
}

struct render_engine vidcconsole = {
	vidcconsole_name,
	vidcconsole_init,
	
	vidcconsole_putchar,

	vidcconsole_spawn,
	vidcconsole_swapin,
	vidcconsole_mmap,
	vidcconsole_render,
	vidcconsole_scrollup,
	vidcconsole_scrolldown,
	vidcconsole_cls,
	vidcconsole_update,
	vidcconsole_scrollback,
	vidcconsole_scrollforward,
	vidcconsole_scrollbackend,
	vidcconsole_clreos,
	vidcconsole_debugprint,
	vidcconsole_cursorupdate,
	vidcconsole_cursorflashrate,
	vidcconsole_setfgcol,
	vidcconsole_setbgcol,
	vidcconsole_textpalette,
	vidcconsole_sgr,
	vidcconsole_blank,
	vidcconsole_ioctl,
	vidcconsole_redraw,
	vidcconsole_attach,
	vidcconsole_flash,
	vidcconsole_cursorflash
};







struct vidcvideo_softc {
	struct device device;
	int sc_opened;
};

int
vidcvideo_probe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
/*
	struct vidcvideo_softc *vidcvideosoftc = (void *)match;
	struct mainbus_attach_args *mb = aux;
*/

	return 1;
}

void
vidcvideo_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct vidcvideo_softc *vidcvideosoftc = (void *)self;
	vidcvideosoftc->sc_opened=0;

	printf ( ": vidc 20\n" );
}

struct cfattach vidcvideo_ca = {
	sizeof (struct vidcvideo_softc), vidcvideo_probe, vidcvideo_attach
};

struct cfdriver vidcvideo_cd = {
	NULL, "vidcvideo", DV_DULL
};

int
vidcvideoopen(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{
	struct vidcvideo_softc *sc;
	struct vconsole vconsole_new;
	int unit = minor(dev);
	int s;

	if ( unit >= vidcvideo_cd.cd_ndevs )
		return ENXIO;
	sc = vidcvideo_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	s = spltty();
/*	if (sc->sc_opened) {
		(void)splx(s);
		return(EBUSY);
	}*/
	++sc->sc_opened;
	(void)splx(s);

	if (sc->sc_opened == 1) {
		vconsole_new = *vconsole_default;
		vconsole_new.render_engine = &vidcconsole;
		vconsole_spawn_re (
		makedev ( physcon_major, 64 + minor(dev) ),
		    &vconsole_new );
	} else {
		log(LOG_WARNING, "Multiple open of/dev/vidcvideo0 by proc %d\n", p->p_pid);
	}

	return 0;
}

int
vidcvideoclose(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{
	struct vidcvideo_softc *sc;
	int unit = minor(dev);
	int s;

	if ( unit >= vidcvideo_cd.cd_ndevs )
		return ENXIO;
	sc = vidcvideo_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	s = spltty();
	--sc->sc_opened;
	(void)splx(s);

	return 0;
}

extern int physconioctl __P(( dev_t, int, caddr_t, int,	struct proc *));

int
vidcvideoioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	dev = makedev(physcon_major, 64 + minor(dev));
	return ( physconioctl ( dev, cmd, data, flag, p ));
}

extern int physconmmap __P((dev_t, int, int));

int
vidcvideommap(dev, offset, prot)
	dev_t dev;
	int offset;
	int prot;
{
	dev = makedev(physcon_major, 64 + minor(dev));
	return(physconmmap(dev, offset, prot));
}

