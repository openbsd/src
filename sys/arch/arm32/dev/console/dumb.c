/* $NetBSD: dumb.c,v 1.2 1996/03/18 19:33:05 mark Exp $ */

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
 * dumb.c
 *
 * Console functions
 *
 * Created      : 17/09/94
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <machine/param.h>
#include <machine/katelib.h>
#include <machine/cpu.h>
#include <machine/bootconfig.h>
#include <machine/iomd.h>
#include <machine/vidc.h>
#include <machine/vconsole.h>

#define TERMTYPE_PUTSTRING	dumb_putstring
#define TERMTYPE_INIT	dumb_init

int
TERMTYPE_INIT(vc)
	struct vconsole *vc;
{
	/* This dumb termial is so dumb it requires very little init */
	return 0;
}

static void
do_scrollup(vc)
	struct vconsole *vc;
{

	if (vc==vconsole_current)
		vc->SCROLLUP ( vc, 0, vc->ychars-1 );

	vc->ycur=vc->ychars-1;

	if ( ((vc->flags)&(LOSSY)) == 0 ) {
		int counter;
		for ( counter=vc->xchars; counter < ((vc->ychars)*(vc->xchars)); counter++ ) {
			vc->charmap[counter-vc->xchars] = vc->charmap[counter];
		}
		for ( counter=vc->xchars*(vc->ychars-1); counter < (vc->xchars*vc->ychars); counter++ ) {
			vc->charmap[counter]=0x20;
		}
	}
}

static int
do_render(c, vc)
	char c;
	struct vconsole *vc;
{
	/* THE RENDER STAGE **********************************/
	if ((c>=0x20)&&(c<=0x7f)) {
		if (((vc->flags)&(LOSSY))==0) {
			vc->charmap[ vc->xcur + vc->ycur*vc->xchars ] = c | 7<<8;
		}

		if ( vc==vconsole_current )
			vc->RENDER ( vc, c );

		vc->xcur++;
	}

	if ( vc->xcur >= vc->xchars ) {
		vc->xcur=0;
		vc->ycur++;
	}

	if ( vc->ycur >= vc->ychars ) {
		do_scrollup ( vc );
		vc->xcur=0;
		vc->ycur=vc->ychars-1;
	}
}
	
int
TERMTYPE_PUTSTRING(string, length, vc)
	char *string;
	int length;
	struct vconsole *vc;
{
	char c;

	while ( ((c=*(string++))!=0) && ((length--)>0)) {
		if ((c<31)||(c>127)) c='*';
		switch (c) {
		case 0x0a:
			vc->ycur++;
			if ( vc->ycur>=vc->ychars ) {
				do_scrollup ( vc );
				vc->ycur=vc->ychars-1;
			}
			break;

		case 0x0d:
			vc->xcur=0;
			break;

		default:
			do_render ( c, vc );
			break;
		}
	}
}
