/*	$OpenBSD: conf.h,v 1.83 2007/07/23 13:30:21 mk Exp $	*/
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
 *	@(#)conf.h	8.3 (Berkeley) 1/21/94
 */


#ifndef _SYS_CONF_H_
#define _SYS_CONF_H_

/*
 * Definitions of device driver entry switches
 */

struct buf;
struct proc;
struct tty;
struct uio;
struct vnode;
struct knote;

/*
 * Types for d_type
 */
#define	D_TAPE	1
#define	D_DISK	2
#define	D_TTY	3

/*
 * Flags for d_flags
 */
#define D_KQFILTER	0x0001		/* has kqfilter entry */
#define D_CLONE		0x0002		/* clone upon open */

#ifdef _KERNEL

#define	dev_type_open(n)	int n(dev_t, int, int, struct proc *)
#define	dev_type_close(n)	int n(dev_t, int, int, struct proc *)
#define	dev_type_strategy(n)	void n(struct buf *)
#define	dev_type_ioctl(n) \
	int n(dev_t, u_long, caddr_t, int, struct proc *)

#define	dev_decl(n,t)	__CONCAT(dev_type_,t)(__CONCAT(n,t))
#define	dev_init(c,n,t) \
	((c) > 0 ? __CONCAT(n,t) : (__CONCAT(dev_type_,t)((*))) enxio)

#endif /* _KERNEL */

/*
 * Block device switch table
 */
struct bdevsw {
	int	(*d_open)(dev_t dev, int oflags, int devtype,
				     struct proc *p);
	int	(*d_close)(dev_t dev, int fflag, int devtype,
				     struct proc *p);
	void	(*d_strategy)(struct buf *bp);
	int	(*d_ioctl)(dev_t dev, u_long cmd, caddr_t data,
				     int fflag, struct proc *p);
	int	(*d_dump)(dev_t dev, daddr64_t blkno, caddr_t va,
				    size_t size);
	daddr64_t (*d_psize)(dev_t dev);
	u_int	d_type;
	/* u_int	d_flags; */
};

#ifdef _KERNEL

extern struct bdevsw bdevsw[];

/* bdevsw-specific types */
#define	dev_type_dump(n)	int n(dev_t, daddr64_t, caddr_t, size_t)
#define	dev_type_size(n)	daddr64_t n(dev_t)

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
	int	(*d_open)(dev_t dev, int oflags, int devtype,
				     struct proc *p);
	int	(*d_close)(dev_t dev, int fflag, int devtype,
				     struct proc *);
	int	(*d_read)(dev_t dev, struct uio *uio, int ioflag);
	int	(*d_write)(dev_t dev, struct uio *uio, int ioflag);
	int	(*d_ioctl)(dev_t dev, u_long cmd, caddr_t data,
				     int fflag, struct proc *p);
	int	(*d_stop)(struct tty *tp, int rw);
	struct tty *
		(*d_tty)(dev_t dev);
	int	(*d_poll)(dev_t dev, int events, struct proc *p);
	paddr_t	(*d_mmap)(dev_t, off_t, int);
	u_int	d_type;
	u_int	d_flags;
	int	(*d_kqfilter)(dev_t dev, struct knote *kn);
};

#ifdef _KERNEL

extern struct cdevsw cdevsw[];

/* cdevsw-specific types */
#define	dev_type_read(n)	int n(dev_t, struct uio *, int)
#define	dev_type_write(n)	int n(dev_t, struct uio *, int)
#define	dev_type_stop(n)	int n(struct tty *, int)
#define	dev_type_tty(n)		struct tty *n(dev_t)
#define	dev_type_poll(n)	int n(dev_t, int, struct proc *)
#define	dev_type_mmap(n)	paddr_t n(dev_t, off_t, int)
#define dev_type_kqfilter(n)	int n(dev_t, struct knote *)

#define	cdev_decl(n) \
	dev_decl(n,open); dev_decl(n,close); dev_decl(n,read); \
	dev_decl(n,write); dev_decl(n,ioctl); dev_decl(n,stop); \
	dev_decl(n,tty); dev_decl(n,poll); dev_decl(n,mmap); \
	dev_decl(n,kqfilter)

/* open, close, read, write, ioctl */
#define	cdev_disk_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev, D_DISK, 0 }

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
	0, seltrue, (dev_type_mmap((*))) enodev }

/* open, close, read, write, ioctl, stop, tty */
#define	cdev_tty_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	dev_init(c,n,tty), ttpoll, (dev_type_mmap((*))) enodev, \
	D_TTY, D_KQFILTER, ttkqfilter }

/* open, close, read, ioctl, poll, nokqfilter */
#define	cdev_mouse_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,poll), \
	(dev_type_mmap((*))) enodev }

/* open, close, read, write, ioctl, poll, nokqfilter */
#define	cdev_mousewr_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,poll), \
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

/* open, close, read, write, ioctl, poll, kqfilter -- XXX should be a tty */
#define	cdev_cn_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	0, dev_init(c,n,poll), (dev_type_mmap((*))) enodev, \
	D_TTY, D_KQFILTER, dev_init(c,n,kqfilter) }

/* open, read, write, ioctl, poll, kqfilter -- XXX should be a tty */
#define cdev_ctty_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) nullop, dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
	0, dev_init(c,n,poll), (dev_type_mmap((*))) enodev, \
	D_TTY, D_KQFILTER, ttkqfilter }

/* open, close, read, write, ioctl, mmap */
#define cdev_mm_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, seltrue, dev_init(c,n,mmap) }

/* open, close, read, write, ioctl, mmap */
#define cdev_crypto_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, (dev_type_poll((*))) enodev, (dev_type_mmap((*))) enodev }

/* open, close, read, write, ioctl */
#define cdev_systrace_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, (dev_type_poll((*))) enodev, (dev_type_mmap((*))) enodev }

/* read, write */
#define cdev_swap_init(c,n) { \
	(dev_type_open((*))) nullop, (dev_type_close((*))) nullop, \
	dev_init(c,n,read), dev_init(c,n,write), (dev_type_ioctl((*))) enodev, \
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, read, write, ioctl, tty, poll, kqfilter */
#define cdev_ptc_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
	dev_init(c,n,tty), dev_init(c,n,poll), (dev_type_mmap((*))) enodev, \
	D_TTY, D_KQFILTER, dev_init(c,n,kqfilter) }

/* open, close, read, write, ioctl, mmap */
#define cdev_ptm_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, (dev_type_poll((*))) enodev, (dev_type_mmap((*))) enodev }

/* open, close, read, ioctl, poll, kqfilter XXX should be a generic device */
#define cdev_log_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,poll), \
	(dev_type_mmap((*))) enodev, 0, D_KQFILTER, dev_init(c,n,kqfilter) }

/* open */
#define cdev_fd_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) enodev, \
	(dev_type_read((*))) enodev, (dev_type_write((*))) enodev, \
	(dev_type_ioctl((*))) enodev, (dev_type_stop((*))) enodev, \
	0, (dev_type_poll((*))) enodev, (dev_type_mmap((*))) enodev }

/* open, close, read, write, ioctl, poll, kqfilter -- XXX should be generic device */
#define cdev_bpftun_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, dev_init(c,n,poll), (dev_type_mmap((*))) enodev, \
	0, D_KQFILTER, dev_init(c,n,kqfilter) }

/* open, close, ioctl */
#define	cdev_lkm_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, ioctl */
#define	cdev_ch_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, ioctl */
#define       cdev_uk_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
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
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
	dev_init(c,n,mmap) }

/* open, close, read, write, ioctl, poll, kqfilter */
#define cdev_audio_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,poll), \
	dev_init(c,n,mmap), 0, D_KQFILTER, dev_init(c,n,kqfilter) }

/* open, close, read, write, ioctl, poll, nokqfilter */
#define cdev_midi_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,poll), \
	(dev_type_mmap((*))) enodev }

#define	cdev_svr4_net_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) enodev, \
	(dev_type_read((*))) enodev, (dev_type_write((*))) enodev, \
	(dev_type_ioctl((*))) enodev, (dev_type_stop((*))) nullop, \
	0, (dev_type_poll((*))) enodev, (dev_type_mmap((*))) enodev }

/* open, close, read, write, ioctl, poll, nokqfilter */
#define cdev_xfs_init(c, n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,poll), \
	(dev_type_mmap((*))) enodev }

/* open, close, read */
#define cdev_ksyms_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, (dev_type_ioctl((*))) enodev, \
	(dev_type_stop((*))) enodev, 0, seltrue, \
	(dev_type_mmap((*))) enodev, 0 }

/* open, close, read, write, ioctl, stop, tty, poll, mmap, kqfilter */
#define	cdev_wsdisplay_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	dev_init(c,n,tty), ttpoll, dev_init(c,n,mmap), \
	0, D_KQFILTER, dev_init(c,n,kqfilter) }

/* open, close, read, write, ioctl, poll, kqfilter */
#define	cdev_random_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, dev_init(c,n,poll), (dev_type_mmap((*))) enodev, \
	0, D_KQFILTER, dev_init(c,n,kqfilter) }
void	randomattach(void);

/* open, close, ioctl, poll, nokqfilter */
#define	cdev_usb_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,poll), \
	(dev_type_mmap((*))) enodev }

/* open, close, write, ioctl */
#define cdev_ulpt_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, (dev_type_poll((*))) enodev, (dev_type_mmap((*))) enodev }

/* open, close, ioctl */
#define cdev_pf_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, read, write, ioctl, poll, nokqfilter */
#define	cdev_urio_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, dev_init(c,n,poll), (dev_type_mmap((*))) enodev }

/* open, close, read, write, ioctl, poll, kqfilter */
#define	cdev_usbdev_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, dev_init(c,n,poll), (dev_type_mmap((*))) enodev, 0, D_KQFILTER, \
	dev_init(c,n,kqfilter) }

/* open, close, init */
#define cdev_pci_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, init */
#define cdev_iop_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, ioctl */
#define cdev_radio_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, write, ioctl */
#define cdev_spkr_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev }

/* open, close, write, ioctl */
#define cdev_lpt_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	0, seltrue, (dev_type_mmap((*))) enodev }

/* open, close, read, ioctl, mmap */
#define cdev_bktr_init(c, n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, seltrue, \
	dev_init(c,n,mmap) }

/* open, close, read, ioctl, poll, kqfilter */
#define cdev_hotplug_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, dev_init(c,n,poll), \
	(dev_type_mmap((*))) enodev, 0, D_KQFILTER, dev_init(c,n,kqfilter) }

/* open, close, ioctl */
#define cdev_gpio_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, ioctl */
#define       cdev_bio_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
	(dev_type_mmap((*))) enodev }

/* open, close, ioctl */
#define       cdev_bthub_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_poll((*))) enodev, \
	(dev_type_mmap((*))) enodev }

#endif

/*
 * Line discipline switch table
 */
struct linesw {
	int	(*l_open)(dev_t dev, struct tty *tp);
	int	(*l_close)(struct tty *tp, int flags);
	int	(*l_read)(struct tty *tp, struct uio *uio,
				     int flag);
	int	(*l_write)(struct tty *tp, struct uio *uio,
				     int flag);
	int	(*l_ioctl)(struct tty *tp, u_long cmd, caddr_t data,
				     int flag, struct proc *p);
	int	(*l_rint)(int c, struct tty *tp);
	int	(*l_start)(struct tty *tp);
	int	(*l_modem)(struct tty *tp, int flag);
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
	struct	vnode *sw_vp;
};
#define	SW_FREED	0x01
#define	SW_SEQUENTIAL	0x02
#define	sw_freed	sw_flags	/* XXX compat */

#ifdef _KERNEL
extern struct swdevt swdevt[];
extern int chrtoblktbl[];
extern int nchrtoblktbl;

struct bdevsw *bdevsw_lookup(dev_t);
struct cdevsw *cdevsw_lookup(dev_t);
int	chrtoblk(dev_t);
int	blktochr(dev_t);
int	iskmemdev(dev_t);
int	iszerodev(dev_t);
dev_t	getnulldev(void);

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

cdev_decl(ptm);

cdev_decl(ctty);

cdev_decl(audio);
cdev_decl(midi);
cdev_decl(sequencer);
cdev_decl(radio);
cdev_decl(cn);

bdev_decl(sw);
cdev_decl(sw);

bdev_decl(vnd);
cdev_decl(vnd);

bdev_decl(ccd);
cdev_decl(ccd);

bdev_decl(raid);
cdev_decl(raid);

cdev_decl(iop);

cdev_decl(ch);

cdev_decl(ss);

bdev_decl(sd);
cdev_decl(sd);

cdev_decl(ses);

bdev_decl(st);
cdev_decl(st);

bdev_decl(cd);
cdev_decl(cd);

bdev_decl(rd);
cdev_decl(rd);

bdev_decl(uk);
cdev_decl(uk);

cdev_decl(bpf);

cdev_decl(pf);

cdev_decl(tun);

cdev_decl(random);

cdev_decl(wsdisplay);
cdev_decl(wskbd);
cdev_decl(wsmouse);
cdev_decl(wsmux);

#ifdef COMPAT_SVR4
# define NSVR4_NET	1
#else
# define NSVR4_NET	0
#endif
cdev_decl(svr4_net);

cdev_decl(ksyms);

cdev_decl(crypto);

cdev_decl(systrace);

cdev_decl(bio);
cdev_decl(bthub);

cdev_decl(gpr);
cdev_decl(bktr);

cdev_decl(usb);
cdev_decl(ugen);
cdev_decl(uhid);
cdev_decl(ucom);
cdev_decl(ulpt);
cdev_decl(uscanner);
cdev_decl(urio);

cdev_decl(hotplug);
cdev_decl(gpio);

#endif

#endif /* _SYS_CONF_H_ */
