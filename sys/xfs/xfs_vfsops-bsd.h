/* $OpenBSD: xfs_vfsops-bsd.h,v 1.3 2000/03/03 00:54:59 todd Exp $ */
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 *
 * 4. Neither the name of the Institute nor the names of its contributors
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


#ifndef _xfs_vfsops_bsd_h
#define _xfs_vfsops_bsd_h

int
xfs_mount(struct mount * mp,
	  const char *user_path,
	  caddr_t user_data,
	  struct nameidata * ndp,
	  struct proc * p);

int
xfs_start(struct mount * mp, int flags, struct proc * p);

int
xfs_unmount(struct mount * mp, int mntflags, struct proc *p);

int
xfs_root(struct mount *mp, struct vnode **vpp);

int
xfs_quotactl(struct mount *mp, int cmd, uid_t uid, caddr_t arg, struct proc *p);

int
xfs_statfs(struct mount *mp, struct statfs *sbp, struct proc *p);

int
xfs_sync(struct mount *mp, int waitfor, struct ucred *cred, struct proc *p);

int
xfs_vget(struct mount * mp,
	 ino_t ino,
	 struct vnode ** vpp);

int
xfs_fhtovp(struct mount * mp,
	   struct fid * fhp,
	   struct vnode ** vpp);

int
xfs_vptofh(struct vnode * vp,
	   struct fid * fhp);

int
xfs_dead_lookup(struct vop_lookup_args * ap);

#endif /* _xfs_vfsops_bsd_h */
