/*	$NetBSD: custom.h,v 1.9 1995/03/28 18:14:32 jtc Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
 * This is a rewrite (retype) of the Amiga's custom chip register map, based
 * on the Hardware Reference Manual.  It is NOT based on the Amiga's
 * hardware/custom.h.
 */

#ifndef _AMIGA_CUSTOM_
#define _AMIGA_CUSTOM_

#ifndef LOCORE
struct Custom {
    /*** read-only registers ***/
	unsigned short zz1;
	unsigned short dmaconr;
	unsigned short vposr;
	unsigned short vhposr;
	unsigned short zz2;
	unsigned short joy0dat;
	unsigned short joy1dat;
	unsigned short clxdat;
	unsigned short adkconr;
	unsigned short pot0dat;
	unsigned short pot1dat;
	unsigned short potgor;
	unsigned short serdatr;
	unsigned short dskbytr;
	unsigned short intenar;
	unsigned short intreqr;

	/*** write-only registers ***/

	/* disk */
	void *dskpt;
	unsigned short dsklen;

	unsigned short zz3[2];
	unsigned short vposw;
	unsigned short vhposw;
	unsigned short copcon;
	unsigned short serdat;
	unsigned short serper;
	unsigned short potgo;
	unsigned short joytest;
	unsigned short zz4[4];

	/* blitter */
	unsigned short bltcon0;
	unsigned short bltcon1;
	unsigned short bltafwm;
	unsigned short bltalwm;
	void *bltcpt;
	void *bltbpt;
	void *bltapt;
	void *bltdpt;
	unsigned short bltsize;
	unsigned short zz5[3];
	unsigned short bltcmod;
	unsigned short bltbmod;
	unsigned short bltamod;
	unsigned short bltdmod;
	unsigned short zz6[4];
	unsigned short bltcdat;
	unsigned short bltbdat;
	unsigned short bltadat;
	unsigned short zz7[3];
	unsigned short deniseid;

	/* more disk */
	unsigned short dsksync;

    /* copper */
	union {
		void *cp;
		struct {
			unsigned short ch, cl;
		} cs;
	} _cop1lc;
#define cop1lc	_cop1lc.cp
#define cop1lch	_cop1lc.cs.ch
#define cop1lcl	_cop1lc.cs.cl
	union {
		void *cp;
		struct {
			unsigned short ch;
			unsigned short cl;
		} cs;
	} _cop2lc;
#define cop2lc	_cop2lc.cp
#define cop2lch	_cop2lc.cs.ch
#define cop2lcl	_cop2lc.cs.cl
	unsigned short copjmp1;
	unsigned short copjmp2;
	unsigned short copins;

	/* display parameters */
	unsigned short diwstrt;
	unsigned short diwstop;
	unsigned short ddfstrt;
	unsigned short ddfstop;

	/* control registers */
	unsigned short dmacon;
	unsigned short clxcon;
	unsigned short intena;
	unsigned short intreq;

	/* audio */
	unsigned short adkcon;
	struct Audio {
		void *lc;
		unsigned short len;
		unsigned short per;
		unsigned short vol;
		unsigned short zz[3];
	} aud[4];

	/* display */
	union {
		void *bp[6];
		struct {
			unsigned short bph;
			unsigned short bpl;
		} bs[6];
	} _bplpt;
#define bplpt	_bplpt.bp
#define bplptl(n)	_bplpt.bs[n].bpl
#define bplpth(n)	_bplpt.bs[n].bph

	unsigned short zz8[4];
	unsigned short bplcon0;
	unsigned short bplcon1;
	unsigned short bplcon2;
	unsigned short zz9;
	unsigned short bpl1mod;
	unsigned short bpl2mod;
	unsigned short zz10[2+6+2];

	/* sprites */
	void *sprpt[8];
	struct Sprite {
		unsigned short pos;
		unsigned short ctl;
		unsigned short data;
		unsigned short datb;
	} spr[8];

	unsigned short color[32];
	unsigned short htotal;
	unsigned short hsstop;
	unsigned short hbstrt;
	unsigned short hbstop;
	unsigned short vtotal;
	unsigned short vsstop;
	unsigned short vbstrt;
	unsigned short vbstop;
	unsigned short sprhstrt;
	unsigned short sprhstop;
	unsigned short bplhstrt;
	unsigned short bplhstop;
	unsigned short hhposw;
	unsigned short hhposr;
	unsigned short beamcon0;
	unsigned short hsstrt;
	unsigned short vsstrt;
	unsigned short hcenter;
	unsigned short diwhigh;	/* 1e4 */
	unsigned short padf3[11];
	unsigned short fmode;
};
#endif


/* Custom chips as seen by the kernel */
#ifdef _KERNEL
#ifndef LOCORE
vm_offset_t CUSTOMADDR, CUSTOMbase;
#define CUSTOMBASE	(0x00DFF000)	/* now just offset rel to zorro2 */
#endif
#define custom (*((volatile struct Custom *)CUSTOMbase))
#endif

/* This is used for making copper lists.  */
#define CUSTOM_OFS(field) ((long)&((struct Custom*)0)->field)

/* Bit definitions for dmacon and dmaconr */
#define DMAB_SETCLR     15
#define DMAB_BLTDONE    14
#define DMAB_BLTNZERO   13
#define DMAB_BLITHOG    10
#define DMAB_MASTER     9
#define DMAB_RASTER     8
#define DMAB_COPPER     7
#define DMAB_BLITTER    6
#define DMAB_SPRITE     5
#define DMAB_DISK       4
#define DMAB_AUD3       3
#define DMAB_AUD2       2
#define DMAB_AUD1       1
#define DMAB_AUD0       0

#define DMAF_SETCLR     (1<<DMAB_SETCLR)
#define DMAF_BLTDONE    (1<<DMAB_BLTDONE)
#define DMAF_BLTNZERO   (1<<DMAB_BLTNZERO)
#define DMAF_BLITHOG    (1<<DMAB_BLITHOG)
#define DMAF_MASTER     (1<<DMAB_MASTER)
#define DMAF_RASTER     (1<<DMAB_RASTER)
#define DMAF_COPPER     (1<<DMAB_COPPER)
#define DMAF_BLITTER    (1<<DMAB_BLITTER)
#define DMAF_SPRITE     (1<<DMAB_SPRITE)
#define DMAF_DISK       (1<<DMAB_DISK)
#define DMAF_AUD3       (1<<DMAB_AUD3)
#define DMAF_AUD2       (1<<DMAB_AUD2)
#define DMAF_AUD1       (1<<DMAB_AUD1)
#define DMAF_AUD0       (1<<DMAB_AUD0)



/* Bit definitions for intena, intenar, intreq, and intreqr */
#define INTB_SETCLR     15
#define INTB_INTEN      14
#define INTB_EXTER      13
#define INTB_DSKSYNC    12
#define INTB_RBF        11
#define INTB_AUD3       10
#define INTB_AUD2       9
#define INTB_AUD1       8
#define INTB_AUD0       7
#define INTB_BLIT       6
#define INTB_VERTB      5
#define INTB_COPER      4
#define INTB_PORTS      3
#define INTB_SOFTINT    2
#define INTB_DSKBLK     1
#define INTB_TBE        0

#define INTF_SETCLR     (1<<INTB_SETCLR)
#define INTF_INTEN      (1<<INTB_INTEN)
#define INTF_EXTER      (1<<INTB_EXTER)
#define INTF_DSKSYNC    (1<<INTB_DSKSYNC)
#define INTF_RBF        (1<<INTB_RBF)
#define INTF_AUD3       (1<<INTB_AUD3)
#define INTF_AUD2       (1<<INTB_AUD2)
#define INTF_AUD1       (1<<INTB_AUD1)
#define INTF_AUD0       (1<<INTB_AUD0)
#define INTF_BLIT       (1<<INTB_BLIT)
#define INTF_VERTB      (1<<INTB_VERTB)
#define INTF_COPER      (1<<INTB_COPER)
#define INTF_PORTS      (1<<INTB_PORTS)
#define INTF_SOFTINT    (1<<INTB_SOFTINT)
#define INTF_DSKBLK     (1<<INTB_DSKBLK)
#define INTF_TBE        (1<<INTB_TBE)

/* Bit definitions for adkcon, adkconr */
#define ADKB_SETCLR   15
#define ADKB_PRECOMP1 14
#define ADKB_PRECOMP0 13
#define ADKB_MFMPREC  12
#define ADKB_UARTBRK  11
#define ADKB_WORDSYNC 10
#define ADKB_MSBSYNC  9
#define ADKB_FAST     8

#define ADKF_SETCLR     (1<<ADKB_SETCLR)
#define ADKF_PRECOMP1 (1<<ADKB_PRECOMP1)
#define ADKF_PRECOMP0 (1<<ADKB_PRECOMP0)
#define ADKF_MFMPREC  (1<<ADKB_MFMPREC)
#define ADKF_UARTBRK  (1<<ADKB_UARTBRK)
#define ADKF_WORDSYNC (1<<ADKB_WORDSYNC)
#define ADKF_MSBSYNC  (1<<ADKB_MSBSYNC)
#define ADKF_FAST     (1<<ADKB_FAST)

#endif /* _AMIGA_CUSTOM_ */
