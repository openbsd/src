/*	$NetBSD: ffs_extern.h,v 1.83 2016/10/01 13:15:45 jdolecek Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
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
 *	@(#)ffs_extern.h	8.6 (Berkeley) 3/30/95
 */

#ifndef _UFS_FFS_FFS_EXTERN_H_
#define _UFS_FFS_FFS_EXTERN_H_

/*
 * Sysctl values for the fast filesystem.
 */
#define FFS_CLUSTERREAD		1	/* cluster reading enabled */
#define FFS_CLUSTERWRITE	2	/* cluster writing enabled */
#define FFS_REALLOCBLKS		3	/* block reallocation enabled */
#define FFS_ASYNCFREE		4	/* asynchronous block freeing enabled */
#define FFS_LOG_CHANGEOPT	5	/* log optimalization strategy change */
#define FFS_EXTATTR_AUTOCREATE	6	/* size for backing file autocreation */
#define FFS_MAXID		7	/* number of valid ffs ids */

struct buf;
struct fid;
struct fs;
struct inode;
struct ufs1_dinode;
struct ufs2_dinode;
struct mount;
struct nameidata;
struct lwp;
struct statvfs;
struct timeval;
struct timespec;
struct ufsmount;
struct uio;
struct vnode;
struct mbuf;
struct cg;


__BEGIN_DECLS


/* ffs_appleufs.c */
struct appleufslabel;
u_int16_t ffs_appleufs_cksum(const struct appleufslabel *);
int	ffs_appleufs_validate(const char*, const struct appleufslabel *,
			      struct appleufslabel *);
void	ffs_appleufs_set(struct appleufslabel *, const char *, time_t,
			 uint64_t);

/* ffs_bswap.c */
void	ffs_sb_swap(struct fs*, struct fs *);
void	ffs_dinode1_swap(struct ufs1_dinode *, struct ufs1_dinode *);
void	ffs_dinode2_swap(struct ufs2_dinode *, struct ufs2_dinode *);
struct csum;
void	ffs_csum_swap(struct csum *, struct csum *, int);
struct csum_total;
void	ffs_csumtotal_swap(struct csum_total *, struct csum_total *);
void	ffs_cg_swap(struct cg *, struct cg *, struct fs *);

/* ffs_subr.c */
void	ffs_fragacct(struct fs *, int, int32_t[], int, int);
int	ffs_isblock(struct fs *, u_char *, int32_t);
int	ffs_isfreeblock(struct fs *, u_char *, int32_t);
void	ffs_clrblock(struct fs *, u_char *, int32_t);
void	ffs_setblock(struct fs *, u_char *, int32_t);
void	ffs_itimes(struct inode *, const struct timespec *,
    const struct timespec *, const struct timespec *);
void	ffs_clusteracct(struct fs *, struct cg *, int32_t, int);

/* ffs_quota2.c */
int	ffs_quota2_mount(struct mount *);

__END_DECLS

#endif /* !_UFS_FFS_FFS_EXTERN_H_ */
