/*	$OpenBSD: fifo.h,v 1.6 1998/08/06 19:34:34 csapuntz Exp $	*/
/*	$NetBSD: fifo.h,v 1.10 1996/02/09 22:40:15 christos Exp $	*/

/*
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
 *	@(#)fifo.h	8.3 (Berkeley) 8/10/94
 */
#ifdef FIFO

/*
 * Prototypes for fifo operations on vnodes.
 */
int	fifo_badop	__P((void *));
int	fifo_ebadf	__P((void *));

int	fifo_lookup	__P((void *));
#define fifo_create	fifo_badop
#define fifo_mknod	fifo_badop
int	fifo_open	__P((void *));
int	fifo_close	__P((void *));
#define fifo_access	fifo_ebadf
#define fifo_getattr	fifo_ebadf
#define fifo_setattr	fifo_ebadf
int	fifo_read	__P((void *));
int	fifo_write	__P((void *));
#define fifo_lease_check nullop
int	fifo_ioctl	__P((void *));
int	fifo_select	__P((void *));
#define fifo_mmap	fifo_badop
#define fifo_fsync	nullop
#define fifo_seek	fifo_badop
#define fifo_remove	fifo_badop
#define fifo_revoke     vop_generic_revoke
#define fifo_link	fifo_badop
#define fifo_rename	fifo_badop
#define fifo_mkdir	fifo_badop
#define fifo_rmdir	fifo_badop
#define fifo_symlink	fifo_badop
#define fifo_readdir	fifo_badop
#define fifo_readlink	fifo_badop
#define fifo_abortop	fifo_badop
int fifo_inactive __P((void *));
#define fifo_reclaim	nullop
#define fifo_lock       vop_generic_lock
#define fifo_unlock     vop_generic_unlock
int	fifo_bmap	__P((void *));
#define fifo_strategy	fifo_badop
int	fifo_print	__P((void *));
#define fifo_islocked	vop_generic_islocked
int	fifo_pathconf	__P((void *));
int	fifo_advlock	__P((void *));
#define fifo_blkatoff	fifo_badop
#define fifo_valloc	fifo_badop
#define fifo_reallocblks fifo_badop
#define fifo_vfree	fifo_badop
#define fifo_truncate	nullop
#define fifo_update	nullop
#define fifo_bwrite	nullop

void 	fifo_printinfo __P((struct vnode *));

extern int (**fifo_vnodeop_p) __P((void *));

#endif /* FIFO */
