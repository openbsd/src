/*	$OpenBSD: kernfs.h,v 1.7 1998/12/28 05:51:38 millert Exp $	*/
/*	$NetBSD: kernfs.h,v 1.10 1996/02/09 22:40:21 christos Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)kernfs.h	8.5 (Berkeley) 6/15/94
 */

#define	_PATH_KERNFS	"/kern"		/* Default mountpoint */

#ifdef _KERNEL

struct kern_target {
	u_char kt_type;
	u_char kt_namlen;
	char *kt_name;
	void *kt_data;
#define	KTT_NULL	 1
#define	KTT_TIME	 5
#define KTT_INT		17
#define	KTT_STRING	31
#define KTT_HOSTNAME	47
#define KTT_AVENRUN	53
#define KTT_DEVICE	71
#define	KTT_MSGBUF	89
#define KTT_USERMEM	91
#define KTT_DOMAIN	95
#ifdef IPSEC
#define KTT_IPSECSPI	107
#endif
	u_char kt_tag;
	u_char kt_vtype;
	mode_t kt_mode;
};

struct kernfs_mount {
	struct vnode	*kf_root;	/* Root node */
};

struct kernfs_node {
	struct kern_target *kf_kt;
};

#define VFSTOKERNFS(mp)	((struct kernfs_mount *)((mp)->mnt_data))
#define	VTOKERN(vp) ((struct kernfs_node *)(vp)->v_data)

#define kernfs_fhtovp ((int (*) __P((struct mount *, struct fid *, \
	    struct mbuf *, struct vnode **, int *, struct ucred **)))eopnotsupp)
#define kernfs_quotactl ((int (*) __P((struct mount *, int, uid_t, caddr_t, \
	    struct proc *)))eopnotsupp)
#define kernfs_sysctl ((int (*) __P((int *, u_int, void *, size_t *, void *, \
	    size_t, struct proc *)))eopnotsupp)
#define kernfs_vget ((int (*) __P((struct mount *, ino_t, struct vnode **))) \
	    eopnotsupp)
#define kernfs_vptofh ((int (*) __P((struct vnode *, struct fid *)))eopnotsupp)
#define kernfs_sync ((int (*) __P((struct mount *, int, struct ucred *, \
				   struct proc *)))nullop)

extern int (**kernfs_vnodeop_p) __P((void *));
extern struct vfsops kernfs_vfsops;
extern dev_t rrootdev;
#endif /* _KERNEL */
