/*	$NetBSD: disk.h,v 1.9 1996/01/07 22:04:07 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Jason R. Thorpe.  All rights reserved.
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
 * from: Header: disk.h,v 1.5 92/11/19 04:33:03 torek Exp  (LBL)
 *
 *	@(#)disk.h	8.1 (Berkeley) 6/2/93
 */

/*
 * Disk device structures.
 */

#include <sys/time.h>
#include <sys/queue.h>

struct buf;
struct disklabel;
struct cpu_disklabel;

struct disk {
	TAILQ_ENTRY(disk) dk_link;	/* link in global disklist */
	char		*dk_name;	/* disk name */
	int		dk_bopenmask;	/* block devices open */
	int		dk_copenmask;	/* character devices open */
	int		dk_openmask;	/* composite (bopen|copen) */
	int		dk_state;	/* label state   ### */
	int		dk_blkshift;	/* shift to convert DEV_BSIZE to blks */
	int		dk_byteshift;	/* shift to convert bytes to blks */

	/*
	 * Metrics data; note that some metrics may have no meaning
	 * on certain types of disks.
	 */
	int		dk_busy;	/* busy counter */
	u_int64_t	dk_xfer;	/* total number of transfers */
	u_int64_t	dk_seek;	/* total independent seek operations */
	u_int64_t	dk_bytes;	/* total bytes transfered */
	struct timeval	dk_attachtime;	/* time disk was attached */
	struct timeval	dk_timestamp;	/* timestamp of last unbusy */
	struct timeval	dk_time;	/* total time spent busy */

	struct	dkdriver *dk_driver;	/* pointer to driver */

	/*
	 * Disk label information.  Storage for the in-core disk label
	 * must be dynamically allocated, otherwise the size of this
	 * structure becomes machine-dependent.
	 */
	daddr_t		dk_labelsector;		/* sector containing label */
	struct disklabel *dk_label;	/* label */
	struct cpu_disklabel *dk_cpulabel;
};

struct dkdriver {
	void	(*d_strategy) __P((struct buf *));
#ifdef notyet
	int	(*d_open) __P((dev_t dev, int ifmt, int, struct proc *));
	int	(*d_close) __P((dev_t dev, int, int ifmt, struct proc *));
	int	(*d_ioctl) __P((dev_t dev, u_long cmd, caddr_t data, int fflag,
				struct proc *));
	int	(*d_dump) __P((dev_t));
	void	(*d_start) __P((struct buf *, daddr_t));
	int	(*d_mklabel) __P((struct disk *));
#endif
};

/* states */
#define	DK_CLOSED	0		/* drive is closed */
#define	DK_WANTOPEN	1		/* drive being opened */
#define	DK_WANTOPENRAW	2		/* drive being opened */
#define	DK_RDLABEL	3		/* label being read */
#define	DK_OPEN		4		/* label read, drive open */
#define	DK_OPENRAW	5		/* open without label */

#ifdef DISKSORT_STATS
/*
 * Stats from disksort().
 */
struct disksort_stats {
	long	ds_newhead;		/* # new queue heads created */
	long	ds_newtail;		/* # new queue tails created */
	long	ds_midfirst;		/* # insertions into sort list */
	long	ds_endfirst;		/* # insertions at end of sort list */
	long	ds_newsecond;		/* # inversions (2nd lists) created */
	long	ds_midsecond;		/* # insertions into 2nd list */
	long	ds_endsecond;		/* # insertions at end of 2nd list */
};
#endif

/*
 * disklist_head is defined here so that user-land has access to it.
 */
TAILQ_HEAD(disklist_head, disk);	/* the disklist is a TAILQ */

#ifdef _KERNEL
extern	int disk_count;			/* number of disks in global disklist */

void	disk_init __P((void));
void	disk_attach __P((struct disk *));
void	disk_detach __P((struct disk *));
void	disk_busy __P((struct disk *));
void	disk_unbusy __P((struct disk *, long));
void	disk_resetstat __P((struct disk *));
struct	disk *disk_find __P((char *));
#endif
