/*	$OpenBSD: hdvar.h,v 1.8 2005/11/18 00:09:15 miod Exp $	*/
/*	$NetBSD: rdvar.h,v 1.6 1997/01/30 09:14:19 thorpej Exp $	*/

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
 * from: Utah $Hdr: rdvar.h 1.1 92/12/21$
 *
 *	@(#)rdvar.h	8.1 (Berkeley) 6/10/93
 */

struct	hdidentinfo {
	short	ri_hwid;		/* 2 byte HW id */
	short	ri_maxunum;		/* maximum allowed unit number */
	char	*ri_desc;		/* drive type description */
	int	ri_nbpt;		/* DEV_BSIZE blocks per track */
	int	ri_ntpc;		/* tracks per cylinder */
	int	ri_ncyl;		/* cylinders per unit */
	int	ri_nblocks;		/* DEV_BSIZE blocks on disk */
};

struct hdstats {
	long	hdretries;
	long	hdresets;
	long	hdtimeouts;
	long	hdpolltries;
	long	hdpollwaits;
};

struct	hd_softc {
	struct	device sc_dev;
	struct	disk sc_dkdev;
	int	sc_slave;		/* HP-IB slave */
	int	sc_punit;		/* physical unit on slave */
	int	sc_flags;
	short	sc_type;
	char	*sc_addr;
	int	sc_resid;
	struct	hpibqueue sc_hq;	/* hpib job queue entry */
	struct	hd_iocmd sc_ioc;
	struct	hd_rscmd sc_rsc;
	struct	hd_stat sc_stat;
	struct	hd_ssmcmd sc_ssmc;
	struct	hd_srcmd sc_src;
	struct	hd_clearcmd sc_clear;
	struct	buf sc_tab;		/* buffer queue */
	struct	hdstats sc_stats;
	struct	timeout sc_timeout;
};

/* sc_flags values */
#define	HDF_ALIVE	0x01
#define	HDF_SEEK	0x02
#define HDF_SWAIT	0x04
#define HDF_OPENING	0x08
#define HDF_CLOSING	0x10
#define HDF_WANTED	0x20
#define HDF_WLABEL	0x40

#ifdef _KERNEL
extern	const struct hdidentinfo hdidentinfo[];
#endif
