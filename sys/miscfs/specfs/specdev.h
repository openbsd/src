/*	$OpenBSD: specdev.h,v 1.20 2007/06/02 00:45:21 thib Exp $	*/
/*	$NetBSD: specdev.h,v 1.12 1996/02/13 13:13:01 mycroft Exp $	*/

/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)specdev.h	8.3 (Berkeley) 8/10/94
 */

/*
 * This structure defines the information maintained about
 * special devices. It is allocated in checkalias and freed
 * in vgone.
 */
struct specinfo {
	struct	vnode **si_hashchain;
	struct	vnode *si_specnext;
	struct  mount *si_mountpoint;
	dev_t	si_rdev;
	struct	lockf *si_lockf;
	daddr64_t si_lastr;
	union {
		struct vnode *ci_parent; /* pointer back to parent device */
		u_int8_t ci_bitmap[8]; /* bitmap of devices cloned off us */
	} si_ci;
};

struct cloneinfo {
	struct vnode *ci_vp; /* cloned vnode */
	void *ci_data; /* original vnode's v_data */
};

/*
 * Exported shorthand
 */
#define v_rdev v_specinfo->si_rdev
#define v_hashchain v_specinfo->si_hashchain
#define v_specnext v_specinfo->si_specnext
#define v_specmountpoint v_specinfo->si_mountpoint
#define v_speclockf v_specinfo->si_lockf
#define v_specparent v_specinfo->si_ci.ci_parent
#define v_specbitmap v_specinfo->si_ci.ci_bitmap

/*
 * Special device management
 */
#define	SPECHSZ	64
#if	((SPECHSZ&(SPECHSZ-1)) == 0)
#define	SPECHASH(rdev)	(((rdev>>5)+(rdev))&(SPECHSZ-1))
#else
#define	SPECHASH(rdev)	(((unsigned)((rdev>>5)+(rdev)))%SPECHSZ)
#endif

extern struct vnode *speclisth[SPECHSZ];

/*
 * Prototypes for special file operations on vnodes.
 */
extern	int (**spec_vnodeop_p)(void *);
struct	nameidata;
struct	componentname;
struct	ucred;
struct	flock;
struct	buf;
struct	uio;

int	spec_badop(void *);
int	spec_ebadf(void *);

int	spec_lookup(void *);
#define	spec_create	spec_badop
#define	spec_mknod	spec_badop
int	spec_open(void *);
int	spec_close(void *);
#define	spec_access	spec_ebadf
#define	spec_getattr	spec_ebadf
#define	spec_setattr	spec_ebadf
int	spec_read(void *);
int	spec_write(void *);
int	spec_ioctl(void *);
int	spec_poll(void *);
int	spec_kqfilter(void *);
int	spec_fsync(void *);
#define	spec_remove	spec_badop
#define	spec_link	spec_badop
#define	spec_rename	spec_badop
#define	spec_mkdir	spec_badop
#define	spec_rmdir	spec_badop
#define	spec_symlink	spec_badop
#define	spec_readdir	spec_badop
#define	spec_readlink	spec_badop
#define	spec_abortop	spec_badop
int spec_inactive(void *);
#define	spec_reclaim	nullop
#define spec_lock       vop_generic_lock
#define spec_unlock     vop_generic_unlock
#define spec_islocked   vop_generic_islocked
int	spec_bmap(void *);
int	spec_strategy(void *);
int	spec_print(void *);
int	spec_pathconf(void *);
int	spec_advlock(void *);
#define	spec_reallocblks spec_badop
#define	spec_bwrite	vop_generic_bwrite
#define spec_revoke     vop_generic_revoke

int	spec_vnoperate(void *);

/* spec_subr.c */
int	spec_open_clone(struct vop_open_args *);
int	spec_close_clone(struct vop_close_args *);
