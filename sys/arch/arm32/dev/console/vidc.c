/* $NetBSD: vidc.c,v 1.2 1996/03/18 19:33:07 mark Exp $ */

/*
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
 * vidc.c
 *
 * Old console ioctl declarations
 *
 * Created      : 17/09/94
 */

#include <sys/types.h>
#include <machine/vidc.h>
#include <machine/katelib.h>

/* VIDC STUFF */

/*
 * A structure containing ALL the information required to restore
 * the VIDC20 to any given state.  ALL vidc transactions should
 * go through these procedures, which record the vidc's state.
 * it may be an idea to set the permissions of the vidc base address
 * so we get a fault, so the fault routine can record the state but
 * I guess that's not really necessary for the time being, since we
 * can make the kernel more secure later on.  Also, it is possible
 * to write a routine to allow 'reading' of the vidc registers.
 */

struct vidc_state vidc_lookup = {	
	{ 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
          0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
	},

	VIDC_PALREG,
	VIDC_BCOL,
	VIDC_CP1 ,
	VIDC_CP2,
	VIDC_CP3,
	VIDC_HCR,
	VIDC_HSWR,
	VIDC_HBSR,
	VIDC_HDSR,
	VIDC_HDER,
	VIDC_HBER,
	VIDC_HCSR,
	VIDC_HIR,
	VIDC_VCR,
	VIDC_VSWR,
	VIDC_VBSR,
	VIDC_VDSR,
	VIDC_VDER,
	VIDC_VBER,
	VIDC_VCSR,
	VIDC_VCER,
	VIDC_EREG,
	VIDC_FSYNREG,
	VIDC_CONREG,
	VIDC_DCTL
};

struct vidc_state vidc_current[1];
 
int
vidc_write(reg, value)
	u_int reg;
	int value;
{
	int counter;

	int *current;
	int *tab;

	tab 	= (int *)&vidc_lookup;
	current = (int *)vidc_current; 

	/* End higly doddgy code */
/*
WriteWord ( VIDC_BASE, reg | value );
return 1;
*/

	/*
	 * OK, the VIDC_PALETTE register is handled differently
	 * to the others on the VIDC, so take that into account here
	 */
	if (reg==VIDC_PALREG) {
		vidc_current->palreg = 0;
		WriteWord ( VIDC_BASE, reg | value );
		return 0;
	}

	if (reg==VIDC_PALETTE) {
		WriteWord ( VIDC_BASE, reg | value );
		vidc_current->palette[vidc_current->palreg] = value;
		vidc_current->palreg++;
		vidc_current->palreg = vidc_current->palreg & 0xff;
		return 0;
	}

	/*
	 * Undefine SAFER if you wish to speed things up (a little)
	 * although this means the function will assume things abou
	 * the structure of vidc_state. i.e. the first 256 words are
	 * the palette array
	 */

#define SAFER

#ifdef 	SAFER
#define INITVALUE 0
#else
#define INITVALUE 256
#endif

	for ( counter=INITVALUE; counter<= sizeof(struct vidc_state); counter++ ) {
		if ( reg==tab[counter] ) {
			WriteWord ( VIDC_BASE, reg | value );
			current[counter] = value;
			return 0;
		}
	}
	return -1;
}

void
vidc_setpalette(vidc)
	struct vidc_state *vidc;
{
	int counter = 0;

	vidc_write(VIDC_PALREG, 0x00000000);
	for (counter = 0; counter < 255; counter++)
		vidc_write(VIDC_PALETTE, vidc->palette[counter]);
}

void
vidc_setstate(vidc)
	struct vidc_state *vidc;
{
	vidc_write ( VIDC_PALREG,	vidc->palreg 	);
	vidc_write ( VIDC_BCOL,		vidc->bcol	);
	vidc_write ( VIDC_CP1,		vidc->cp1	);
	vidc_write ( VIDC_CP2,		vidc->cp2	);
	vidc_write ( VIDC_CP3,		vidc->cp3	);
	vidc_write ( VIDC_HCR,		vidc->hcr	);
	vidc_write ( VIDC_HSWR,		vidc->hswr	);
	vidc_write ( VIDC_HBSR,		vidc->hbsr	);
	vidc_write ( VIDC_HDSR,		vidc->hdsr	);
	vidc_write ( VIDC_HDER,		vidc->hder	);
	vidc_write ( VIDC_HBER,		vidc->hber	);
	vidc_write ( VIDC_HCSR,		vidc->hcsr	);
	vidc_write ( VIDC_HIR,		vidc->hir	);
	vidc_write ( VIDC_VCR,		vidc->vcr	);
	vidc_write ( VIDC_VSWR,		vidc->vswr	);
	vidc_write ( VIDC_VBSR,		vidc->vbsr	);
	vidc_write ( VIDC_VDSR,		vidc->vdsr	);
	vidc_write ( VIDC_VDER,		vidc->vder	);
	vidc_write ( VIDC_VBER,		vidc->vber	);
	vidc_write ( VIDC_VCSR,		vidc->vcsr	);
	vidc_write ( VIDC_VCER,		vidc->vcer	);
/*
 * Right, dunno what to set these to yet, but let's keep RiscOS's
 * ones for now, until the time is right to finish this code
 */	

/*	vidc_write ( VIDC_EREG,		vidc->ereg	);	*/
/*	vidc_write ( VIDC_FSYNREG,	vidc->fsynreg	);	*/
/*	vidc_write ( VIDC_CONREG,	vidc->conreg	);	*/
/*	vidc_write ( VIDC_DCTL,		vidc->dctl	);	*/

}


void
vidc_getstate(vidc)
	struct vidc_state *vidc;
{
	*vidc = *vidc_current;
}

void
vidc_stdpalette()
{
        WriteWord(VIDC_BASE, VIDC_PALREG | 0x00000000);
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(  0,   0,   0));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(255,   0,   0));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(  0, 255,   0));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(255, 255,   0));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(  0,   0, 255));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(255,   0, 255));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(  0, 255, 255));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(255, 255, 255));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(128, 128, 128));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(255, 128, 128));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(128, 255, 128));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(255, 255, 128));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(128, 128, 255));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(255, 128, 255));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(128, 255, 255));
        WriteWord(VIDC_BASE, VIDC_PALETTE | VIDC_COL(255, 255, 255));
}

#if 0
int
vidc_col(red, green, blue)
	int red;
	int green;
	int blue;
{
	red 	= red 	& 0xFF;
	green 	= green & 0xFF;
	blue 	= blue 	& 0xFF;

	return ( (blue<<16) + (green<<8) + red );
}

#endif
