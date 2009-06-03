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

/* $arla: nnpfs_vfsops.h,v 1.17 2003/06/02 18:27:10 lha Exp $ */

#ifndef _nnpfs_vfsops_h
#define _nnpfs_vfsops_h

int
nnpfs_mount_common(struct mount *mp,
		 const char *user_path,
		 void *user_data,
		 struct nameidata *ndp,
		 d_thread_t *p);

int
nnpfs_mount_common_sys(struct mount *mp,
		     const char *path,
		     void *data,
		     struct nameidata *ndp,
		     d_thread_t *p);

int
nnpfs_unmount_common(struct mount *mp, int mntflags);

int
nnpfs_root_common(struct mount *mp,
		struct vnode **vpp,
		d_thread_t *proc,
		struct ucred *cred);

int
nnpfs_fhlookup (d_thread_t *proc,
	      struct nnpfs_fhandle_t *fhp,
	      struct vnode **vpp);

int
nnpfs_fhopen (d_thread_t *proc,
	    struct nnpfs_fhandle_t *fhp,
	    int flags,
	    register_t *retval);

int nnpfs_make_dead_vnode(struct mount *mp, struct vnode **vpp);

#endif				       /* _nnpfs_vfsops_h */
