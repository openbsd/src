/* $NetBSD: vt220.h,v 1.2 1996/03/18 19:33:12 mark Exp $ */

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
 * vt220.h
 *
 * VT220 emulation header file
 *
 * Created      : 17/09/94
 */

#define MAXTABSTOPS	132

#define STATE_INIT	0
#define STATE_HASH 	1
#define STATE_BROPN	2
#define STATE_BRCLO 	3
#define STATE_STAR 	4
#define STATE_PLUS 	5
#define STATE_MINUS 	6
#define STATE_DOT 	7
#define STATE_SLASH 	8
#define STATE_AMPSND 	9
#define SHP_INIT 	10
#define STATE_ESC	11
#define STATE_CSIQM	14
#define STATE_CSI	15
#define STATE_DCS	16
#define STATE_SCA	17
#define STATE_STR	18

/* sub-states for Device Control String processing */

#define DCS_INIT        0       /* got ESC P ... */
#define DCS_AND_UDK     1       /* got ESC P ... | */
#define DCS_UDK_DEF     2       /* got ESC P ... | fnckey / */
#define DCS_UDK_ESC     3       /* got ESC P ... | fnckey / ... ESC */
#define DCS_DLD_DSCS    4       /* got ESC P ... { */
#define DCS_DLD_DEF     5       /* got ESC P ... { dscs */
#define DCS_DLD_ESC     6       /* got ESC P ... { dscs ... / ... ESC */

#define F_AWM		(1)
#define F_LASTCHAR	(2)
#define F_IRM		(3)

#define MAXPARMS	10

struct vt220_info {
	int tab_stops[MAXTABSTOPS];
	int state;
	int hp_state;
	int disable_function;
	int lastchar;
	int beepoff;
	int param[MAXPARMS];
	int parami;
	u_char m_om, sc_om;		/* flag, vt100 mode, origin mode */	
	u_char scrr_beg;
	u_char scrr_len;
	u_char scrr_end;
	int simple_cursor_store;
	int simple_cursor_on;

	int dcs_state;

	int G0, G1, G2, G3, GL, GR, sc_G0, sc_G1, sc_G2, sc_G3, sc_GL, sc_GR;
	int Gs, ss;
	int xcur, ycur,	sc_xcur, sc_ycur;
	int sc_flag;

	char *report_chars;
	int report_count;
	int cursor_on;

	int fgcol, bgcol;
	int nfgcol, nbgcol;

	int attribs;
	int flags;
	int sflags;

	int lastpos;
	int irm;
};

