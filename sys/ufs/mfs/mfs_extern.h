/*	$OpenBSD: mfs_extern.h,v 1.8 2000/02/07 04:57:18 assar Exp $	*/
/*	$NetBSD: mfs_extern.h,v 1.4 1996/02/09 22:31:27 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
 *	@(#)mfs_extern.h	8.2 (Berkeley) 6/16/94
 */

struct buf;
struct mount;
struct nameidata;
struct proc;
struct statfs;
struct ucred;
struct vnode;
struct vfsconf;
struct mbuf;

__BEGIN_DECLS
/* mfs_vfsops.c */
int	mfs_mountroot	__P((void));
int	mfs_initminiroot	__P((caddr_t));
int	mfs_mount	__P((struct mount *, const char *, caddr_t,
			     struct nameidata *, struct proc *));
int	mfs_start	__P((struct mount *, int, struct proc *));
int	mfs_statfs	__P((struct mount *, struct statfs *, struct proc *));

int	mfs_init	__P((struct vfsconf *));
int	mfs_checkexp	__P((struct mount *mp, struct mbuf *nam,
			     int *extflagsp, struct ucred **credanonp));

/* mfs_vnops.c */
int	mfs_open	__P((void *));
int	mfs_ioctl	__P((void *));
int	mfs_strategy	__P((void *));
void	mfs_doio	__P((struct buf *, caddr_t));
int	mfs_bmap	__P((void *));
int	mfs_close	__P((void *));
int	mfs_inactive	__P((void *));
int	mfs_reclaim	__P((void *));
int	mfs_print	__P((void *));
#define	mfs_revoke vop_generic_revoke
int	mfs_badop	__P((void *));

__END_DECLS
