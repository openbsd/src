/*	$OpenBSD: stireg.h,v 1.1 1998/12/31 03:20:44 mickey Exp $	*/

/*
 * Copyright 1996 1995 by Open Software Foundation, Inc.   
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */
/* 
 * Copyright (c) 1991,1992,1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *	Utah $Hdr: grf_stireg.h 1.7 94/12/14$
 */

#ifndef _STIREG_H_
#define _STIREG_H_

/*
 * Standard Text Interface.
 */

#define STI_CODECNT	7
#define STI_REGIONS	8
#define STI_NAMELEN	32

/*
 * Structure templates for old byte-wide interface
 */
typedef struct {
	char	p[3];
	u_char	b0;
} stibyte;

typedef struct {
	char	p0[3];
	u_char	b0;
	char	p1[3];
	u_char	b1;
} stihalf;

typedef struct {
	char	p0[3];
	u_char	b0;
	char	p1[3];
	u_char	b1;
	char	p2[3];
	u_char	b2;
	char	p3[3];
	u_char	b3;
} stiword;

/*
 * STI ROM layout, byte-wide and word-wide interfaces.
 * Word-wide fields are aligned to word boundaries for access even
 * though some are technically smaller (e.g. devtype is 8 bits).
 */
struct sti_bytewide {
	stibyte	sti_devtype;	/* 0x03: device type (see below) */
	stihalf	sti_revno;	/* 0x07: global/local ROM revision */
	char	sti_p0[0x4];
	stiword	sti_gid_hi;	/* 0x13: graphics ID (high word) */
	stiword	sti_gid_lo;	/* 0x23: graphics ID (low byte) */
	stiword	sti_fontaddr;	/* 0x33: font start address */
	stiword	sti_mss;	/* 0x43: max state storage */
	stiword	sti_erom;	/* 0x53: last address of ROM */
	stiword	sti_mmap;	/* 0x63: memory map information */
	stihalf	sti_mstore;	/* 0x73: max re-entrant storage */
	stihalf	sti_mtimo;	/* 0x7b: max 1/10 secs for routines */
#ifdef hp300
	char	sti_p1[0x180];
#else
	char	sti_p1[0x80];
#endif
	stiword	sti_routine[STI_CODECNT+1]; /* 0x103: routines */
};

struct sti_wordwide {
	u_int	sti_devtype;	/* 0x00: device type (see below) */
	u_int	sti_revno;	/* 0x04: global ROM revision */
	u_int	sti_gid_hi;	/* 0x08: graphics ID (high word) */
	u_int	sti_gid_lo;	/* 0x0c: graphics ID (low byte) */
	u_int	sti_fontaddr;	/* 0x10: font start address */
	u_int	sti_mss;	/* 0x14: max state storage */
	u_int	sti_erom;	/* 0x18: last address of ROM */
	u_int	sti_mmap;	/* 0x1c: memory map information */
	u_int	sti_mstore;	/* 0x20: max re-entrant storage */
	u_int	sti_mtimo;	/* 0x24: max 1/10 secs for routines */
#ifdef hp300
	char	sti_p0[0x58];
#else
	char	sti_p0[0x18];
#endif
	u_int	sti_routine[STI_CODECNT+1]; /* 0x40: routines */
};

#define	STI_GETBYTE(t,a,f) \
	(((t) == STI_TYPE_BWGRF) ? \
		((struct sti_bytewide *)(a))->f.b0 : \
		(((struct sti_wordwide *)(a))->f & 0xff))

#define	STI_GETHALF(t,a,f) \
	(((t) == STI_TYPE_BWGRF) ? \
		((((struct sti_bytewide *)(a))->f.b0 << 8) | \
		 (((struct sti_bytewide *)(a))->f.b1)) : \
		(((struct sti_wordwide *)(a))->f & 0xffff))

#define	STI_GETWORD(t,a,f) \
	(((t) == STI_TYPE_BWGRF) ? \
		((((struct sti_bytewide *)(a))->f.b0 << 24) | \
		 (((struct sti_bytewide *)(a))->f.b1 << 16) | \
		 (((struct sti_bytewide *)(a))->f.b2 << 8) | \
		 (((struct sti_bytewide *)(a))->f.b3)) : \
		((struct sti_wordwide *)(a))->f)


#define	STI_DEVTYP(t,a)	STI_GETBYTE(t,a,sti_devtype)
#define	STI_GLOREV(t,a)	(STI_GETHALF(t,a,sti_revno) >> 8)
#define	STI_LOCREV(t,a)	(STI_GETHALF(t,a,sti_revno) & 0xff)
#define	STI_ID_HI(t,a)	STI_GETWORD(t,a,sti_gid_hi)
#define	STI_ID_LO(t,a)	STI_GETWORD(t,a,sti_gid_lo)
#define	STI_FONTAD(t,a)	((u_int)(a) + STI_GETWORD(t,a,sti_fontaddr))
#define	STI_MSS(t,a)	((u_int)(a) + STI_GETWORD(t,a,sti_mss))
#define	STI_EROM(t,a)	((u_int)(a) + STI_GETWORD(t,a,sti_erom))
#define	STI_MMAP(t,a)	((u_int)(a) + STI_GETWORD(t,a,sti_mmap))
#define	STI_MSTOR(t,a)	STI_GETHALF(t,a,sti_mstore)
#define	STI_MTOUT(t,a)	STI_GETHALF(t,a,sti_mtimo)

	/* INIT_GRAPH address */
#define	STI_IGADDR(t,a)	((u_int)(a) + STI_GETWORD(t,a,sti_routine[0]))
	/* STATE_MGMT address */
#define	STI_SMADDR(t,a)	((u_int)(a) + STI_GETWORD(t,a,sti_routine[1]))
	/* FONT_UNP/MV address*/
#define	STI_FUADDR(t,a)	((u_int)(a) + STI_GETWORD(t,a,sti_routine[2]))
	/* BLOCK_MOVE address */
#define	STI_BMADDR(t,a)	((u_int)(a) + STI_GETWORD(t,a,sti_routine[3]))
	/* SELF_TEST address */
#define	STI_STADDR(t,a)	((u_int)(a) + STI_GETWORD(t,a,sti_routine[4]))
	/* EXCEP_HDLR address */
#define	STI_EHADDR(t,a)	((u_int)(a) + STI_GETWORD(t,a,sti_routine[5]))
	/* INQ_CONF address */
#define	STI_ICADDR(t,a)	((u_int)(a) + STI_GETWORD(t,a,sti_routine[6]))
	/* End address */
#define	STI_EADDR(t,a)	((u_int)(a) + STI_GETWORD(t,a,sti_routine[7]))

/* STI_ID_HI */
#define STI_ID_FDDI	0x280B31AF	/* Medusa STI ROM graphics ID */

/* STI_DEVTYP */
#define	STI_TYPE_BWGRF	1	/* graphics device (byte-wide if) */
#define	STI_TYPE_WWGRF	3	/* graphics device (word-wide if) */

/*
 * STI font information.
 * Note that fields of word-wide structure are not word aligned
 * making access a little harder.
 */
struct stifont_bytewide {
	stihalf	sti_firstchar;	/* 0x03: first character */
	stihalf	sti_lastchar;	/* 0x0b: last character */
	stibyte	sti_fwidth;	/* 0x13: font width */
	stibyte	sti_fheight;	/* 0x17: font height */
	stibyte	sti_ftype;	/* 0x1b: font type */
	stibyte	sti_bpc;	/* 0x1f: bytes per character */
	stiword	sti_next;	/* 0x23: offset of next font */
	stibyte	sti_uheight;	/* 0x33: underline height */
	stibyte	sti_uoffset;	/* 0x37: underline offset */
};

struct stifont_wordwide {
	u_short	sti_firstchar;	/* 0x00: first character */
	u_short	sti_lastchar;	/* 0x02: last character */
	u_char	sti_fwidth;	/* 0x04: font width */
	u_char	sti_fheight;	/* 0x05: font height */
	u_char	sti_ftype;	/* 0x06: font type */
	u_char	sti_bpc;	/* 0x07: bytes per character */
	u_int	sti_next;	/* 0x08: offset of next font */
	u_char	sti_uheight;	/* 0x0c: underline height */
	u_char	sti_uoffset;	/* 0x0d: underline offset */
	char	sti_p0[2];
};

#define STIF_FIRSTC(t,a) \
	(((t) == STI_TYPE_BWGRF) ? \
		((((struct stifont_bytewide *)(a))->sti_firstchar.b0 << 8) | \
		 (((struct stifont_bytewide *)(a))->sti_firstchar.b1)) : \
		(((volatile u_int *)(a))[0] >> 16))
#define STIF_LASTC(t,a) \
	(((t) == STI_TYPE_BWGRF) ? \
		((((struct stifont_bytewide *)(a))->sti_lastchar.b0 << 8) | \
		 (((struct stifont_bytewide *)(a))->sti_lastchar.b1)) : \
	 	(((volatile u_int *)(a))[0] & 0xffff))
#define STIF_FWIDTH(t,a) \
	(((t) == STI_TYPE_BWGRF) ? \
		((struct stifont_bytewide *)(a))->sti_fwidth.b0 : \
	 	(((volatile u_int *)(a))[1] >> 24))
#define STIF_FHEIGHT(t,a) \
	(((t) == STI_TYPE_BWGRF) ? \
		((struct stifont_bytewide *)(a))->sti_fheight.b0 : \
		((((volatile u_int *)(a))[1] >> 16) & 0xff))
#define STIF_FTYPE(t,a) \
	(((t) == STI_TYPE_BWGRF) ? \
		((struct stifont_bytewide *)(a))->sti_ftype.b0 : \
		((((volatile u_int *)(a))[1] >> 8) & 0xff))
#define STIF_BPC(t,a) \
	(((t) == STI_TYPE_BWGRF) ? \
		((struct stifont_bytewide *)(a))->sti_bpc.b0 : \
		(((volatile u_int *)(a))[1] & 0xff))
#define	STIF_NEXT(t,a) \
	(((t) == STI_TYPE_BWGRF) ? \
		((((struct stifont_bytewide *)(a))->sti_next.b0 << 24) | \
		 (((struct stifont_bytewide *)(a))->sti_next.b1 << 16) | \
		 (((struct stifont_bytewide *)(a))->sti_next.b2 << 8) | \
		 (((struct stifont_bytewide *)(a))->sti_next.b3)) : \
		((volatile u_int *)(a))[2])
#define STIF_UHEIGHT(t,a) \
	(((t) == STI_TYPE_BWGRF) ? \
		((struct stifont_bytewide *)(a))->sti_uheight.b0 : \
		(((volatile u_int *)(a))[3] >> 24))
#define STIF_UOFFSET(t,a) \
	(((t) == STI_TYPE_BWGRF) ? \
		((struct stifont_bytewide *)(a))->sti_uoffset.b0 : \
		((((volatile u_int *)(a))[3] >> 16) & 0xff))



/*
 * Device region information.
 */
struct sti_region {
	u_int	offset  :14,
		sysonly :1,
                cache   :1,
	        btlb    :1,
                last    :1,
	        length  :14;
};

/*
 * Global configuration information. There is one per STI device.
 */
struct sti_config {
	u_int	text_planes;
	u_short	dwidth;
	u_short	dheight;
	u_short	owidth;
	u_short	oheight;
	u_short	fbwidth;
	u_short	fbheight;
	u_int	regions[STI_REGIONS];
	u_int	reentry_level;
	u_int	save_address;
	u_int	*future;
};

/*
 * Font config
 */
struct sti_fontcfg {
	u_short	firstc;		/* first character */
	u_short	lastc;		/* last character */
	u_char	ftwidth;	/* font width */
	u_char	ftheight;	/* font height */
	u_char	ftype;		/* font type */
	u_char	bpc;		/* bytes per character */
	u_char	uheight;	/* underline height */
	u_char	uoffset;	/* underline offset */
};

/*
 * Device initialization structures.
 */
struct sti_initflags {	/* Initialization flags */
	u_int	wait           :1,
	        hardreset      :1,
		texton         :1, /* ignored if no_change_text == 1 */
		graphon        :1, /* ignored if no_change_graph == 1 */
		clear          :1,
		cmap_black     :1, /* ignored if hardreset == 0 */
		bus_error_timer:1, /* ignored if no_change_bet == 1 */
		bus_error_int  :1, /* ignored if no_change_bei == 1 */
		no_change_text :1,
		no_change_graph:1,
		no_change_bet  :1,
		no_change_bei  :1,
		init_text_cmap :1,
		pad	    :19;
	int	*future;
};

struct sti_initin {	/* Initialization input args */
	int	text_planes;
	int	*future;
};

struct sti_initout {	/* Initialization output args */
	int	errno;
	int	text_planes;
	int	*future;
};

/*
 * Inquire Configuration.
 */
struct sti_inquireflags {
	u_int	wait :1;
	u_int	pad  :31;
	int	*future;
};

struct sti_inquirein {
	int	*future;
};

struct sti_inquireout {
	int	errno;
	short	dwidth;
	short	dheight;
	short   owidth;
	short	oheight;
	short   fbwidth;
	short	fbheight;
	int	bpp;
	int	bits;
	int	planes;
	char	devname[STI_NAMELEN];
	u_int	attributes;
	int	*future;
};

/*
 * Font Unpack/Move.
 */
struct sti_fontflags {
	u_int	wait :1;
	u_int	pad  :31;
	int	*future;
};

struct sti_fontin {
	int	startaddr;
	short	index;
	char	fg_color;
	char	bg_color;
	short	dest_x;
	short	dest_y;
	int	*future;
};

struct sti_fontout {
	int	errno;
	int	*future;
};

/*
 * Block Move.
 */
struct sti_moveflags {
	u_int	wait  :1;
	u_int	color :1;
	u_int	clear :1;
	u_int	pad   :29;
	int	*future;
};

struct sti_movein {
	char	fg_color;
	char	bg_color;
	short	src_x;
	short	src_y;
	short	dest_x;
	short	dest_y;
	short	wwidth;
	short	wheight;
	int	*future;
};

struct sti_moveout {
	int	errno;
	int	*future;
};

/*
 * Error codes returned by STI ROM routines.
 */
#define NO_ERROR		0
#define BAD_REENTRY_LEVEL	1
#define NO_REGIONS_DEFINED	2
#define ILLEGAL_NUMBER_PLANES	3
#define INVALID_FONT_INDEX	4
#define INVALID_FONT_LOCATION	5
#define INVALID_COLOR		6
#define INVALID_BLKMOVE_SRC	7
#define INVALID_BLKMOVE_DST	8
#define INVALID_BLKMOVE_SIZE	9
#define NO_BUS_ERROR_INT	10
#define BUS_ERROR		11
#define HARDWARE_FAILURE	12

#endif /* _STIREG_H_ */
