/*	$OpenBSD: conf.h,v 1.27 1998/08/30 17:11:33 art Exp $	*/
/*	$NetBSD: conf.h,v 1.33 1996/05/03 20:03:32 christos Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)conf.h	8.3 (Berkeley) 1/21/94
 */

/*
 * Definitions of device driver entry switches
 */

struct buf;
struct proc;
struct tty;
struct uio;
struct vnode;

/*
 * Types for d_type
 */
#define	D_TAPE	1
#define	D_DISK	2
#define	D_TTY	3

#ifdef _KERNEL

#define	dev_type_open(n)	int n __P((dev_t, int, int, struct proc *))
#define	dev_type_close(n)	int n __P((dev_t, int, int, struct proc *))
#define	dev_type_strategy(n)	void n __P((struct buf *))
#define	dev_type_ioctl(n) \
	int n __P((dev_t, u_long, caddr_t, int, struct proc *))

#define	dev_decl(n,t)	__CONCAT(dev_type_,t)(__CONCAT(n,t))
#define	dev_init(c,n,t) \
	((c) > 0 ? __CONCAT(n,t) : (__CONCAT(dev_type_,t)((*))) enxio)

#endif /* _KERNEL */

/*
 * Block device switch table
 */
struct bdevsw {
	int	(*d_open)	__P((dev_t dev, int oflags, int devtype,
				     struct proc *p));
	int	(*d_close)	__P((dev_t dev, int fflag, int devtype,
				     struct proc *p));
	void	(*d_strategy)	__P((struct buf *bp));
	int	(*d_ioctl)	__P((dev_t dev, u_long cmd, caddr_t data,
				     int fflag, struct proc *p));
#ifndef __BDEVSW_DUMP_OLD_TYPE
	int	(*d_dump)	__P((dev_t dev, daddr_t blkno, caddr_t va,
				    size_t size));
#else /* not __BDEVSW_DUMP_OLD_TYPE */
	int	(*d_dump)	();	/* parameters vary by architecture */
#endif /* __BDEVSW_DUMP_OLD_TYPE */
	int	(*d_psize)	__P((dev_t dev));
	int	d_type;
};

#ifdef _KERNEL

extern struct bdevsw bdevsw[];

/* bdevsw-specific types */
#ifndef __BDEVSW_DUMP_OLD_TYPE
#define	dev_type_dump(n)	int n __P((dev_t, daddr_t, caddr_t, size_t))
#else /* not __BDEVSW_DUMP_OLD_TYPE */
#define	dev_type_dump(n)	int n()	/* parameters vary by architecture */
#endif /* __BDEVSW_DUMP_OLD_TYPE */
#define	dev_type_size(n)	int n __P((dev_t))

/* bdevsw-specific initializations */
#define	dev_size_init(c,n)	(c > 0 ? __CONCAT(n,size) : 0)

#define	bdev_decl(n) \
	dev_decl(n,open); dev_decl(n,close); dev_decl(n,strategy); \
	dev_decl(n,ioctl); dev_decl(n,dump); dev_decl(n,size)

#define	bdev_disk_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), \
	dev_init(c,n,strategy), dev_init(c,n,ioctl), \
	dev_init(c,n,dump), dev_size_init(c,n), D_DISK }

#define	bdev_tape_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), \
	dev_init(c,n,strategy), dev_init(c,n,ioctl), \
	dev_init(c,n,dump), 0, D_TAPE }

#define	bdev_swap_init(c,n) { \
	(dev_type_open((*))) enodev, (dev_type_close((*))) enodev, \
	dev_init(c,n,strategy), (dev_type_ioctl((*))) enodev, \
	(dev_type_dump((*))) enodev, 0 }

#define	bdev_lkm_dummy() { \
	(dev_type_open((*))) lkmenodev, (dev_type_close((*))) enodev, \
	(dev_type_strategy((*))) enodev, (dev_type_ioctl((*))) enodev, \
	(dev_type_dump((*))) enodev, 0 }

#define	bdev_notdef() { \
	(dev_type_open((*))) enodev, (dev_type_close((*))) enodev, \
	(dev_type_strategy((*))) enodev, (dev_type_ioctl((*))) enodev, \
	(dev_type_dump((*))) enodev, 0 }

#endif

/*
 * Character device switch table
 */
struct cdevsw {
	int	(*d_open)	__P((dev_t dev, int oflags, int devtype,
				     struct proc *p));
	int	(*d_close)	__P((dev_t dev, int fflag, int devtype,
				     struct proc *));
	int	(*d_read)	__P((dev_t dev, struct uio *uio, int ioflag));
	int	(*d_write)	__P((dev_t dev, struct uio *uio, int ioflag));
	int	(*d_ioctl)	__P((dev_t dev, u_long cmd, caddr_t data,
				     int fflag, struct proc *p));
	int	(*d_stop)	__P((struct tty *tp, int rw));
	struct tty *
		(*d_tty)	__P((dev_t dev));
	int	(*d_select)	__P((dev_t dev, int which, struct proc *p));
	int	(*d_mmap)	__P((dev_t, int, int));
	int	d_type;
};

#ifdef _KERNEL

extern struct cdevsw cdevsw[];

/* cdevsw-specific types */
#define	dev_type_read(n)	int n __P((dev_t, struct uio *, int))
#define	dev_type_write(n)	int n __P((dev_t, struct uio *, int))
#define	dev_type_stop(n)	int n __P((struct tty *, int))
#define	dev_type_tty(n)		struct tty *n __P((dev_t))
#define	dev_type_select(n)	int n __P((dev_t, int, struct proc *))
#define	dev_type_mmap(n)	int n __P((dev_t, int, int))

#define	cdev_decl(n) \
	dev_decl(n,open); dev_decl(n,close); dev_decl(n,read); \
	dev_decl(n,write); dev_decl(n,ioctl); dev_decl(n,stop); \
	dev_decl(n,tty); dev_decl(n,select); dev_decl(n,mmap)

/* open, close, read, write, ioctl */
#define	cdev_disk_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev, D_DISK }

/* open, close, read, write, ioctl */
#define	cdev_tape_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev, D_TAPE }

/* open, close, read, ioctl */
#define cdev_scanner_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) nullop, \
	0, seltrue, (dev_type_mmap((*))) enodev, 0 }

/* open, close, read, write, ioctl, stop, tty */
#define	cdev_tty_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	dev_init(c,n,tty), ttselect, (dev_type_mmap((*))) enodev, D_TTY }

/* open, close, read, ioctl, select */
#define	cdev_mouse_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,select), \
	(dev_type_mmap((*))) enodev }

/* open, close, read, write, ioctl, select */
#define	cdev_mousewr_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,select), \
	(dev_type_mmap((*))) enodev }

#define	cdev_lkm_dummy() { \
	(dev_type_open((*))) lkmenodev, (dev_type_close((*))) enodev, \
	(dev_type_read((*))) enodev, (dev_type_write((*))) enodev, \
	(dev_type_ioctl((*))) enodev, (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev }

#define	cdev_notdef() { \
	(dev_type_open((*))) enodev, (dev_type_close((*))) enodev, \
	(dev_type_read((*))) enodev, (dev_type_write((*))) enodev, \
	(dev_type_ioctl((*))) enodev, (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev }

/* open, close, read, write, ioctl, select -- XXX should be a tty */
#define	cdev_cn_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	0, dev_init(c,n,select), (dev_type_mmap((*))) enodev, D_TTY }

/* open, read, write, ioctl, select -- XXX should be a tty */
#define cdev_ctty_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) nullop, dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
	0, dev_init(c,n,select), (dev_type_mmap((*))) enodev, D_TTY }

/* open, close, read, write, mmap */
#define cdev_mm_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), (dev_type_ioctl((*))) enodev, \
	(dev_type_stop((*))) enodev, 0, seltrue, dev_init(c,n,mmap) }

/* read, write */
#define cdev_swap_init(c,n) { \
	(dev_type_open((*))) nullop, (dev_type_close((*))) nullop, \
	dev_init(c,n,read), dev_init(c,n,write), (dev_type_ioctl((*))) enodev, \
	(dev_type_stop((*))) enodev, 0, (dev_type_select((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, read, write, ioctl, tty, select */
#define cdev_ptc_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
	dev_init(c,n,tty), dev_init(c,n,select), (dev_type_mmap((*))) enodev, \
	D_TTY }

/* open, close, read, ioctl, select -- XXX should be a generic device */
#define cdev_log_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,select), \
	(dev_type_mmap((*))) enodev }

/* open */
#define cdev_fd_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) enodev, \
	(dev_type_read((*))) enodev, (dev_type_write((*))) enodev, \
	(dev_type_ioctl((*))) enodev, (dev_type_stop((*))) enodev, \
	0, (dev_type_select((*))) enodev, (dev_type_mmap((*))) enodev }

/* open, close, read, write, ioctl, select -- XXX should be generic device */
#define cdev_bpftun_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, dev_init(c,n,select), (dev_type_mmap((*))) enodev }

/* open, close, ioctl */
#define	cdev_lkm_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_select((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, ioctl */
#define	cdev_ch_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_select((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, ioctl */
#define       cdev_uk_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_select((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, read, ioctl */
#define cdev_ss_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, seltrue, \
	(dev_type_mmap((*))) enodev }

/* open, close, ioctl, mmap */
#define	cdev_fb_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_select((*))) enodev, \
	dev_init(c,n,mmap) }

/* open, close, read, write, ioctl */
#define cdev_audio_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, dev_init(c,n,select), dev_init(c,n,mmap) }

#define	cdev_svr4_net_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) enodev, \
	(dev_type_read((*))) enodev, (dev_type_write((*))) enodev, \
	(dev_type_ioctl((*))) enodev, (dev_type_stop((*))) nullop, \
	0, (dev_type_select((*))) enodev, (dev_type_mmap((*))) enodev }

/* open, close, read, ioctl */
#define	cdev_gen_ipf(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_select((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, read, write, ioctl, select */
#define cdev_xfs_init(c, n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,select), \
	(dev_type_mmap((*))) enodev }

/* open, close, read */
#define cdev_ksyms_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, (dev_type_ioctl((*))) enodev, \
	(dev_type_stop((*))) enodev, 0, seltrue, \
	(dev_type_mmap((*))) enodev, 0 }

/* open, close, read, write, ioctl, select */
#define	cdev_random_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, dev_init(c,n,select), (dev_type_mmap((*))) enodev }
void	randomattach __P((void));

/* symbolic sleep message strings */
extern char devopn[], devio[], devwait[], devin[], devout[];
extern char devioc[], devcls[];

#endif

/*
 * Line discipline switch table
 */
struct linesw {
	int	(*l_open)	__P((dev_t dev, struct tty *tp));
	int	(*l_close)	__P((struct tty *tp, int flags));
	int	(*l_read)	__P((struct tty *tp, struct uio *uio,
				     int flag));
	int	(*l_write)	__P((struct tty *tp, struct uio *uio,
				     int flag));
	int	(*l_ioctl)	__P((struct tty *tp, u_long cmd, caddr_t data,
				     int flag, struct proc *p));
	int	(*l_rint)	__P((int c, struct tty *tp));
	int	(*l_start)	__P((struct tty *tp));
	int	(*l_modem)	__P((struct tty *tp, int flag));
};

#ifdef _KERNEL
extern struct linesw linesw[];
#endif

/*
 * Swap device table
 */
struct swdevt {
	dev_t	sw_dev;
	int	sw_flags;
	int	sw_nblks;
	struct	vnode *sw_vp;
};
#define	SW_FREED	0x01
#define	SW_SEQUENTIAL	0x02
#define	sw_freed	sw_flags	/* XXX compat */

#ifdef _KERNEL
extern struct swdevt swdevt[];

int	chrtoblk __P((dev_t));
int	blktochr __P((dev_t));
int	iskmemdev __P((dev_t));
int	iszerodev __P((dev_t));

cdev_decl(filedesc);

cdev_decl(log);

#ifndef LKM
# define	NLKM	0
# define	lkmenodev	enodev
#else
# define	NLKM	1
#endif
cdev_decl(lkm);

#define	ptstty		ptytty
#define	ptsioctl	ptyioctl
cdev_decl(pts);

#define	ptctty		ptytty
#define	ptcioctl	ptyioctl
cdev_decl(ptc);

cdev_decl(ctty);

cdev_decl(audio);

cdev_decl(cn);

bdev_decl(vnd);
cdev_decl(vnd);

bdev_decl(ccd);
cdev_decl(ccd);

cdev_decl(ch);

cdev_decl(ss);

bdev_decl(sd);
cdev_decl(sd);

bdev_decl(st);
cdev_decl(st);

bdev_decl(cd);
cdev_decl(cd);

bdev_decl(rd);
cdev_decl(rd);

bdev_decl(uk);
cdev_decl(uk);

cdev_decl(bpf);

cdev_decl(tun);

cdev_decl(random);

cdev_decl(ipl);

#ifdef COMPAT_SVR4
# define NSVR4_NET	1
#else
# define NSVR4_NET	0
#endif
cdev_decl(svr4_net);

#endif
