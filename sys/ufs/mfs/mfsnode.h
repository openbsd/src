/*	$OpenBSD: mfsnode.h,v 1.7 1998/08/06 19:35:11 csapuntz Exp $	*/
/*	$NetBSD: mfsnode.h,v 1.3 1996/02/09 22:31:31 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
 *	@(#)mfsnode.h	8.2 (Berkeley) 8/11/93
 */

/*
 * This structure defines the control data for the memory based file system.
 */

struct mfsnode {
	struct	vnode *mfs_vnode;	/* vnode associated with this mfsnode */
	caddr_t	mfs_baseoff;		/* base of file system in memory */
	long	mfs_size;		/* size of memory file system */
	pid_t	mfs_pid;		/* supporting process pid */
	struct	buf *mfs_buflist;	/* list of I/O requests */
	long	mfs_spare[4];
};

/*
 * Convert between mfsnode pointers and vnode pointers
 */
#define VTOMFS(vp)	((struct mfsnode *)(vp)->v_data)
#define MFSTOV(mfsp)	((mfsp)->mfs_vnode)

/* Prototypes for MFS operations on vnodes. */
#define	mfs_lookup	mfs_badop
#define	mfs_create	mfs_badop
#define	mfs_mknod	mfs_badop
#define	mfs_access	mfs_badop
#define	mfs_getattr	mfs_badop
#define	mfs_setattr	mfs_badop
#define	mfs_read	mfs_badop
#define	mfs_write	mfs_badop
#define	mfs_select	mfs_badop
#define	mfs_mmap	mfs_badop
#define	mfs_seek	mfs_badop
#define	mfs_remove	mfs_badop
#define	mfs_link	mfs_badop
#define	mfs_rename	mfs_badop
#define	mfs_mkdir	mfs_badop
#define	mfs_rmdir	mfs_badop
#define	mfs_symlink	mfs_badop
#define	mfs_readdir	mfs_badop
#define	mfs_readlink	mfs_badop
#define	mfs_abortop	mfs_badop
#define	mfs_lock	vop_generic_lock
#define	mfs_unlock	vop_generic_unlock
#define	mfs_islocked	vop_generic_islocked
#define	mfs_pathconf	mfs_badop
#define	mfs_advlock	mfs_badop
#define	mfs_blkatoff	mfs_badop
#define	mfs_valloc	mfs_badop
#define	mfs_vfree	mfs_badop
#define	mfs_truncate	mfs_badop
#define	mfs_update	nullop
#define	mfs_bwrite	vop_generic_bwrite
