/* $NetBSD: vt220.c,v 1.3 1996/03/18 19:33:10 mark Exp $ */

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
 * vt220.c
 *
 * VT220 emulation functions
 *
 * Created      : 17/09/94
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <machine/param.h>
#include <machine/katelib.h>
#include <machine/cpu.h>
#include <machine/bootconfig.h>
#include <machine/iomd.h>
#include <machine/vconsole.h>
#include <machine/vidc.h>

#include <arm32/dev/console/vt220.h>

/* Temporary for debugging the vnode bug */

int vnodeconsolebug = 0;
u_int vnodeconsolebug1 = 0;
u_int vnodeconsolebug2 = 0;

static char vt220_name[] = "vt100";

/* These defines are for the developer only !!!! */
#define SELFTEST
#define dprintf(x)	;

/*************************************************/

#define FGCOL	0x0700
#define BGCOL	0x3800

/* Options */
#define HARD_RESET_AT_INIT
#undef SIMPLE_CURSOR	/* Define if any render engine is cursorless */

/*************************************************/

#define DA_VT220     "\033[?62;1;2;6;7;8;9c"

char console_proc[41];	/* Is this debugging ? */

#ifdef SIMPLE_CURSOR
#define SIMPLE_CURSOR_CHAR ('_')
#endif

#define TERMTYPE_PUTSTRING	vt220_putstring
#define TERMTYPE_INIT	vt220_init

extern struct vconsole *vconsole_master;

static int default_beepstate = 0;

#define CDATA struct vt220_info *cdata = (struct vt220_info *)vc->data

struct vt220_info master_termdata_store;
struct vt220_info *master_termdata = &master_termdata_store;

int do_render __P(( char /*c*/, struct vconsole */*vc*/ ));
void do_render_noscroll __P(( char /*c*/, struct vconsole */*vc*/ ));
void do_scrollcheck __P(( struct vconsole */*vc*/ ));
void vt_ris __P((struct vconsole */*vc*/));

void
clr_params(cdata)
	struct vt220_info *cdata;
{
	int i;
	for (i=0; i<MAXPARMS; i++)
		cdata->param[i] = 0;
	cdata->parami = 0;
}

int
TERMTYPE_INIT(vc)
	struct vconsole *vc;
{
	struct vt220_info *cdata;

	if (vc->data==NULL) {
		if ( vc==vconsole_master ) {
			vc->data = (char *) master_termdata;
		} else {
			MALLOC ( vc->data, char *, sizeof(struct vt220_info),
			    M_DEVBUF, M_NOWAIT );
		}
	}

	cdata = (struct vt220_info *)vc->data;

	bzero ( (char *)cdata, sizeof (cdata) );

#ifdef HARD_RESET_AT_INIT
	vt_ris ( vc );
#else
	cdata->state = STATE_INIT;
	cdata->disable_function = 1;
	cdata->m_om = 0;
	vc->xcur = vc->ycur = 0;
	cdata->beepoff=default_beepstate;
	cdata->simple_cursor_store = ' ';
	cdata->scrr_beg = 0;
	cdata->scrr_len = vc->ychars;
	cdata->scrr_end = vc->ychars-1;
	cdata->simple_cursor_on = 0;

	for ( counter=0; counter<MAXTABSTOPS; counter++ ) {
		if ( !(counter%8) )
			cdata->tab_stops[counter] = 1;
		else
			cdata->tab_stops[counter] = 0;
	}
#endif
	return 0;
}

int
mapped_cls(vc)
	struct vconsole *vc;
{
	int ptr;
	if (vc->charmap == NULL)
		return -1;
	for ( ptr=0; ptr<(vc->xchars*vc->ychars); ptr++ )
		vc->charmap[ptr]=0x20;
	return 0;
}

void
do_scrolldown(vc)
	struct vconsole *vc;
{
	struct vt220_info *cdata = (struct vt220_info *)vc->data;

/*
	if ( ( cdata->scrr_beg<0 ) || ( cdata->scrr_end>=vc->ychars) )
		dprintf ( "INVALID SCROLLDOWN" );
*/

	/* Clean up */

/*
	if ( cdata->scrr_beg < 0 ) cdata->scrr_beg = 0;
*/
	if ( cdata->scrr_end >= vc->ychars ) cdata->scrr_end=vc->ychars-1;
	

	if (vc==vconsole_current)
		vc->SCROLLDOWN ( vc, cdata->scrr_beg, cdata->scrr_end );
	else
		vconsole_pending=vc->number;

	if (vnodeconsolebug) {
		if (ReadWord(0xf148a000) != vnodeconsolebug2) {
			log(LOG_WARNING, "vnode 0xf148a000 v_flag changed from %08x to %08x in do_scrolldown(1)\n",
			    vnodeconsolebug2, ReadWord(0xf148a000));
			log(LOG_WARNING, "vc=%08x vcur=%08x charmap=%08x\n", vc, vconsole_current, vc->charmap);
		}
	}

/*
 * This version of the following code was responcible for the vnode bug
 * It can trash part of the word that follows the allocated block
 * e.g. charmap = 0xf1484000, size 0x6000
 *      vnode = 0xf148a000
 *	first word of vnode is trashed.
 * bug is an adjustment of -1 for counting backwards was not made.
 
	if ( ((vc->flags)&(LOSSY)) == 0 ) {
		int counter;
		for ( counter=((cdata->scrr_end+1)*(vc->xchars)); 
			counter > (cdata->scrr_beg+1)*(vc->xchars) ; counter-- ) {
			vc->charmap[counter] = vc->charmap[counter-vc->xchars];
		}

		for ( counter=(cdata->scrr_beg)*(vc->xchars);
			counter < (cdata->scrr_beg+1)*(vc->xchars); counter++ ) {
			vc->charmap[counter]=0x20;
		}
	}
*/
	if ( ((vc->flags)&(LOSSY)) == 0 ) {
		int counter;
		for ( counter=((cdata->scrr_end+1)*(vc->xchars)) - 1; 
			counter >= (cdata->scrr_beg+1)*(vc->xchars) ; counter-- ) {
			vc->charmap[counter] = vc->charmap[counter-vc->xchars];
		}

		for ( counter=(cdata->scrr_beg)*(vc->xchars);
			counter < (cdata->scrr_beg+1)*(vc->xchars); counter++ ) {
			vc->charmap[counter]=0x20;
		}
	}
	if (vnodeconsolebug) {
		if (ReadWord(0xf148a000) != vnodeconsolebug2) {
			log(LOG_WARNING, "vnode 0xf148a000 v_flag changed from %08x to %08x in do_scrolldown(2)\n",
			    vnodeconsolebug2, ReadWord(0xf148a000));
			log(LOG_WARNING, "vc=%08x vcur=%08x charmap=%08x\n", vc, vconsole_current, vc->charmap);
		}
	}
}

void
do_scrollup(vc)
	struct vconsole *vc;
{
	struct vt220_info *cdata = (struct vt220_info *)vc->data;

	if (vc==vconsole_current)
		vc->SCROLLUP ( vc, cdata->scrr_beg, cdata->scrr_end );
	else
		vconsole_pending = vc->number;

	if ( cdata->scrr_end == 0 )
		vc->ycur=vc->ychars-1;
	else
		vc->ycur=cdata->scrr_end;

/* Do a cyclic buffer for this !!!!!!!!!!!!!! */	
	if ( ((vc->flags)&(LOSSY)) == 0 ) {
/* bcopy was weird, do this for now */
		int counter;
		for ( counter=(cdata->scrr_beg+1)*vc->xchars; 
		    counter < ((cdata->scrr_end+1)*(vc->xchars)); counter++ ) {
			vc->charmap[counter-vc->xchars] = vc->charmap[counter];
		}
		for ( counter=vc->xchars*(cdata->scrr_end);
		    counter < (vc->xchars*(cdata->scrr_end+1)); counter++ ) {
			vc->charmap[counter]=0x20;
		}
	}
}

void
respond(vc)
	struct vconsole *vc;
{
	struct vt220_info *cdata = (struct vt220_info *)vc->data;

        while (*cdata->report_chars && cdata->report_count > 0) {
                (*linesw[vc->tp->t_line].l_rint)
                        (*cdata->report_chars++ & 0xff, vc->tp);
                cdata->report_count--;
        }
}

void
vt_curadr(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;

#ifdef SELFTEST
strcpy ( console_proc, "vt_curadr" );
#endif

    if ( cdata->m_om )	/* relative to scrolling region */
    {
	cdata->param[0]+=cdata->scrr_beg;
	if ( (cdata->param[0]==0) && (cdata->param[1]==0) )
	{
		vc->xcur = 0;
		vc->ycur = 0;
		return;
	}
	/* Limit checking */
	cdata->param[0] = (cdata->param[0] <= 0) ? 1 : cdata->param[0];
	cdata->param[0] = (cdata->param[0] > vc->ychars) ? vc->ychars : cdata->param[0];
	cdata->param[1] = (cdata->param[1] <= 0) ? 1 : cdata->param[1];
	cdata->param[1] = (cdata->param[1] >= vc->xchars) ? (vc->xchars-1) : cdata->param[1];
	(cdata->param[0])--;
	(cdata->param[1])--;
	vc->ycur = cdata->param[0];
	vc->xcur = cdata->param[1];
    }
    else
    {
	if ( (cdata->param[0]==0) && (cdata->param[1]==0) )
	{
		vc->xcur = 0;
		vc->ycur = 0;
		return;
	}
	/* Limit checking */

	(cdata->param[0])--;
	(cdata->param[1])--;

	vc->ycur = cdata->param[0];
	vc->xcur = cdata->param[1];

{
	char buf[80];
	sprintf ( buf, "vc->xcur %d vc->ycur %d", vc->xcur, vc->ycur );
	dprintf ( buf );
}
    }
}

extern void beep_generate(void);

void
vt_reset_dec_priv_qm(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;
#ifdef SELFTEST
strcpy ( console_proc, "vt_reset_dec_priv_qm" );
#endif
    switch(cdata->param[0])
    {
        case 7:         /* AWM - auto wrap mode */
		beep_generate();
		cdata->flags &= ~F_AWM;
		break;
        case 0:         /* error, ignored */
        case 1:         /* CKM - cursor key mode */
        case 2:         /* ANM - ansi/vt52 mode */
        case 3:         /* COLM - column mode */
        case 4:         /* SCLM - scrolling mode */
        case 5:         /* SCNM - screen mode */
        case 8:         /* ARM - auto repeat mode */
        case 9:         /* INLM - interlace mode */
        case 10:        /* EDM - edit mode */
        case 11:        /* LTM - line transmit mode */
        case 12:        /* */
        case 13:        /* SCFDM - space compression / field delimiting */
        case 14:        /* TEM - transmit execution mode */
        case 15:        /* */
        case 16:        /* EKEM - edit key execution mode */
        case 25:        /* TCEM - text cursor enable mode */
        case 42:        /* NRCM - 7bit NRC characters */
            break;

        case 6:         /* OM - origin mode */
            cdata->m_om = 0;
            break;
       }
}

void
vt_sc(vc)
	struct vconsole *vc;
{
	struct vt220_info *cdata = (struct vt220_info *)vc->data;

#ifdef SELFTEST
	strcpy ( console_proc, "vt_sc" );
#endif

	cdata->sc_G0 	= cdata->G0;
	cdata->sc_G1 	= cdata->G1;
	cdata->sc_G2 	= cdata->G2;
	cdata->sc_G3 	= cdata->G3;
	cdata->sc_GL 	= cdata->GL;
	cdata->sc_GR 	= cdata->GR;
	cdata->sc_xcur 	= vc->xcur;
	cdata->sc_ycur 	= vc->ycur;
	cdata->sc_om 	= cdata->m_om;
	cdata->sflags	= cdata->flags;
/*
	cdata->sc_attr 	= cdata->c_attr;
	cdata->sc_awm 	= cdata->m_awm;
	cdata->sc_sel 	= cdata->selchar;
	cdata->sc_vtsgr = cdata->vtsgr;
	cdata->sc_flag 	= 1;
*/
}

void
vt_rc(vc)
	struct vconsole *vc;
{
	struct vt220_info *cdata = (struct vt220_info *)vc->data;
	cdata->G0 	= cdata->sc_G0;
	cdata->G1 	= cdata->sc_G1;
	cdata->G2 	= cdata->sc_G2;
	cdata->G3 	= cdata->sc_G3;
	cdata->GL 	= cdata->sc_GL;
	cdata->GR 	= cdata->sc_GR;
	vc->xcur 	= cdata->sc_xcur;
	vc->ycur 	= cdata->sc_ycur;
	cdata->m_om	= cdata->sc_om;
	cdata->flags = cdata->sflags;

/*
	cdata->c_attr 	= cdata->sc_attr;
	cdata->awm 	= cdata->sc_awm;
	cdata->sel 	= cdata->sc_selchar;
	cdata->vtsgr 	= cdata->sc_vtsgr;
	cdata->flag 	= 0;
*/
}

void
vt_clreol(vc)
	struct vconsole *vc;
{
/*
	struct vt220_info *cdata = (struct vt220_info *)vc->data;
*/
	int counter;
	int x = vc->xcur;
	int y = vc->ycur;

#ifdef SELFTEST
	strcpy ( console_proc, "vt_clreol" );
#endif

	for ( counter=vc->xcur; counter<vc->xchars; counter++ )
		do_render_noscroll ( ' ', vc );

	vc->xcur = x;
	vc->ycur = y;
}

/* index, move cursor down */

void
vt_ind(vc)
	struct vconsole *vc;
{
	struct vt220_info *cdata = (struct vt220_info *)vc->data;

#ifdef SELFTEST
	strcpy ( console_proc, "vt_ind" );
#endif

	vc->ycur++;

	{
		char buf[80];
		sprintf ( buf, "{vt_ind [%d,%d] [%d,%d] }",
			vc->xcur, vc->ycur, cdata->scrr_beg, cdata->scrr_end);
		dprintf ( buf );
	}

	do_scrollcheck ( vc );
}

void
vt_nel(vc)
	struct vconsole *vc;
{
	vc->ycur++;
	do_scrollcheck ( vc );
	vc->xcur=0;
}

void
vt_ri(vc)
	struct vconsole *vc;
{
        struct vt220_info *cdata = (struct vt220_info *)vc->data;

	if (vnodeconsolebug & 1)
		vnodeconsolebug2 = ReadWord(0xf148a000);

	vc->ycur--;

        if (vc->ycur<=cdata->scrr_beg)
		vc->ycur = cdata->scrr_beg;


	if (vc->ycur <= cdata->scrr_beg) {
		if (vnodeconsolebug & 4) {
			if (ReadWord(0xf148a000) != vnodeconsolebug2) {
				log(LOG_WARNING, "vnode 0xf148a000 v_flag changed from %08x to %08x in vt_ri(1)\n",
				    vnodeconsolebug2, ReadWord(0xf148a000));
				log(LOG_WARNING, "vc=%08x ycur=%d scrr_beg=%d\n", vc, vc->ycur, cdata->scrr_beg);
				vnodeconsolebug2 = ReadWord(0xf148a000);
			}
		}
		do_scrolldown ( vc );
		if (vnodeconsolebug & 4) {
			if (ReadWord(0xf148a000) != vnodeconsolebug2) {
				log(LOG_WARNING, "vnode 0xf148a000 v_flag changed from %08x to %08x in vt_ri(2)\n",
				    vnodeconsolebug2, ReadWord(0xf148a000));
				log(LOG_WARNING, "vc=%08x ycur=%d scrr_beg=%d charmap=%08x\n", vc, vc->ycur, cdata->scrr_beg, vc->charmap);
				vnodeconsolebug2 = ReadWord(0xf148a000);
			}
		}
		vc->ycur = cdata->scrr_beg;
	}
}

/* selective in line erase */

int
vt_sel(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;
    register int counter;
    register int x = vc->xcur;
    register int y = vc->ycur;

    switch ( cdata->param[0] )
    {
	case 0:		/* Delete to the end of the line */
	    for ( counter=x; counter<vc->xchars; counter++ )
		do_render_noscroll ( ' ', vc );
	    break;
		
	case 1:		/* Delete to the beginning of the line */
	    vc->xcur = 0;
	    for ( counter=0; counter<x; counter++ )
		do_render_noscroll ( ' ', vc );
	    break;

	case 2:		/* Delete the whole line */
	default:
	    vc->xcur = 0;
 	    for ( counter=0; counter<vc->xchars; counter++ )
		do_render_noscroll ( ' ', vc );
	    break;
	  }

    vc->xcur = x;
    vc->ycur = y;
    return 0;
}

void
vt_cuu(vc)
	struct vconsole *vc;
{
        struct vt220_info *cdata = (struct vt220_info *)vc->data;
	cdata->param[0] = (cdata->param[0] <= 0) ? 1 : cdata->param[0];
	vc->ycur -= cdata->param[0];
	vc->ycur = vc->ycur < 0 ? 0 : vc->ycur;
}

void
vt_cub(vc)
	struct vconsole *vc;
{
        struct vt220_info *cdata = (struct vt220_info *)vc->data;

	cdata->param[0] = (cdata->param[0] <= 0) ? 1 : cdata->param[0];

	vc->xcur -= cdata->param[0];
	cdata->param[0] = (cdata->param[0] < 0 ) ? 0 : cdata->param[0];
}

void
vt_da(vc)
	struct vconsole *vc;
{
        struct vt220_info *cdata = (struct vt220_info *)vc->data;
        static u_char *response = (u_char *)DA_VT220;

        cdata->report_chars = response;
        cdata->report_count = 18;
        respond(vc);
}

void
vt_str(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;
    int counter;

    if (cdata == NULL) {
    	return;
    }

    clr_params ( cdata );

    cdata->state = STATE_INIT;
    cdata->disable_function = 1;
    cdata->m_om = 0; /* origin mode */
    vc->xcur = vc->ycur = 0;
    cdata->beepoff=default_beepstate;
    cdata->simple_cursor_store = ' ';
    cdata->scrr_beg = 0;
    cdata->scrr_len = vc->ychars;
    cdata->scrr_end = vc->ychars-1;
    cdata->simple_cursor_on = 0;
    cdata->nfgcol = 7;
    cdata->nbgcol = 0;
    cdata->fgcol = cdata->nfgcol;
    cdata->bgcol = cdata->nbgcol;
    cdata->attribs=cdata->fgcol<<8 | cdata->bgcol<<11;
    cdata->sc_flag = 0;                      /* save cursor position */
    cdata->flags = F_AWM;
    cdata->irm = 0;

    for ( counter=0; counter<MAXTABSTOPS; counter++ )
    {
        if ( !(counter%8) )
                cdata->tab_stops[counter] = 1;
        else
                cdata->tab_stops[counter] = 0;
    }
}

void
vt_ris(vc)
	struct vconsole *vc;
{
	vc->xcur = vc->ycur = 0;
        vt_str(vc);                   /* and soft terminal reset */
}

void
vt_cud(vc)
	struct vconsole *vc;
{
        struct vt220_info *cdata = (struct vt220_info *)vc->data;

	cdata->param[0] = (cdata->param[0] <= 0) ? 1 : cdata->param[0];

	cdata->param[0] = (cdata->param[0] > (cdata->scrr_end-vc->ycur))
			? (cdata->scrr_end-vc->ycur) : cdata->param[0];

	vc->ycur += cdata->param[0];

	do_scrollcheck ( vc );
}

void
vt_tst(vc)
	struct vconsole *vc;
{
    int counter, times;

    for ( times=0; times<100; times++ );
	for ( counter=32; counter<127; counter++ )
		do_render ( counter, vc );
}

void
vt_il(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;
    register int beg, end;

    if ( vc->ycur >= (vc->ychars-2) )
	return;

    cdata->param[0] = cdata->param[0]<=0 ? 1 : cdata->param[0];

    beg = cdata->scrr_beg;
    end = cdata->scrr_end;

    cdata->scrr_beg = vc->ycur;
    cdata->scrr_end = vc->ychars-1;

    for ( ; cdata->param[0]>0; cdata->param[0]-- )
        do_scrolldown( vc );

    cdata->scrr_beg = beg;
    cdata->scrr_end = end;
}

void
vt_ic(vc)
	struct vconsole *vc;
{
    int counter;
    int ox, oy;
    int *ptr = vc->charmap + (vc->xcur + vc->ycur*vc->xchars);
    for ( counter=vc->xchars-vc->xcur; counter>=0; counter-- )
        ptr[counter+1] = ptr[counter];
    ptr[0] =  ' ';
    ox = vc->xcur; oy = vc->ycur;

    for ( ; vc->xcur < vc->xchars; )
        do_render_noscroll ( vc->charmap[vc->xcur+vc->ycur*vc->xchars], vc );
    vc->xcur = ox; vc->ycur = oy;
}

void
vt_dch(vc)
	struct vconsole *vc;
{
    int counter;
    int ox, oy;
    int *ptr = vc->charmap + (vc->ycur*vc->xchars);

    for ( counter=vc->xcur; counter<(vc->xchars-1); counter++ )
        ptr[counter] = ptr[counter+1];

    ptr[vc->xchars] =  ' ';

    ox = vc->xcur; oy = vc->ycur;

    for ( ; vc->xcur < vc->xchars; )
        do_render_noscroll ( vc->charmap[vc->xcur+vc->ycur*vc->xchars], vc );

    vc->xcur = ox; vc->ycur = oy;
}

void
vt_dl(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;
    register int beg, end;

    cdata->param[0] = cdata->param[0]<=0 ? 1 : cdata->param[0];
    cdata->param[0] = cdata->param[0]>vc->xchars ? vc->xchars : cdata->param[0];

/*
    vc->xcur=0;
*/

    beg = cdata->scrr_beg;
    end = cdata->scrr_end;

    cdata->scrr_beg = vc->ycur;
    cdata->scrr_end = vc->ychars-1;

    for ( ; cdata->param[0]>0; cdata->param[0]-- )
        do_scrollup( vc );

    cdata->scrr_beg = beg;
    cdata->scrr_end = end;
}

/* Set scrolling region */

void
vt_stbm(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;

    if((cdata->param[0] == 0) && (cdata->param[1] == 0))
    {
        cdata->xcur = 0;
        cdata->ycur = 0;
        cdata->scrr_beg = 0;
        cdata->scrr_len = vc->ychars;
        cdata->scrr_end = cdata->scrr_len - 1;
        return;
    }
    if(cdata->param[1] <= cdata->param[0])
            return;

    /* range parm 1 */

    cdata->scrr_beg = cdata->param[0]-1;
    cdata->scrr_len = cdata->param[1] - cdata->param[0] + 1;
    cdata->scrr_end = cdata->param[1]-1;

/*
    if (cdata->scrr_beg<0)
	cdata->scrr_beg = 0;
*/

    if (cdata->scrr_end>(vc->ychars-1))
	cdata->scrr_end = vc->ychars-1;

    cdata->scrr_len = cdata->scrr_end - cdata->scrr_beg - 1;
{
	char buf[80];
	sprintf ( buf, "scrr_beg %d, scrr_end %d", cdata->scrr_beg,
				cdata->scrr_end );
	dprintf ( buf );
}

    cdata->flags &= ~F_LASTCHAR;
}

void
vt_dsr(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;
    static u_char *answr = (u_char *)"\033[0n";
    static u_char *panswr = (u_char *)"\033[?13n"; /* Printer Unattached */
    static u_char *udkanswr = (u_char *)"\033[?21n"; /* UDK Locked */
    static u_char *langanswr = (u_char *)"\033[?27;1n"; /* North American*/
    static u_char buffer[16];
    int i = 0;

    switch(cdata->param[0])
    {
            case 5:         /* return status */
                    cdata->report_chars = answr;
                    cdata->report_count = 4;
                    respond(vc);
                    break;

            case 6:         /* return cursor position */
                    buffer[i++] = 0x1b;
                    buffer[i++] = '[';
                    if((vc->ycur+1) > 10)
                            buffer[i++] = ((vc->ycur+1) / 10) + '0';
                    buffer[i++] = ((vc->ycur+1) % 10) + '0';
                    buffer[i++] = ';';
                    if((vc->xcur+1) > 10)
                            buffer[i++] = ((cdata->xcur+1) / 10) + '0';
                    buffer[i++] = ((vc->xcur+1) % 10) + '0';
                    buffer[i++] = 'R';
                    buffer[i++] = '\0';

                    cdata->report_chars = buffer;
                    cdata->report_count = i;
                    respond(vc);
                    break;

            case 15:        /* return printer status */
                    cdata->report_chars = panswr;
                    cdata->report_count = 6;
                    respond(vc);
                    break;

            case 25:        /* return udk status */
                    cdata->report_chars = udkanswr;
                    cdata->report_count = 6;
                    respond(vc);
                    break;

            case 26:        /* return language status */
                    cdata->report_chars = langanswr;
                    cdata->report_count = 8;
                    respond(vc);
                    break;

            default:        /* nothing else valid */
                    break;
    }
}

void
vt_su(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;

    cdata->param[0] = cdata->param[0]<=0 ? 1 : cdata->param[0];
    cdata->param[0] = cdata->param[0]>(vc->xchars-1) ? vc->xchars-1 : cdata->param[0];

    do_scrollup ( vc );
}

void
vt_set_dec_priv_qm(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;

    switch(cdata->param[0])
    {
        case 7:         /* AWM - auto wrap mode */
		cdata->flags |= F_AWM;
		break;

	/* Implement these */
        case 1:         /* CKM - cursor key mode */
        case 3:         /* COLM - column mode */
        case 6:         /* OM - origin mode */
        case 8:         /* ARM - auto repeat mode */
	case 25:        /* TCEM - text cursor enable mode */
                break;

        case 0:         /* error, ignored */
        case 2:         /* ANM - ansi/vt52 mode */
        case 4:         /* SCLM - scrolling mode */
        case 5:         /* SCNM - screen mode */
        case 9:         /* INLM - interlace mode */
        case 10:        /* EDM - edit mode */
        case 11:        /* LTM - line transmit mode */
        case 12:        /* */
        case 13:        /* SCFDM - space compression / field delimiting */
        case 14:        /* TEM - transmit execution mode */
        case 15:        /* */
        case 16:        /* EKEM - edit key execution mode */
        case 42:        /* NRCM - 7bit NRC characters */
                break;
    }
}

void
vt_keyappl(vc)
	struct vconsole *vc;
{
	dprintf ( "VT_KEYAPPL" );
}

void
vt_clrtab(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;
    int i;

    if(cdata->param[0] == 0)
            cdata->tab_stops[vc->xcur] = 0;
    else if(cdata->param[0] == 3)
    {
            for(i=0; i<MAXTABSTOPS; i++)
                    cdata->tab_stops[i] = 0;
    }
}

/*
void
vt_cuf(vc)
	struct vconsole *vc;
{
        struct vt220_info *cdata = (struct vt220_info *)vc->data;

	cdata->param[0] = (cdata->param[0] <= 0) ? 1 : cdata->param[0];

	vc->xcur += cdata->param[0];

	cdata->param[0] = (cdata->param[0] > vc->xchars ) ? vc->xchars : cdata->param[0];
}
*/

void
vt_cuf(vc)
	struct vconsole *vc;
{
        struct vt220_info *cdata = (struct vt220_info *)vc->data;
        register int p = cdata->param[0];

        if(vc->xcur == ((vc->xchars)-1))         /* already at right margin */
                return;
        if(p <= 0)                      /* parameter min = 1 */
                p = 1;
        else if(p > ((vc->xchars)-1))         /* parameter max = 79 */
                p = ((vc->xchars)-1);
        if((vc->xcur + p) > ((vc->xchars)-1))                /* not more than */
               p = ((vc->xchars)-1) - vc->xcur;
        vc->xcur += p;
}

void
vt_sgr(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;
    register int i=0;
    
    do
    {
	vc->SGR ( vc, cdata->param[i] );
        switch ( cdata->param[i++] )
        {
            case 0:	/* reset to normal attributes */
		cdata->fgcol = cdata->nfgcol;
		cdata->bgcol = cdata->nbgcol;
    cdata->attribs=cdata->fgcol<<8 | cdata->bgcol<<11;
		break;
            case 1:	/* bold */
		cdata->attribs |= BOLD;
		break;
            case 4:	/* underline */
		cdata->attribs |= UNDERLINE;
		break;
            case 5:	/* blinking */
		cdata->attribs |= BLINKING;
		break;
            case 7:	/* reverse */
		cdata->fgcol = cdata->nbgcol;
		cdata->bgcol = cdata->nfgcol;
		cdata->attribs |= REVERSE;
    cdata->attribs&=~0x3F00;
    cdata->attribs|=cdata->fgcol<<8 | cdata->bgcol<<11;
		break;
            case 22:	/* not bold */
		cdata->attribs &= ~BOLD;
		break;
            case 24:	/* not underlined */
		cdata->attribs &= ~UNDERLINE;
		break;
            case 25:	/* not blinking */
		cdata->attribs &= ~BLINKING;
		break;
            case 27:	/* not reverse */
		cdata->attribs &= ~REVERSE;
		cdata->fgcol = cdata->nfgcol;
		cdata->bgcol = cdata->nbgcol;
    cdata->attribs&=~0x3F00;
    cdata->attribs|=cdata->fgcol<<8 | cdata->bgcol<<11;
            	break;
            	
	    default:
	    	if ( ( cdata->param[i-1] > 29 )&&( cdata->param[i-1] < 38 ) )
		{
	   		vc->SETFGCOL ( vc, cdata->param[i-1] - 30 );
			cdata->fgcol = cdata->param[i-1] - 30;
    cdata->attribs&=~0x3F00;
    cdata->attribs|=cdata->fgcol<<8 | cdata->bgcol<<11;

		}
	    	if ( ( cdata->param[i-1] > 39 )&&( cdata->param[i-1] < 48 ) )
		{
	    		vc->SETBGCOL ( vc, cdata->param[i-1] - 40 );
			cdata->bgcol = cdata->param[i-1] - 40;
    cdata->attribs&=~0x3F00;
    cdata->attribs|=cdata->fgcol<<8 | cdata->bgcol<<11;
		}
	}
    } while ( i<=cdata->parami );
}

void
vt_clreos(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;
    int ptr;
    if ( vc == vconsole_current )
        vc->R_CLREOS ( vc, cdata->param[0] );
    else
	vconsole_pending = vc->number;

    switch ( cdata->param[0] )
    {
	case 0:	/* Erase from cursor to end of screen */
		for ( ptr=vc->xcur + vc->ycur * vc->xchars
				; ptr<(vc->xchars*vc->ychars); ptr++ )
			vc->charmap[ptr]=0x20;
		break;

	case 1: /* Erase from start to cursor */
		for (   ptr=0;
			ptr<vc->ycur*vc->xchars + vc->xcur;
			ptr++ )
			vc->charmap[ptr]=0x20;
		break;

	case 2: /* Blitz the whole bloody thing */
		if ((vc->flags&LOSSY)==0)
			mapped_cls(vc);
		if (vc==vconsole_current)
		     vc->CLS(vc);
		else
		     vconsole_pending = vc->number;
		break;
    }
}

void
vt_set_ansi(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;

    switch(cdata->param[0])
    {
            case 0:         /* error, ignored */
            case 1:         /* GATM - guarded area transfer mode */
            case 2:         /* KAM - keyboard action mode */
            case 3:         /* CRM - Control Representation mode */
            case 20:        /* LNM - line feed / newline mode */
                    break;

            case 4:         /* IRM - insert replacement mode */
		cdata->irm = 1;
		break;

            case 5:         /* SRTM - status report transfer mode */
            case 6:         /* ERM - erasue mode */
            case 7:         /* VEM - vertical editing mode */
            case 10:        /* HEM - horizontal editing mode */
            case 11:        /* PUM - position unit mode */
            case 12:        /* SRM - send-receive mode */
            case 13:        /* FEAM - format effector action mode */
            case 14:        /* FETM - format effector transfer mode */
            case 15:        /* MATM - multiple area transfer mode */
            case 16:        /* TTM - transfer termination */
            case 17:        /* SATM - selected area transfer mode */
            case 18:        /* TSM - tabulation stop mode */
            case 19:        /* EBM - editing boundary mode */
                    break;
    }
}

void
vt_reset_ansi(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;

    switch(cdata->param[0])
    {
	    /* Implement these */
            case 0:         /* error, ignored */
            case 1:         /* GATM - guarded area transfer mode */
            case 2:         /* KAM - keyboard action mode */
            case 3:         /* CRM - Control Representation mode */
            case 20:        /* LNM - line feed / newline mode */
                    break;

            case 4:         /* IRM - insert replacement mode */
		cdata->irm = 0;
		break;

            case 5:         /* SRTM - status report transfer mode */
            case 6:         /* ERM - erasue mode */
            case 7:         /* VEM - vertical editing mode */
            case 10:        /* HEM - horizontal editing mode */
            case 11:        /* PUM - position unit mode */
            case 12:        /* SRM - send-receive mode */
            case 13:        /* FEAM - format effector action mode */
            case 14:        /* FETM - format effector transfer mode */
            case 15:        /* MATM - multiple area transfer mode */
            case 16:        /* TTM - transfer termination */
            case 17:        /* SATM - selected area transfer mode */
            case 18:        /* TSM - tabulation stop mode */
            case 19:        /* EBM - editing boundary mode */
                    break;
    }
}

/* DRN */

void
do_render_noscroll(c, vc)
	char c;
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;
    /* THE RENDER STAGE **********************************/

    if ((c>=0x20)&&(c<=0x7f))
    {
	if (((vc->flags)&(LOSSY))==0)
	{
          if ( vc->charmap[vc->xcur+vc->ycur*vc->xchars] != c|cdata->attribs )
          {
	    if ( vc==vconsole_current )
	        vc->RENDER ( vc, c );
	    else
	        vconsole_pending = vc->number;
	  }
	  vc->charmap[ vc->xcur + vc->ycur*vc->xchars ] = c|cdata->attribs;
	}
 	else
	{
	    if ( vc==vconsole_current )
	        vc->RENDER ( vc, c );
	    else
	        vconsole_pending = vc->number;
        }
    }

    vc->xcur++;

    /*do_scrollcheck ( vc );*/
}

#ifdef SIMPLE_CURSOR
void
simple_cursor_on(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;
    if (cdata->simple_cursor_on)
	return 0;
    if (vc!=vconsole_current)
	return 0;
    if (((vc->flags)&(LOSSY))==0)
    {
	cdata->simple_cursor_store = vc->charmap[ vc->xcur + vc->ycur*vc->xchars ];
	vc->RENDER ( vc, SIMPLE_CURSOR_CHAR );
    }
    cdata->simple_cursor_on = 1;
}

void
simple_cursor_off(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;
     if (!cdata->simple_cursor_on)
	return 0;
   if (vc!=vconsole_current)
	return 0;
    if (((vc->flags)&(LOSSY))==0)
    {
	vc->RENDER ( vc, cdata->simple_cursor_store );
    }
    cdata->simple_cursor_on = 0;
}
#endif

/* DSC */
void
do_scrollcheck(vc)
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;

    /* BOUNDARY CHECK ************************************/
    if ((vc->xcur >= (vc->xchars))&&((cdata->flags&F_AWM)!=0))
    {
	cdata->flags|=F_LASTCHAR;
	cdata->lastpos = vc->ycur*vc->xchars+vc->xcur;
    }

    /* SCROLL CHECK *************************************/
    if ( vc->ycur >= cdata->scrr_end+1 )
    {
	do_scrollup ( vc );
    }
}

/* DR */

int
do_render(c, vc)
	char c;
	struct vconsole *vc;
{
    struct vt220_info *cdata = (struct vt220_info *)vc->data;
    /* THE RENDER STAGE **********************************/

    if (((cdata->flags&F_AWM)==0)&&(vc->xcur >= 20))
	return 0;

    if ( cdata->flags & F_LASTCHAR )
    {
	if ( cdata->lastpos==vc->ycur*vc->xchars+vc->xchars)
	{
	    vc->ycur++;
	    vc->xcur = 0;
	    cdata->flags &= ~F_LASTCHAR;
	    do_scrollcheck(vc);
	}
	  else
	{
	    cdata->flags &= ~F_LASTCHAR;
	}
    }

    if ((c>=0x20)&&(c<=0x7f))
    {
	if (((vc->flags)&(LOSSY))==0)
	{
	    if ( cdata->irm == 0 )
	    {
                if(vc->charmap[vc->xcur+vc->ycur*vc->xchars]!=c|cdata->attribs )
                {
	            if ( vc==vconsole_current )
	                vc->RENDER ( vc, c );
	            else
	                vconsole_pending = vc->number;
	        }
	        vc->charmap[vc->xcur+vc->ycur*vc->xchars ] = c|cdata->attribs;
            }
	    else
	    {
	        int counter;
		int ox, oy;
	        int *ptr = vc->charmap + (vc->xcur + vc->ycur*vc->xchars);
	        for ( counter=vc->xchars-vc->xcur; counter>=0; counter-- )
		    ptr[counter+1] = ptr[counter];
	        ptr[0] = c;
		ox = vc->xcur; oy = vc->ycur;

		for ( ; vc->xcur < vc->xchars; )
	            do_render_noscroll ( vc->charmap[vc->xcur+vc->ycur*vc->xchars], vc );
		vc->xcur = ox; vc->ycur = oy;
	
	    }
	}
 	else
        {
            if ( vc==vconsole_current )
	        vc->RENDER ( vc, c );
	    else
	        vconsole_pending = vc->number;
        }
    }

    vc->xcur++;

    do_scrollcheck ( vc );

    if ( cdata->flags & F_LASTCHAR )
	vc->xcur--;
    return 0;
}

int
TERMTYPE_PUTSTRING(string, length, vc)
	char *string;
	int length;
	struct vconsole *vc;
{
    struct vt220_info *cdata;
    register char c;
    char dc[] = "x\0";
    cdata = (struct vt220_info *)vc->data;

    /* A piece of saftey code */
    if ( vconsole_current->vtty == 0 )
	return 0;

#ifdef SIMPLE_CURSOR
    if ( vc==vconsole_current )
        if ( vc->CURSORUPDATE(vc)==-1 ) simple_cursor_off ( vc );
#else
    if ( vc==vconsole_current )
	vc->CURSORUPDATE (vc);
#endif

    while ( ((c=*(string++))!=0) && ((length--)>0) )
    { 

    if ( cdata->state != STATE_INIT )
	cdata->flags &= ~F_LASTCHAR;

    if ( ( c == 0x0a ) || ( c== 0x0d ) )
	cdata->flags &= ~F_LASTCHAR;

/* Middle mouse button freezes the display while active */

        while ((ReadByte(IO_MOUSE_BUTTONS) & MOUSE_BUTTON_MIDDLE) == 0);

/* Left mouse button slows down the display speed */

        if ((ReadByte(IO_MOUSE_BUTTONS) & MOUSE_BUTTON_LEFT) == 0)
          delay(5000);

/* Always process characters in the range of 0x00 to 0x1f */


#ifdef DEBUGTERM
*dc = c;
switch (c)
{
	case 0x0a: 	dprintf ( "[0a]" ); break;
	case 0x0d: 	dprintf ( "[0d]" ); break;
	case 0x0c: 	dprintf ( "[0c]" ); break;
	case 0x1b:	dprintf ( "[1b]" ); break;
}
#endif
	if ( c <= 0x1f )
      	{
		if ( cdata->disable_function ) 
		{
			if ( cdata->flags & F_LASTCHAR )
			{
		          if ( cdata->lastpos==vc->ycur*vc->xchars+vc->xchars)
			  {
			    vc->ycur++;
			    vc->xcur = 0;
			    cdata->flags &= ~F_LASTCHAR;
			    do_scrollcheck(vc);
			  }
			  else
			  {
			    cdata->flags &= ~F_LASTCHAR;
			  }
			}

		    	switch (c)
	    	    	{
				case 0x00:	/* NUL */
				case 0x01:	/* SOH */
				case 0x02:	/* STX */
				case 0x03:	/* ETX */
				case 0x04:	/* EOT */
				case 0x05:	/* ENQ */
				case 0x06:	/* ACK */
					break;

				case 0x07:	/* BEL */
					beep_generate();
					if ( !cdata->beepoff )
						c = 'G';
					break;

				case 0x08:	/* BS */
	cdata->flags &= ~F_LASTCHAR;
					if ( vc->xcur>0 )
						vc->xcur--;
					break;

				case 0x09:	/* TAB */
					while ( vc->xcur < vc->xchars-1 )
					{
						vc->xcur++;
						if (cdata->tab_stops[vc->xcur])
						    break;
					}
					break;

				case 0x0a:
	cdata->flags &= ~F_LASTCHAR;
					vc->ycur++;
						do_scrollcheck ( vc );
					break;

				case 0x0c:
	cdata->flags &= ~F_LASTCHAR;
					if ((vc->flags&LOSSY)==0)
						mapped_cls(vc);
					if (vc==vconsole_current)
					     vc->CLS(vc);
					vc->xcur=0;
					vc->ycur=0;

					break;
				case 0x0d:
	cdata->flags &= ~F_LASTCHAR;
					vc->xcur=0;
					break;
		
				case 0x10:	/* DLE */
				case 0x11:	/* DC1/XON */
				case 0x12:	/* DC2 */
				case 0x13:	/* DC3/XOFF */
				case 0x14:	/* DC4 */
				case 0x15:	/* NAK */
				case 0x16:	/* SYN */
				case 0x17:	/* ETB */
					break;

				case 0x18:	/* CAN */
					cdata->state = STATE_INIT;
					clr_params(cdata);
					break;
				
				case 0x19:	/* EM */
					break;

				case 0x1a:	/* SUB */
					cdata->state = STATE_INIT;
					clr_params(cdata);
					break;

				case 0x1b:	/* ESC */
					cdata->state = STATE_ESC;
					clr_params(cdata);
					break;

				case 0x1c:	/* FS */
				case 0x1d:	/* GS */
				case 0x1e:	/* RS */
				case 0x1f:	/* US */
					break;
			}	
		}
	}
      	else
	{
		/* 0x20 to 0xff depends on current state */
		switch ( cdata->state )
		{
			case STATE_INIT:
			    do_render ( c, vc );
			    break;

			case STATE_ESC:
#ifdef DEBUGTERM
{
    char buf[]="x";
    buf[0] = c;
    dprintf ( buf );
}
#endif
			    switch (c)
			    {
				case '7':	/* SAVE CUSOR */
				    vt_sc ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case '8':	/* RESTORE CUSOR */
				    vt_rc ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case '=':
				    vt_keyappl ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case '>':	/* Keypad numeric mode */
#ifdef DEBUGTERM
dprintf ( "\r\nKEYPAD NUMERIC MODE\r\n ");
#endif
				    cdata->state = STATE_INIT;
				    break;

				case 'D':
				    vt_ind ( vc );
				    cdata->state = STATE_INIT;
				    break;				

				case 'E':
				    vt_nel ( vc );
				    cdata->state = STATE_INIT;
				    break;				

				case 'H':
				    cdata->tab_stops[vc->xcur] = 1;
				    cdata->state = STATE_INIT;
				    break;

				case 'M':
				    vt_ri ( vc );
				    cdata->state = STATE_INIT;
				    break;				

				case 'N':
				    cdata->Gs = cdata->G2;
				    cdata->ss = 1;
				    cdata->state = STATE_INIT;
				    break;				

				case 'O':
				    cdata->Gs = cdata->G3;
				    cdata->ss = 1;
				    cdata->state = STATE_INIT;
				    break;				

				case 'P':
				    cdata->dcs_state = DCS_INIT;
				    cdata->state = STATE_DCS;
				    break;

				case 'Z':
				    vt_da ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case '~':
				    cdata->GR = cdata->G1;
				    cdata->state = STATE_INIT;
				    break;				

				case '[':
				    clr_params ( cdata );
				    cdata->state = STATE_CSI;
				    break;				

				case '\\':
				    cdata->state = STATE_INIT;
				    break;				

				case 'c':
				    vt_ris ( vc );
				    cdata->state = STATE_CSI;
				    break;				

				case 'n':
				    cdata->GL = cdata->G2;
				    cdata->state = STATE_INIT;
				    break;				

				case 'o':
				    cdata->GL = cdata->G3;
				    cdata->state = STATE_INIT;
				    break;				

				case '}':
				    cdata->GR = cdata->G2;
				    cdata->state = STATE_INIT;
				    break;				

				case '|':
				    cdata->GR = cdata->G3;
				    cdata->state = STATE_INIT;
				    break;				

				default:
				    do_render ( c, vc );
				    cdata->state = STATE_INIT;
				    break;
			    }
			    break;

			case STATE_CSIQM:
#ifdef DEBUGTERM
{
    char buf[]="x";
    buf[0] = c;
    dprintf ( buf );
}
#endif
			    switch (c)
			    {
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':	/* parameters */
				    cdata->param[cdata->parami] *= 10;
				    cdata->param[cdata->parami] += (c-'0');
				    break;

				case ';':	/* next parameter */
				    cdata->parami =
					(cdata->parami+1 < MAXPARMS) ?
					cdata->parami+1 : cdata->parami;
				    break;

				case 'h':
				    vt_set_dec_priv_qm ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'l':
				    vt_reset_dec_priv_qm ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'n':
				    vt_dsr ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'K':
				    vt_sel ( vc );
				    cdata->state = STATE_INIT;
				    break;

				default:
				    do_render ( '[', vc );
				    do_render ( c, vc );
				    cdata->state = STATE_INIT;
	{
		register int counter;
		for ( counter=0; counter<=cdata->parami; counter++ )
		{
			do_render ( '@', vc );
		}
	}
				    do_render ( c, vc );
				    break;
			    }

			case STATE_CSI:
#ifdef DEBUGTERM
{
    char buf[]="x";
    buf[0] = c;
    dprintf ( buf );
}
#endif
			    switch (c)
			    {
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':	/* parameters */
				    cdata->param[cdata->parami] *= 10;
				    cdata->param[cdata->parami] += (c-'0');
				    break;

				case ';':	/* next parameter */
				    cdata->parami =
					(cdata->parami+1 < MAXPARMS) ?
					cdata->parami+1 : cdata->parami;
				    break;

				case '?':	/* ESC [ ? family */
				    cdata->state = STATE_CSIQM;
				    break;

				case '@':	/* insert char */
				    vt_ic ( vc );
				    cdata->state = STATE_INIT;
				    break;	

				/*
				case '!':
				    cdata->state = STATE_STR;
				    break;	
				*/

				case 'A':	/* cursor up */
			 	    vt_cuu ( vc );
				    cdata->state = STATE_INIT;
				    break;	

				case 'B':	/* cursor down */
				    vt_cud ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'C':
				    vt_cuf ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'D':	/* cursor back */
				    vt_cub ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'H':	/* direct cursor addressing */
				    vt_curadr ( vc );
				    cdata->state = STATE_INIT;
				    break;
				
				case 'J': 	/* erase screen */
				    vt_clreos ( vc );
				    cdata->state = STATE_INIT;
				    break;

				    cdata->state = STATE_INIT;
				    break;

				case 'K':	/* erase line */
				    vt_clreol ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'L': 	/* insert line */
				    vt_il ( vc );
				    cdata->state = STATE_INIT;
				    break;

 				case 'M':	/* delete line */
				    vt_dl ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'P':	/* Delete chars */
				    vt_dch ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'S':	/* scroll up */
				    vt_su ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'c':
				    vt_da ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'f':
				    vt_curadr ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'h':
				    vt_set_ansi ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'l':
				    vt_reset_ansi ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'm':	/* select graphic rendition */
				    vt_sgr( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'n':
				    vt_dsr ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'r':	/* set scrolling region */
				    vt_stbm ( vc );
				    cdata->state = STATE_INIT;
				    break;

				case 'y':
				    vt_tst ( vc );
				    cdata->state = STATE_INIT;
				    break;

				default:
				    do_render ( '[', vc );
				    do_render ( c, vc );
				    cdata->state = STATE_INIT;
	{
		register int counter;
		for ( counter=0; counter<=cdata->parami; counter++ )
		{
			do_render ( '@', vc );
		}
	}
				    do_render ( c, vc );
				    cdata->state = STATE_INIT;
				    break;
	   		     } 
			    break;

			default:
			    cdata->state = STATE_INIT;
			    break;
		}
    	}
  }
#ifdef SIMPLE_CURSOR
	if ( vc==vconsole_current )
		if ( vc->CURSORUPDATE(vc)==-1 )
			simple_cursor_on ( vc );
#else
	if ( vc==vconsole_current )
		vc->CURSORUPDATE (vc);
#endif
	return 0;
}

void
console_debug()
{
}

int
vt220_swapin(vc)
	struct vconsole *vc;
{
#ifdef SIMPLE_CURSOR
	if ( vc==vconsole_current )
		if ( vc->CURSORUPDATE(vc)==-1 ) simple_cursor_on ( vc );
#else
	if ( vc==vconsole_current )
		vc->CURSORUPDATE (vc);
#endif
	return 0;
}

int
vt220_swapout(vc)
	struct vconsole *vc;
{
	return 0;
}


int
vt220_sleep(vc)
	struct vconsole *vc;
{
#ifdef SIMPLE_CURSOR
	if ( vc==vconsole_current )
		if ( vc->CURSORUPDATE(vc)==-1 ) simple_cursor_off ( vc );
#else
	if ( vc==vconsole_current )
		vc->CURSORUPDATE (vc);
#endif
	vc->FLASH        ( vc, 0 );
	vc->CURSOR_FLASH ( vc, 0 );
	return 0;
}

int
vt220_wake(vc)
	struct vconsole *vc;
{
	vc->FLASH        ( vc, 1 );
	vc->CURSOR_FLASH ( vc, 1 );

#ifdef SIMPLE_CURSOR
	if ( vc==vconsole_current )
		if ( vc->CURSORUPDATE(vc)==-1 )
			simple_cursor_on ( vc );
#else
	if ( vc==vconsole_current )
		vc->CURSORUPDATE (vc);
#endif
	return 0;
}

int
vt220_scrollback(vc)
	struct vconsole *vc;
{
	return -1;
}

int
vt220_scrollforward(vc)
	struct vconsole *vc;
{
	return -1;
}

int
vt220_scrollbackend(vc)
	struct vconsole *vc;
{
	return -1;
}

int
vt220_debugprint(vc)
	struct vconsole *vc;
{
	printf ( "VT220 TERMINAL EMULATOR\n\n" );
	printf ( "no information\n" );
	printf ( "\n" );
	return 0;
}

int
vt220_modechange(vc)
	struct vconsole *vc;
{
	if (vc->number >= 64)
		return(0);

	if (vc == NULL) {
		return(EINVAL);
	}
	vt_str ( vc );
	if (vc->charmap)
        {
		free ( vc->charmap, M_DEVBUF );

		MALLOC (vc->charmap, int *, sizeof(int)*((vc->xchars)*(vc->ychars)), M_DEVBUF, M_NOWAIT );
		if ((vc->flags&LOSSY)==0)
			mapped_cls(vc);
		if (vc==vconsole_current)
	     		vc->CLS(vc);
	vc->xcur=0;
	vc->ycur=0;
	}

	return 0;
}

int
vt220_attach(vc, a, b, aux)
	struct vconsole *vc;
	struct device *a;
	struct device *b;
	void *aux;
{
	return 0;
}

struct terminal_emulator vt220 = {
	vt220_name,
	vt220_init,
	vt220_putstring,
	vt220_swapin,
	vt220_swapout,
	vt220_sleep,
	vt220_wake,
	vt220_scrollback,
	vt220_scrollforward,
	vt220_scrollbackend,
	vt220_debugprint,
	vt220_modechange,
	vt220_attach
};
