/*	$OpenBSD: hd_compat.c,v 1.1 1997/02/03 08:11:54 downsj Exp $	*/
/*	$NetBSD: rd_compat.c,v 1.6 1997/01/30 09:14:18 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * from: Utah $Hdr: rd_compat.c 1.1 92/12/21$
 *
 *	@(#)rd_compat.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Compatibility for CS80 disks without disklabels.
 */

#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/device.h>
#include <sys/buf.h>		/* XXX */

#include <hp300/dev/hpibvar.h>

#include <hp300/dev/hdreg.h>
#include <hp300/dev/hdvar.h>

/*
 * CS/80 partitions.  We reserve the first cylinder for a LIF
 * style boot directory (the 8k allowed in the BSD filesystem
 * is just way too small).  This boot area is outside of all but
 * the C partition.  This implies that you cannot use the C 
 * partition on a bootable disk since the filesystem would overlay
 * the boot area.  You must use the A partition.
 *
 * These maps support four basic layouts:
 *
 *	A/B/G:   This is the "traditional" setup for a bootable disk.
 *	         A is the root partition, B the swap, and G a user partition.
 *	A/D/H:   This is a setup for bootable systems requiring more swap
 *		 (e.g. those who use HPCL).  It has A as the root, D as a
 *		 larger swap, and H as a smaller user partition.
 *	A/D/E/F: Similar to A/D/H with E and F breaking H into two partitions.
 *		 E could be used for /usr and F for users.
 *	C:       This gives a single, non-bootable, large user filesystem.
 *	         Good for second drives on a machine (e.g. /usr/src).
 */
struct	size {
	daddr_t	nblocks;
	int	cyloff;
} hd7945A_sizes[8] = {
	HDSZ(15904),	1,		/* A=cyl 1 thru 142 */
	HDSZ(20160),	143,		/* B=cyl 143 thru 322 */
	HDSZ(108416),	0,		/* C=cyl 0 thru 967 */
	HDSZ(40320),	143,		/* D=cyl 143 thru 502 */
	HDSZ(0),	0,		/* E=<undefined> */
	HDSZ(0),	0,		/* F=<undefined> */
	HDSZ(72240),	323,		/* G=cyl 323 thru 967 */
	HDSZ(52080),	503,		/* H=cyl 503 thru 967 */
}, hd9134D_sizes[8] = {
	HDSZ(15936),	1,		/* A=cyl 1 thru 166 */
	HDSZ(13056),	167,		/* B=cyl 167 thru 302 */
	HDSZ(29088),	0,		/* C=cyl 0 thru 302 */
	HDSZ(0),	0,		/* D=<undefined> */
	HDSZ(0),	0,		/* E=<undefined> */
	HDSZ(0),	0,		/* F=<undefined> */
	HDSZ(0),	0,		/* G=<undefined> */
	HDSZ(0),	0,		/* H=<undefined> */
}, hd9122S_sizes[8] = {
	HDSZ(0),	0,		/* A=<undefined> */
	HDSZ(0),	0,		/* B=<undefined> */
	HDSZ(1232),	0,		/* C=cyl 0 thru 76 */
	HDSZ(0),	0,		/* D=<undefined> */
	HDSZ(0),	0,		/* E=<undefined> */
	HDSZ(0),	0,		/* F=<undefined> */
	HDSZ(0),	0,		/* G=<undefined> */
	HDSZ(0),	0,		/* H=<undefined> */
}, hd7912P_sizes[8] = {
	HDSZ(15904),	0,		/* A=cyl 1 thru 71 */
	HDSZ(22400),	72,		/* B=cyl 72 thru 171 */
	HDSZ(128128),	0,		/* C=cyl 0 thru 571 */
	HDSZ(42560),	72,		/* D=cyl 72 thru 261 */
	HDSZ(0),	292,		/* E=<undefined> */
	HDSZ(0),	542,		/* F=<undefined> */
	HDSZ(89600),	172,		/* G=cyl 221 thru 571 */
	HDSZ(69440),	262,		/* H=cyl 262 thru 571 */
}, hd7914P_sizes[8] = {
	HDSZ(15904),	1,		/* A=cyl 1 thru 71 */
	HDSZ(40320),	72,		/* B=cyl 72 thru 251 */
	HDSZ(258048),	0,		/* C=cyl 0 thru 1151 */
	HDSZ(64960),	72,		/* D=cyl 72 thru 361 */
	HDSZ(98560),	362,		/* E=cyl 362 thru 801 */
	HDSZ(78400),	802,		/* F=cyl 802 thru 1151 */
	HDSZ(201600),	252,		/* G=cyl 221 thru 1151 */
	HDSZ(176960),	362,		/* H=cyl 362 thru 1151 */
}, hd7933H_sizes[8] = {
	HDSZ(16146),	1,		/* A=cyl 1 thru 27 */
	HDSZ(66976),	28,		/* B=cyl 28 thru 139 */
	HDSZ(789958),	0,		/* C=cyl 0 thru 1320 */
	HDSZ(16146),	140,		/* D=cyl 140 thru 166 */
	HDSZ(165646),	167,		/* E=cyl 167 thru 443 */
	HDSZ(165646),	444,		/* F=cyl 444 thru 720 */
	HDSZ(706238),	140,		/* G=cyl 140 thru 1320 */
	HDSZ(358800),	721,		/* H=cyl 721 thru 1320 */
}, hd9134L_sizes[8] = {
	HDSZ(15920),	1,		/* A=cyl 1 thru 199 */
	HDSZ(20000),	200,		/* B=cyl 200 thru 449 */
	HDSZ(77840),	0,		/* C=cyl 0 thru 972 */
	HDSZ(32000),	200,		/* D=cyl 200 thru 599 */
	HDSZ(0),	0,		/* E=<undefined> */
	HDSZ(0),	0,		/* F=<undefined> */
	HDSZ(41840),	450,		/* G=cyl 450 thru 972 */
	HDSZ(29840),	600,		/* H=cyl 600 thru 972 */
}, hd7957A_sizes[8] = {
	HDSZ(16016),	1,		/* A=cyl 1 thru 104 */
	HDSZ(24640),	105,		/* B=cyl 105 thru 264 */
	HDSZ(159544),	0,		/* C=cyl 0 thru 1035 */
	HDSZ(42350),	105,		/* D=cyl 105 thru 379 */
	HDSZ(54824),	380,		/* E=cyl 380 thru 735 */
	HDSZ(46200),	736,		/* F=cyl 736 thru 1035 */
	HDSZ(118734),	265,		/* G=cyl 265 thru 1035 */
	HDSZ(101024),	380,		/* H=cyl 380 thru 1035 */
}, hd7958A_sizes[8] = {
	HDSZ(16128),	1,		/* A=cyl 1 thru 64 */
	HDSZ(32256),	65,		/* B=cyl 65 thru 192 */
	HDSZ(255276),	0,		/* C=cyl 0 thru 1012 */
	HDSZ(48384),	65,		/* D=cyl 65 thru 256 */
	HDSZ(100800),	257,		/* E=cyl 257 thru 656 */
	HDSZ(89712),	657,		/* F=cyl 657 thru 1012 */
	HDSZ(206640),	193,		/* G=cyl 193 thru 1012 */
	HDSZ(190512),	257,		/* H=cyl 257 thru 1012 */
}, hd7957B_sizes[8] = {
	HDSZ(16002),	1,		/* A=cyl 1 thru 127 */
	HDSZ(32760),	128,		/* B=cyl 128 thru 387 */
	HDSZ(159894),	0,		/* C=cyl 0 thru 1268 */
	HDSZ(49140),	128,		/* D=cyl 128 thru 517 */
	HDSZ(50400),	518,		/* E=cyl 518 thru 917 */
	HDSZ(44226),	918,		/* F=cyl 918 thru 1268 */
	HDSZ(111006),	388,		/* G=cyl 388 thru 1268 */
	HDSZ(94626),	518,		/* H=cyl 518 thru 1268 */
}, hd7958B_sizes[8] = {
	HDSZ(16254),	1,		/* A=cyl 1 thru 43 */
	HDSZ(32886),	44,		/* B=cyl 44 thru 130 */
	HDSZ(297108),	0,		/* C=cyl 0 thru 785 */
	HDSZ(49140),	44,		/* D=cyl 44 thru 173 */
	HDSZ(121716),	174,		/* E=cyl 174 thru 495 */
	HDSZ(109620),	496,		/* F=cyl 496 thru 785 */
	HDSZ(247590),	131,		/* G=cyl 131 thru 785 */
	HDSZ(231336),	174,		/* H=cyl 174 thru 785 */
}, hd7959B_sizes[8] = {
	HDSZ(16254),	1,		/* A=cyl 1 thru 43 */
	HDSZ(49140),	44,		/* B=cyl 44 thru 173 */
	HDSZ(594216),	0,		/* C=cyl 0 thru 1571 */
	HDSZ(65772),	44,		/* D=cyl 44 thru 217 */
	HDSZ(303912),	218,		/* E=cyl 218 thru 1021 */
	HDSZ(207900),	1022,		/* F=cyl 1022 thru 1571 */
	HDSZ(528444),	174,		/* G=cyl 174 thru 1571 */
	HDSZ(511812),	218,		/* H=cyl 218 thru 1571 */
}, hd2200A_sizes[8] = {
	HDSZ(16272),	1,		/* A=cyl 1 thru 36 */
	HDSZ(49720),	37,		/* B=cyl 37 thru 146 */
	HDSZ(654948),	0,		/* C=cyl 0 thru 1448 */
	HDSZ(65992),	37,		/* D=cyl 37 thru 182 */
	HDSZ(304648),	183,		/* E=cyl 183 thru 856 */
	HDSZ(267584),	857,		/* F=cyl 857 thru 1448 */
	HDSZ(588504),	147,		/* G=cyl 147 thru 1448 */
	HDSZ(572232),	183,		/* H=cyl 183 thru 1448 */
}, hd2203A_sizes[8] = {
	/* modelled after the 7937; i.e. bogus */
	HDSZ(16272),	1,		/* A=cyl 1 thru 18 */
	HDSZ(67800),	19,		/* B=cyl 19 thru 93 */
	HDSZ(1309896),	0,		/* C=cyl 0 thru 1448 */
	HDSZ(16272),	94,		/* D=cyl 19 thru 111 */
	HDSZ(305552),	112,		/* E=cyl 112 thru 449 */
	HDSZ(305552),	450,		/* F=cyl 450 thru 787 */
	HDSZ(1224920),	94,		/* G=cyl 94 thru 1448 */
	HDSZ(597544),	788,		/* H=cyl 788 thru 1448 */
}, hd7936H_sizes[8] = {
	HDSZ(16359),	1,		/* A=cyl 1 thru 19 */
	HDSZ(67158),	20,		/* B=cyl 20 thru 97 */
	HDSZ(600978),	0,		/* C=cyl 0 thru 697 */
	HDSZ(16359),	98,		/* D=cyl 98 thru 116 */
	HDSZ(120540),	117,		/* E=cyl 117 thru 256 */
	HDSZ(120540),	256,		/* F=cyl 256 thru 396 */
	HDSZ(516600),	98,		/* G=cyl 98 thru 697 */
	HDSZ(259161),	397,		/* H=cyl 397 thru 697 */
}, hd7937H_sizes[8] = {
	HDSZ(15990),	1,		/* A=cyl 1 thru 10 */
	HDSZ(67158),	11,		/* B=cyl 11 thru 52 */
	HDSZ(1116102),	0,		/* C=cyl 0 thru 697 */
	HDSZ(124722),	53,		/* D=cyl 53 thru 130 */
	HDSZ(163098),	131,		/* E=cyl 131 thru 232 */
	HDSZ(287820),	233,		/* F=cyl 233 thru 412 */
	HDSZ(1031355),	53,		/* G=cyl 53 thru 697 */
	HDSZ(455715),	413,		/* H=cyl 413 thru 697 */
};

/*
 * Indexed the same as hdidentinfo array.
 */
struct hdcompatinfo {
	struct	size *sizes;	/* partition info */
} hdcompatinfo[] = {
	hd7945A_sizes,
	hd9134D_sizes,
	hd9122S_sizes,
	hd7912P_sizes,
	hd7914P_sizes,
	hd7958A_sizes,
	hd7957A_sizes,
	hd7933H_sizes,
	hd9134L_sizes,
	hd7936H_sizes,
	hd7937H_sizes,
	hd7914P_sizes,
	hd7945A_sizes,
	hd9122S_sizes,
	hd7957B_sizes,
	hd7958B_sizes,
	hd7959B_sizes,
	hd2200A_sizes,
	hd2203A_sizes,
};
int nhdcompatinfo = sizeof(hdcompatinfo) / sizeof(hdcompatinfo[0]);

hdmakedisklabel(unit, lp)
	int unit;
	struct disklabel *lp;
{
	extern struct cfdriver hd_cd;
	register struct hd_softc *rs = hd_cd.cd_devs[unit];
	register struct hdcompatinfo *ci = &hdcompatinfo[rs->sc_type];
	struct hdidentinfo *ri = &hdidentinfo[rs->sc_type];
	register struct partition *pi;
	register int dcount;
	
	lp->d_nsectors = ri->ri_nbpt;
	lp->d_ntracks = ri->ri_ntpc;
	lp->d_ncylinders = ri->ri_ncyl;
	lp->d_secpercyl = ri->ri_nbpt * ri->ri_ntpc;
	lp->d_secperunit = lp->d_secpercyl * ri->ri_ncyl;
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_npartitions = 8;

	pi = lp->d_partitions;
	for (dcount = 0; dcount < lp->d_npartitions; dcount++) {
		pi->p_size = ci->sizes[dcount].nblocks;
		pi->p_offset = ci->sizes[dcount].cyloff * lp->d_secpercyl;
		pi->p_fsize = 1024;
		if (dcount == 1 || dcount == 3)
			pi->p_fstype = FS_SWAP;
		else if (dcount == 2)
			pi->p_fstype = FS_BOOT;
		else
			pi->p_fstype = FS_BSDFFS;
		pi->p_frag = 8;
		pi++;
	}
}
