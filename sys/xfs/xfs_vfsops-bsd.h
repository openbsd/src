/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $arla: xfs_vfsops-bsd.h,v 1.14 2002/09/07 10:46:09 lha Exp $ */

#ifndef _xfs_vfsops_bsd_h
#define _xfs_vfsops_bsd_h

int
xfs_mount_caddr(struct mount *mp, const char *user_path, caddr_t user_data,
		struct nameidata *ndp, d_thread_t *p);

int
xfs_start(struct mount * mp, int flags, d_thread_t * p);

int
xfs_unmount(struct mount * mp, int mntflags, d_thread_t *p);

int
xfs_root(struct mount *mp, struct vnode **vpp);

int
xfs_quotactl(struct mount *mp, int cmd, uid_t uid, caddr_t arg, d_thread_t *p);

int
xfs_statfs(struct mount *mp, struct statfs *sbp, d_thread_t *p);

int
xfs_sync(struct mount *mp, int waitfor, struct ucred *cred, d_thread_t *p);

int
xfs_vget(struct mount * mp,
#ifdef __APPLE__
	 void *ino,
#else
	 ino_t ino,
#endif
	 struct vnode ** vpp);

#ifdef HAVE_STRUCT_VFSOPS_VFS_CHECKEXP
int
xfs_fhtovp(struct mount * mp,
	   struct fid * fhp,
	   struct vnode ** vpp);
#else
int
xfs_fhtovp(struct mount * mp,
	   struct fid * fhp,
	   struct mbuf * nam,
	   struct vnode ** vpp,
	   int *exflagsp,
	   struct ucred ** credanonp);
#endif

struct mbuf;
int
xfs_checkexp (struct mount *mp,
#ifdef __FreeBSD__
	      struct sockaddr *nam,
#else
	      struct mbuf *nam,
#endif
	      int *exflagsp,
	      struct ucred **credanonp);

int
xfs_vptofh(struct vnode * vp,
	   struct fid * fhp);

int
xfs_dead_lookup(struct vop_lookup_args * ap);

#endif /* _xfs_vfsops_bsd_h */
