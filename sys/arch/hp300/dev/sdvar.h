/*	$OpenBSD: sdvar.h,v 1.9 1998/10/04 01:02:25 millert Exp $	*/
/*	$NetBSD: sdvar.h,v 1.7 1997/03/31 07:40:07 scottr Exp $	*/

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 *	@(#)sdvar.h	8.1 (Berkeley) 6/10/93
 */

struct sdstats {
	long	sdresets;
	long	sdtransfers;
	long	sdpartials;
};

struct	sd_softc {
	struct	device sc_dev;
	struct	disk sc_dkdev;
	struct	scsiqueue sc_sq;
	int	sc_format_pid;	/* process using "format" mode */
	short	sc_flags;
	short	sc_type;	/* drive type */
	int	sc_target;	/* SCSI target */
	int	sc_lun;		/* SCSI lun */
	u_short	sc_bshift;	/* convert device blocks to DEV_BSIZE blks */
	u_int	sc_blks;	/* number of blocks on device */
	int	sc_blksize;	/* device block size in bytes */
	u_int	sc_heads;	/* number of heads (tracks) */
	u_int	sc_cyls;	/* number of cylinders */
	struct buf sc_tab;	/* buffer queue */
	struct sdstats sc_stats; /* debugging stats */
	struct scsi_fmt_cdb sc_cmdstore;
	struct scsi_fmt_sense sc_sensestore;
};

/* sc_flags values */
#define	SDF_ALIVE	0x01
#define SDF_OPENING	0x02
#define SDF_CLOSING	0x04
#define SDF_WANTED	0x08
#define SDF_WLABEL	0x10
#define SDF_RMEDIA	0x20
#define SDF_ERROR	0x40

#define	sdunit(x)	DISKUNIT(x)
#define sdpart(x)	DISKPART(x)
#define sdlabdev(d)	MAKEDISKDEV(major(d), sdunit(d), RAW_PART)

#define	b_cylin		b_resid

#define	SDRETRY		2

#ifdef _KERNEL
/* sd.c */
void	sdustart __P((int));

void	sdstart __P((void *));
void	sdgo __P((void *));
void	sdintr __P((void *, int));

int	sdgetcapacity __P((struct sd_softc *, dev_t));
int	sdgetinfo __P((dev_t, struct sd_softc *, struct disklabel *, int));

/* sd_compat.c */
void	sdmakedisklabel __P((int, struct disklabel *));
#endif /* _KERNEL */
