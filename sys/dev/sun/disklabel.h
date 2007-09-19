/*	$OpenBSD: disklabel.h,v 1.5 2007/09/19 23:47:50 tsi Exp $	*/
/*	$NetBSD: disklabel.h,v 1.2 1998/08/22 14:55:28 mrg Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)sun_disklabel.h	8.1 (Berkeley) 6/11/93
 */

/*
 * SunOS disk label layout (only relevant portions discovered here).
 * This describes the format typically found on SPARC systems, but not
 * that usually seen on SunOS/x86 and SunOS/amd64 systems.
 */

#define	SUN_DKMAGIC	55998

/* partition info */
struct sun_dkpart {
	u_int	sdkp_cyloffset;		/* starting cylinder */
	u_int	sdkp_nsectors;		/* number of sectors */
};

/* partition types */
struct sun_partinfo {
	u_short	spi_tag;		/* filesystem type */
	u_short	spi_flag;		/* flags */
};

/* some spi_tag values */
#define SPTAG_EMPTY		0x00
#define SPTAG_BOOT		0x01
#define SPTAG_SUNOS_ROOT	0x02
#define SPTAG_SUNOS_SWAP	0x03
#define SPTAG_SUNOS_USR		0x04
#define SPTAG_WHOLE_DISK	0x05
#define SPTAG_SUNOS_STAND	0x06
#define SPTAG_SUNOS_VAR		0x07
#define SPTAG_SUNOS_HOME	0x08
#define SPTAG_LINUX_SWAP	0x82
#define SPTAG_LINUX_EXT2	0x83

#define	SUNXPART	8
#define	SL_XPMAG	(0x199d1fe2+SUNXPART)
#define	SL_XPMAGTYP	(0x199d1fe2+SUNXPART+1)		/* contains types */

struct sun_disklabel {			/* total size = 512 bytes */
	char		sl_text[128];
	union {
		/* Sun standard fields, also used on Linux */
		struct {
			/* label version */
			u_int	sli_version;
			/* short volume name */
			char	sli_volume[8];
			/* partition count */
			u_short	sli_nparts;
			struct sun_partinfo sli_part[8];
			char	sli_xxx1[292 - sizeof(u_int) -
					 (sizeof(char) * 8) - sizeof(u_short) -
					 (sizeof(struct sun_partinfo) * 8)];
		} i;
		/* BSD-specific extensions */
		struct {
			/* additive cksum, [xl_xpmag,sl_xx1) */
			u_int		slx_xpsum;
			/* "extended" magic number */
			u_int		slx_xpmag;
			/* "extended" partitions, i through p */
			struct sun_dkpart slx_xpart[SUNXPART];
			u_char		slx_types[MAXPARTITIONS];
			u_int8_t	slx_fragblock[MAXPARTITIONS];
			u_int16_t	slx_cpg[MAXPARTITIONS];
			char		slx_xxx1[292 - sizeof(u_int) -
						 sizeof(u_int) -
						 (sizeof(struct sun_dkpart) *
						  SUNXPART) -
						 (sizeof(u_char) *
						  MAXPARTITIONS) -
						 (sizeof(u_int8_t) *
						  MAXPARTITIONS) -
						 (sizeof(u_int16_t) *
						  MAXPARTITIONS)];
		} x;
	} u;
/* Compatibility */
#define sl_xpsum	u.x.slx_xpsum
#define sl_xpmag	u.x.slx_xpmag
#define sl_xpart	u.x.slx_xpart
#define sl_types	u.x.slx_types
#define sl_fragblock	u.x.slx_fragblock
#define sl_cpg		u.x.slx_cpg
#define sl_xxx1		u.x.slx_xxx1
/* Convenience */
#define sl_version	u.i.sli_version
#define sl_volume	u.i.sli_volume
#define sl_nparts	u.i.sli_nparts
#define sl_ipart	u.i.sli_part
#define sl_xxx1i	u.i.sli_xxx1
	u_short sl_rpm;			/* rotational speed */
	u_short	sl_pcylinders;		/* number of physical cyls */
#define	sl_pcyl	sl_pcylinders		/* XXX: old sun3 */
	u_short sl_sparespercyl;	/* spare sectors per cylinder */
	char	sl_xxx3[4];
	u_short sl_interleave;		/* interleave factor */
	u_short	sl_ncylinders;		/* data cylinders */
	u_short	sl_acylinders;		/* alternate cylinders */
	u_short	sl_ntracks;		/* tracks per cylinder */
	u_short	sl_nsectors;		/* sectors per track */
	char	sl_xxx4[4];
	struct sun_dkpart sl_part[8];	/* partition layout */
	u_short	sl_magic;		/* == SUN_DKMAGIC */
	u_short	sl_cksum;		/* xor checksum of all shorts */
};
