/*	$OpenBSD: tcfs.h,v 1.4 2002/03/14 01:27:08 millert Exp $	*/
/*
 * Copyright 2000 The TCFS Project at http://tcfs.dia.unisa.it/
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _TCFS_H_
#define _TCFS_H_

#include <miscfs/tcfs/tcfs_mount.h>

#ifdef _KERNEL
/*
 * A cache of vnode references
 */
struct tcfs_node {
	LIST_ENTRY(tcfs_node)	tcfs_hash;	/* Hash list */
	struct vnode	        *tcfs_lowervp;	/* VREFed once */
	struct vnode		*tcfs_vnode;	/* Back pointer */
};

extern int tcfs_node_create(struct mount *mp, struct vnode *target, struct vnode **vpp, int lockit);

#define	MOUNTTOTCFSMOUNT(mp) ((struct tcfs_mount *)((mp)->mnt_data))
#define	VTOTCFS(vp) ((struct tcfs_node *)(vp)->v_data)
#define	TCFSTOV(xp) ((xp)->tcfs_vnode)
#ifdef TCFS_DIAGNOSTIC
extern struct vnode *tcfs_checkvp(struct vnode *vp, char *fil, int lno);
#define	TCFSVPTOLOWERVP(vp) tcfs_checkvp((vp), __FILE__, __LINE__)
#else
#define	TCFSVPTOLOWERVP(vp) (VTOTCFS(vp)->tcfs_lowervp)
#endif

#define TCFS_VP2UKT(vp) ((MOUNTTOTCFSMOUNT(((vp)->v_mount)))->tcfs_uid_kt)
#define TCFS_VP2GKT(vp) ((MOUNTTOTCFSMOUNT(((vp)->v_mount)))->tcfs_gid_kt)

#define tcfs_fhtovp ((int (*)(struct mount *, struct fid *, \
	struct vnode **))eopnotsupp)
#define tcfs_vptofh ((int (*)(struct vnode *, struct fid *))eopnotsupp)

extern int (**tcfs_vnodeop_p)(void *);
extern struct vfsops tcfs_vfsops;

int tcfs_init(struct vfsconf *);

#define BLOCKSIZE       1024
#define SBLOCKSIZE         8

#define ABS(a)          ((a)>=0?(a):(-a))

/*      variabili esterne       */


/*      prototyphes             */

int     tcfs_bypass(void *);
int     tcfs_open(void *);
int     tcfs_getattr(void *);
int     tcfs_setattr(void *);
int     tcfs_inactive(void *);
int     tcfs_reclaim(void *);
int     tcfs_print(void *);
int     tcfs_strategy(void *);
int     tcfs_bwrite(void *);
int     tcfs_lock(void *);
int     tcfs_unlock(void *);
int     tcfs_islocked(void *);
int     tcfs_read(void *);
int     tcfs_readdir(void *);
int     tcfs_write(void *);
int     tcfs_create(void *);
int     tcfs_mknod(void *);
int     tcfs_mkdir(void *);
int     tcfs_link(void *);
int     tcfs_symlink(void *);
int     tcfs_rename(void *);
int     tcfs_lookup(void *);

void *tcfs_getukey(struct ucred *, struct proc *, struct vnode *);
void *tcfs_getpkey(struct ucred *, struct proc *, struct vnode *);
void *tcfs_getgkey(struct ucred *, struct proc *, struct vnode *);
int tcfs_checkukey(struct ucred *, struct proc *, struct vnode *);
int tcfs_checkpkey(struct ucred *, struct proc *, struct vnode *);
int tcfs_checkgkey(struct ucred *, struct proc *, struct vnode *);
int     tcfs_exec_cmd(struct tcfs_mount*, struct tcfs_args *);
int     tcfs_init_mp(struct tcfs_mount*, struct tcfs_args *);
int     tcfs_set_status(struct tcfs_mount *, struct tcfs_args *, int);
 
#define TCFS_CHECK_AKEY(c,p,v) (\
        tcfs_checkukey((c),(p),(v)) || \
        tcfs_checkpkey((c),(p),(v)) || \
        tcfs_checkgkey((c),(p),(v)) )

#endif /* _KERNEL */
#endif /* _TCFS_H_ */
