/*	$OpenBSD: stireg.h,v 1.10 2005/01/24 19:20:04 miod Exp $	*/

/*
 * Copyright (c) 2000 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _IC_STIREG_H_
#define _IC_STIREG_H_

/* #define	STIDEBUG */

#define	STI_REGION_MAX	8
#define	STI_MONITOR_MAX	256
#define	STI_DEVNAME_LEN	32
#define	STI_NCMAP	256

/* code ROM definitions */
#define	STI_BEGIN	0
#define	STI_INIT_GRAPH	0
#define	STI_STATE_MGMT	1
#define	STI_FONT_UNPMV	2
#define	STI_BLOCK_MOVE	3
#define	STI_SELF_TEST	4
#define	STI_EXCEP_HDLR	5
#define	STI_INQ_CONF	6
#define	STI_SCM_ENT	7
#define	STI_DMA_CTRL	8
#define	STI_FLOW_CTRL	9
#define	STI_UTIMING	10
#define	STI_PROC_MGR	11
#define	STI_UTIL	12
#define	STI_END		13
#define	STI_CODECNT	16

#define	STI_CODEBASE_MAIN	0x40
#define	STI_CODEBASE_ALT	0x80

#define	STI_CODEBASE_PA		STI_CODEBASE_MAIN
#define	STI_CODEBASE_M68K	STI_CODEBASE_ALT
#define	STI_CODEBASE_PA64	STI_CODEBASE_ALT

/* sti returns */
#define	STI_OK		0
#define	STI_FAIL	-1
#define	STI_NRDY	1

/* sti errno */
#define	STI_NOERRNO		0	/* no error */
#define	STI_BADREENTLVL		1	/* bad reentry level */
#define	STI_NOREGIONSDEF	2	/* region table is not setup */
#define	STI_ILLNPLANES		3	/* invalid num of text planes */
#define	STI_ILLINDEX		4	/* invalid fond index */
#define	STI_ILLLOC		5	/* invalid font location */
#define	STI_ILLCOLOUR		6	/* invalid colour */
#define	STI_ILLBLKMVFROM	7	/* invalid from in blkmv */
#define	STI_ILLBLKMVTO		8	/* invalid to in blkmv */
#define	STI_ILLBLKMVSIZE	9	/* invalid siz in blkmv */
#define	STI_BEIUNSUPP		10	/* bus error ints unsupported */
#define	STI_UNXPBE		11	/* unexpected bus error */
#define	STI_UNXHWF		12	/* unexpected hardware failure */
#define	STI_NEGCFG		13	/* no ext global config struct */
#define	STI_NEIG		14	/* no ext init struct */
#define	STI_ILLSCME		15	/* invalid set cmap entry */
#define	STI_ILLCMVAL		16	/* invalid cmap value */
#define	STI_NORESMEM		17	/* no requested global memory */
#define	STI_RESMEMCORR		18	/* reserved memory corrupted */
#define	STI_ILLNTBLKMV		19	/* invalid non-text blkmv */
#define	STI_ILLMONITOR		20	/* monitor selection is out of range */
#define	STI_ILLEXCADDR		21	/* invalid excpt handler addr */
#define	STI_ILLEXCFLAGS		22	/* invalid excpt handler flags */
#define	STI_NOEHE		23	/* no ext exhdl struct */
#define	STI_NOINQCE		24	/* no ext inq cfg struct */
#define	STI_ILLRGNPTR		25	/* invalid region pointer */
#define	STI_ILLUTLOP		26	/* invalid util opcode */
#define	STI_UNKNOWN		250	/* unknown error */
#define	STI_NOCFGPTR		251	/* no config ptr defined */
#define	STI_NOFLPTR		252	/* no flag ptr defined */
#define	STI_NOINPTR		253	/* no in ptr defined */
#define	STI_NOOUTPTR		254	/* no way you can get it */
#define	STI_NOLOCK		255	/* kernel dishonour graphics lock */

/* colours */
#define	STI_COLOUR_BLACK	0
#define	STI_COLOUR_WHITE	1
#define	STI_COLOUR_RED		2
#define	STI_COLOUR_YELLOW	3
#define	STI_COLOUR_GREEN	4
#define	STI_COLOUR_CYAN		5
#define	STI_COLOUR_BLUE		6
#define	STI_COLOUR_MAGENTA	7

#pragma pack(1)

	/* LSB high */
struct	sti_dd {
	u_int32_t	dd_type;	/* 0x00 device type */
#define	STI_DEVTYPE1	1
#define	STI_DEVTYPE4	3
	u_int8_t	dd_unused;
	u_int8_t	dd_nmon;	/* 0x05 number monitor rates */
	u_int8_t	dd_grrev;	/* 0x06 global rom revision */
	u_int8_t	dd_lrrev;	/* 0x07 local rom revision */
	u_int8_t	dd_grid[8];	/* 0x08 graphics id */
	u_int32_t	dd_fntaddr;	/* 0x10 font start address */
	u_int32_t	dd_maxst;	/* 0x14 max state storage */
	u_int32_t	dd_romend;	/* 0x18 rom last address */
	u_int32_t	dd_reglst;	/* 0x1c device region list */
	u_int16_t	dd_maxreent;	/* 0x20 max reent storage */
	u_int16_t	dd_maxtimo;	/* 0x22 max execution timeout .1 sec */
	u_int32_t	dd_montbl;	/* 0x24 mon table address, array of
						names num of dd_nmon */
	u_int32_t	dd_udaddr;	/* 0x28 user data address */
	u_int32_t	dd_stimemreq;	/* 0x2c sti memory request */
	u_int32_t	dd_udsize;	/* 0x30 user data size */
	u_int16_t	dd_pwruse;	/* 0x34 power usage */
	u_int8_t	dd_bussup;	/* 0x36 bus support */
#define	STI_BUSSUPPORT_GSCINTL	0x01	/*	supports pulling INTL for int */
#define	STI_BUSSUPPORT_GSC15X	0x02	/*	supports GSC 1.5X */
#define	STI_BUSSUPPORT_GSC2X	0x04	/*	supports GSC 2.X */
#define	STI_BUSSUPPORT_PCIIOEIM	0x08	/*	will use directed int */
#define	STI_BUSSUPPORT_PCISTD	0x10	/*	will use std PCI int */
#define	STI_BUSSUPPORT_ILOCK	0x20	/*	supports implicit locking */
#define	STI_BUSSUPPORT_ROMMAP	0x40	/*	rom is only in pci erom space */
#define	STI_BUSSUPPORT_2DECODE	0x80	/*	single address decoder */
	u_int8_t	dd_ebussup;	/* 0x37 extended bus support */
#define	STI_EBUSSUPPORT_DMA	0x01	/*	supports dma */
#define	STI_EBUSSUPPORT_PIOLOCK	0x02	/*	no implicit locking for dma */
	u_int8_t	dd_altcodet;	/* 0x38 alternate code type */
#define	STI_ALTCODE_UNKNOWN	0x00
#define	STI_ALTCODE_PA64	0x01	/*	alt code is in pa64 */
	u_int8_t	dd_eddst[3];	/* 0x39 extended DD struct */
	u_int32_t	dd_cfbaddr;	/* 0x3c CFB address, location of
						X11 driver to be used for
						servers w/o accel */
	u_int32_t	dd_pacode[16];	/* 0x40 routines for pa-risc */
	u_int32_t	dd_altcode[16];	/* 0x80 routines for m68k/i386 */
};

#define	STI_REVISION(maj, min)	(((maj) << 4) | ((min) & 0x0f))

/* after the last region there is one indirect list ptr */
struct sti_region {
	u_int	offset  :14;	/* page offset dev io space relative */
	u_int	sys_only: 1;	/* whether allow user access */
	u_int	cache   : 1;	/* map in cache */
	u_int	btlb    : 1;	/* should use BTLB if available */
	u_int	last    : 1;	/* last region in the list */
	u_int	length  :14;	/* size in pages */
};

struct sti_font {
	u_int16_t	first;
	u_int16_t	last;
	u_int8_t	width;
	u_int8_t	height;
	u_int8_t	type;
#define	STI_FONT_HPROMAN8	1
#define	STI_FONT_KANA8		2
	u_int8_t	bpc;
	u_int32_t	next;
	u_int8_t	uheight;
	u_int8_t	uoffset;
	u_int8_t	unused[2];
};

struct sti_fontcfg {
	u_int16_t	first;
	u_int16_t	last;
	u_int8_t	width;
	u_int8_t	height;
	u_int8_t	type;
	u_int8_t	bpc;
	u_int8_t	uheight;
	u_int8_t	uoffset;
};

typedef struct sti_ecfg {
	u_int8_t	current_monitor;
	u_int8_t	uf_boot;
	u_int16_t	power;		/* power dissipation Watts */
	u_int32_t	freq_ref;
	u_int32_t	*addr;		/* memory block of size dd_stimemreq */
	void		*future;
} *sti_ecfg_t;

typedef struct sti_cfg {
	u_int32_t	text_planes;
	u_int16_t	scr_width;
	u_int16_t	scr_height;
	u_int16_t	oscr_width;
	u_int16_t	oscr_height;
	u_int16_t	fb_width;
	u_int16_t	fb_height;
	u_int32_t	regions[STI_REGION_MAX];
	u_int32_t	reent_level;
	u_int32_t	*save_addr;
	sti_ecfg_t	ext_cfg;
} *sti_cfg_t;


/* routine types */
#define	STI_DEP(n) \
	typedef int (*sti_##n##_t)( \
	  sti_##n##flags_t, sti_##n##in_t, sti_##n##out_t, sti_cfg_t);

typedef struct sti_initflags {
	u_int32_t	flags;
#define	STI_INITF_WAIT	0x80000000
#define	STI_INITF_RESET	0x40000000
#define	STI_INITF_TEXT	0x20000000
#define	STI_INITF_NTEXT	0x10000000
#define	STI_INITF_CLEAR	0x08000000
#define	STI_INITF_CMB	0x04000000	/* non-text planes cmap black */
#define	STI_INITF_EBET	0x02000000	/* enable bus error timer */
#define	STI_INITF_EBETI	0x01000000	/* enable bus error timer interrupt */
#define	STI_INITF_PTS	0x00800000	/* preserve text settings */
#define	STI_INITF_PNTS	0x00400000	/* preserve non-text settings */
#define	STI_INITF_PBET	0x00200000	/* preserve BET settings */
#define	STI_INITF_PBETI	0x00100000	/* preserve BETI settings */
#define	STI_INITF_ICMT	0x00080000	/* init cmap for text planes */
#define	STI_INITF_SCMT	0x00040000	/* change current monitor type */
#define	STI_INITF_RIE	0x00020000	/* retain int enables */
	void *future;
} *sti_initflags_t;

typedef struct sti_einitin {
	u_int8_t	mon_type;
	u_int8_t	pad;
	u_int16_t	inflight;	/* possible on pci */
	void		*future;
} *sti_einitin_t;

typedef struct sti_initin {
	u_int32_t	text_planes;	/* number of planes for text */
	sti_einitin_t	ext_in;
} *sti_initin_t;

typedef struct sti_initout {
	int32_t		errno;
	u_int32_t	text_planes;	/* number of planes used for text */
	void		*future;
} *sti_initout_t;

STI_DEP(init);

typedef struct sti_mgmtflags {
	u_int32_t	flags;
#define	STI_MGMTF_WAIT	0x80000000
#define	STI_MGMTF_SAVE	0x40000000
#define	STI_MGMTF_RALL	0x20000000	/* restore all display planes */
	void *future;
} *sti_mgmtflags_t;

typedef struct sti_mgmtin {
	void	*addr;
	void	*future;
} *sti_mgmtin_t;

typedef struct sti_mgmtout {
	int32_t		errno;
	void		*future;
} *sti_mgmtout_t;

STI_DEP(mgmt);

typedef struct sti_unpmvflags {
	u_int32_t	flags;
#define	STI_UNPMVF_WAIT	0x80000000
#define	STI_UNPMVF_NTXT	0x40000000	/* intp non-text planes */
	void		*future;
} *sti_unpmvflags_t;

typedef struct sti_unpmvin {
	u_int32_t	*font_addr;	/* font */
	u_int16_t	index;		/* character index in the font */
	u_int8_t	fg_colour;
	u_int8_t	bg_colour;
	u_int16_t	x, y;
	void		*future;
} *sti_unpmvin_t;

typedef struct sti_unpmvout {
	u_int32_t	errno;
	void		*future;
} *sti_unpmvout_t;

STI_DEP(unpmv);

typedef struct sti_blkmvflags {
	u_int32_t	flags;
#define	STI_BLKMVF_WAIT	0x80000000
#define	STI_BLKMVF_COLR	0x40000000	/* change colour on move */
#define	STI_BLKMVF_CLR	0x20000000	/* clear on move */
#define	STI_BLKMVF_NTXT	0x10000000	/* move in non-text planes */
	void		*future;
} *sti_blkmvflags_t;

typedef struct sti_blkmvin {
	u_int8_t	fg_colour;
	u_int8_t	bg_colour;
	u_int16_t	srcx, srcy, dstx, dsty;
	u_int16_t	width, height;
	u_int16_t	pad;
	void		*future;
} *sti_blkmvin_t;

typedef struct sti_blkmvout {
	u_int32_t	errno;
	void		*future;
} *sti_blkmvout_t;

STI_DEP(blkmv);

typedef struct sti_testflags {
	u_int32_t	flags;
#define	STI_TESTF_WAIT	0x80000000
#define	STI_TESTF_ETST	0x40000000
	void		*future;
} *sti_testflags_t;

typedef struct sti_testin {
	void		*future;
} *sti_testin_t;

typedef struct sti_testout {
	u_int32_t	errno;
	u_int32_t	result;
	void		*future;
} *sti_testout_t;

STI_DEP(test);

typedef struct sti_exhdlflags {
	u_int32_t	flags;
#define	STI_EXHDLF_WAIT	0x80000000
#define	STI_EXHDLF_CINT	0x40000000	/* clear int */
#define	STI_EXHDLF_CBE	0x20000000	/* clear BE */
#define	STI_EXHDLF_PINT	0x10000000	/* preserve int */
#define	STI_EXHDLF_RINT	0x08000000	/* restore int */
#define	STI_EXHDLF_WEIM	0x04000000	/* write eim w/ sti_eexhdlin */
#define	STI_EXHDLF_REIM	0x02000000	/* read eim to sti_eexhdlout */
#define	STI_EXHDLF_GIE	0x01000000	/* global int enable */
#define	STI_EXHDLF_PGIE	0x00800000
#define	STI_EXHDLF_WIEM	0x00400000
#define	STI_EXHDLF_EIEM	0x00200000
#define	STI_EXHDLF_BIC	0x00100000	/* begin int cycle */
#define	STI_EXHDLF_EIC	0x00080000	/* end int cycle */
#define	STI_EXHDLF_RIE	0x00040000	/* reset do not clear int enables */
	void		*future;
} *sti_exhdlflags_t;

typedef struct sti_eexhdlin {
	u_int32_t	eim_addr;
	u_int32_t	eim_data;
	u_int32_t	iem;		/* enable mask */
	u_int32_t	icm;		/* clear mask */
	void		*future;
} *sti_eexhdlin_t;

typedef struct sti_exhdlint {
	u_int32_t	flags;
#define	STI_EXHDLINT_BET	0x80000000	/* bus error timer */
#define	STI_EXHDLINT_HW		0x40000000	/* high water */
#define	STI_EXHDLINT_LW		0x20000000	/* low water */
#define	STI_EXHDLINT_TM		0x10000000	/* texture map */
#define	STI_EXHDLINT_VB		0x08000000	/* vertical blank */
#define	STI_EXHDLINT_UDC	0x04000000	/* unbuffered dma complete */
#define	STI_EXHDLINT_BDC	0x02000000	/* buffered dma complete */
#define	STI_EXHDLINT_UDPC	0x01000000	/* unbuf priv dma complete */
#define	STI_EXHDLINT_BDPC	0x00800000	/* buffered priv dma complete */
} *sti_exhdlint_t;

typedef struct sti_exhdlin {
	sti_exhdlint_t	addr;
	sti_eexhdlin_t	ext;
} *sti_exhdlin_t;

typedef struct sti_eexhdlout {
	u_int32_t	eim_addr;
	u_int32_t	eim_data;
	u_int32_t	iem;		/* enable mask */
	u_int32_t	icm;		/* clear mask */
	void		*future;
} *sti_eexhdlout_t;

typedef struct sti_exhdlout {
	u_int32_t	errno;
	u_int32_t	flags;
#define	STI_EXHDLO_BE	0x80000000	/* BE was intercepted */
#define	STI_EXHDLO_IP	0x40000000	/* there is int pending */
#define	STI_EXHDLO_IE	0x20000000	/* global enable set */
	sti_eexhdlout_t	ext;
} *sti_exhdlout_t;

STI_DEP(exhdl);

typedef struct sti_inqconfflags {
	u_int32_t	flags;
#define	STI_INQCONFF_WAIT	0x80000000
	void		*future;
} *sti_inqconfflags_t;

typedef struct sti_inqconfin {
	void	*future;
} *sti_inqconfin_t;

typedef struct sti_einqconfout {
	u_int32_t	crt_config[3];
	u_int32_t	crt_hw[3];
	void		*future;
} *sti_einqconfout_t;

typedef struct sti_inqconfout {
	u_int32_t	errno;
	u_int16_t	width, height, owidth, oheight, fbwidth, fbheight;
	u_int32_t	bpp;	/* bits per pixel */
	u_int32_t	bppu;	/* accessible bpp */
	u_int32_t	planes;
	u_int8_t	name[STI_DEVNAME_LEN];
	u_int32_t	attributes;
#define	STI_INQCONF_Y2X		0x0001	/* pixel is higher tan wider */
#define	STI_INQCONF_HWBLKMV	0x0002	/* hw blkmv is present */
#define	STI_INQCONF_AHW		0x0004	/* adv hw accel */
#define	STI_INQCONF_INT		0x0008	/* can interrupt */
#define	STI_INQCONF_GONOFF	0x0010	/* supports on/off */
#define	STI_INQCONF_AONOFF	0x0020	/* supports alpha on/off */
#define	STI_INQCONF_VARY	0x0040	/* variable fb height */
#define	STI_INQCONF_ODDBYTES	0x0080	/* use only odd fb bytes */
#define	STI_INQCONF_FLUSH	0x0100	/* fb cache requires flushing */
#define	STI_INQCONF_DMA		0x0200	/* supports dma */
#define	STI_INQCONF_VDMA	0x0400	/* supports vdma */
#define	STI_INQCONF_YUV1	0x2000	/* supports YUV type 1 */
#define	STI_INQCONF_YUV2	0x4000	/* supports YUV type 2 */
#define	STI_INQCONF_BITS \
    "\020\001y2x\002hwblkmv\003ahw\004int\005gonoff\006aonoff\007vary"\
    "\010oddb\011flush\012dma\013vdma\016yuv1\017yuv2"
	sti_einqconfout_t ext;
} *sti_inqconfout_t;

STI_DEP(inqconf);

typedef struct sti_scmentflags {
	u_int32_t	flags;
#define	STI_SCMENTF_WAIT	0x80000000
	void		*future;
} *sti_scmentflags_t;

typedef struct sti_scmentin {
	u_int32_t	entry;
	u_int32_t	value;
	void		*future;
} *sti_scmentin_t;

typedef struct sti_scmentout {
	u_int32_t	errno;
	void		*future;
} *sti_scmentout_t;

STI_DEP(scment);

typedef struct sti_dmacflags {
	u_int32_t	flags;
#define	STI_DMACF_WAIT	0x80000000
#define	STI_DMACF_PRIV	0x40000000	/* priv dma */
#define	STI_DMACF_DIS	0x20000000	/* disable */
#define	STI_DMACF_BUF	0x10000000	/* buffered */
#define	STI_DMACF_MRK	0x08000000	/* write a marker */
#define	STI_DMACF_ABRT	0x04000000	/* abort dma xfer */
	void		*future;
} *sti_dmacflags_t;

typedef struct sti_dmacin {
	u_int32_t	pa_upper;
	u_int32_t	pa_lower;
	u_int32_t	len;
	u_int32_t	mrk_data;
	u_int32_t	mrk_off;
	void		*future;
} *sti_dmacin_t;

typedef struct sti_dmacout {
	u_int32_t	errno;
	void		*future;
} *sti_dmacout_t;

STI_DEP(dmac);

typedef struct sti_flowcflags {
	u_int32_t	flags;
#define	STI_FLOWCF_WAIT	0x80000000
#define	STI_FLOWCF_CHW	0x40000000	/* check high water */
#define	STI_FLOWCF_WHW	0x20000000	/* write high water */
#define	STI_FLOWCF_WLW	0x10000000	/* write low water */
#define	STI_FLOWCF_PCSE	0x08000000	/* preserve cse */
#define	STI_FLOWCF_CSE	0x04000000
#define	STI_FLOWCF_CSWF	0x02000000	/* cs write fine */
#define	STI_FLOWCF_CSWC	0x01000000	/* cs write coarse */
#define	STI_FLOWCF_CSWQ	0x00800000	/* cs write fifo */
	void		*future;
} *sti_flowcflags_t;

typedef struct sti_flowcin {
	u_int32_t	retry;
	u_int32_t	bufz;
	u_int32_t	hwcnt;
	u_int32_t	lwcnt;
	u_int32_t	csfv;	/* cs fine value */
	u_int32_t	cscv;	/* cs coarse value */
	u_int32_t	csqc;	/* cs fifo count */
	void		*future;
} *sti_flowcin_t;

typedef struct sti_flowcout {
	u_int32_t	errno;
	u_int32_t	retry_result;
	u_int32_t	fifo_size;
	void		*future;
} *sti_flowcout_t;

STI_DEP(flowc);

typedef struct sti_utimingflags {
	u_int32_t	flags;
#define	STI_UTIMF_WAIT	0x80000000
#define	STI_UTIMF_HKS	0x40000000	/* has kbuf_size */
	void		*future;
} *sti_utimingflags_t;

typedef struct sti_utimingin {
	void		*data;
	void		*kbuf;
	void		*future;
} *sti_utimingin_t;

typedef struct sti_utimingout {
	u_int32_t	errno;
	u_int32_t	kbuf_size;	/* buffer required size */
	void		*future;
} *sti_utimingout_t;

STI_DEP(utiming);

typedef struct sti_pmgrflags {
	u_int32_t	flags;
#define	STI_UTIMF_WAIT	0x80000000
#define	STI_UTIMOP_CLEANUP	0x00000000
#define	STI_UTIMOP_BAC		0x10000000
#define	STI_UTIMF_CRIT	0x04000000
#define	STI_UTIMF_BUFF	0x02000000
#define	STI_UTIMF_IBUFF	0x01000000
	void		*future;
} *sti_pmgrflags_t;

typedef struct sti_pmgrin {
	u_int32_t	reserved[4];
	void		*future;
} *sti_pmgrin_t;

typedef struct sti_pmgrout {
	int32_t		errno;
	void		*future;
} *sti_pmgrout_t;

STI_DEP(pmgr);

typedef struct sti_utilflags {
	u_int32_t	flags;
#define	STI_UTILF_ROOT	0x80000000	/* was called as root */
	void		*future;
} *sti_utilflags_t;

typedef struct sti_utilin {
	u_int32_t	in_size;
	u_int32_t	out_size;
	u_int8_t	*buf;
} *sti_utilin_t;

typedef struct sti_utilout {
	int32_t		errno;
	void		*future;
} *sti_utilout_t;

STI_DEP(util);

#pragma pack()

#endif /* _IC_STIREG_H_ */
