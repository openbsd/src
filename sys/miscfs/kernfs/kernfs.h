/*	$OpenBSD: kernfs.h,v 1.10 2002/03/14 01:27:08 millert Exp $	*/
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
#define KTT_PHYSMEM	99
#ifdef IPSEC
#define KTT_IPSECSPI	107
#endif
	u_char kt_tag;
	u_char kt_vtype;
	mode_t kt_mode;
};

struct kernfs_node {
	TAILQ_ENTRY(kernfs_node) list;
	struct kern_target *kf_kt;
	struct vnode	*kf_vnode;
#define kf_type		kf_kt->kt_type
#define kf_namlen	kf_kt->kt_namlen
#define kf_name		kf_kt->kt_name
#define kf_data		kf_kt->kt_data
#define kf_vtype	kf_kt->kt_vtype
#define kf_mode		kf_kt->kt_mode
#define kf_tag		kf_kt->kt_tag
};

#define KERNTOV(kn) ((struct vnode *)(kn)->kf_vnode)
#define	VTOKERN(vp) ((struct kernfs_node *)(vp)->v_data)

#define kernfs_fhtovp ((int (*)(struct mount *, struct fid *, \
	    struct vnode **))eopnotsupp)
#define kernfs_quotactl ((int (*)(struct mount *, int, uid_t, caddr_t, \
	    struct proc *))eopnotsupp)
#define kernfs_sysctl ((int (*)(int *, u_int, void *, size_t *, void *, \
	    size_t, struct proc *))eopnotsupp)
#define kernfs_vget ((int (*)(struct mount *, ino_t, struct vnode **)) \
	    eopnotsupp)
#define kernfs_vptofh ((int (*)(struct vnode *, struct fid *))eopnotsupp)
#define kernfs_sync ((int (*)(struct mount *, int, struct ucred *, \
				   struct proc *))nullop)
#define kernfs_checkexp ((int (*)(struct mount *, struct mbuf *,	\
	int *, struct ucred **))eopnotsupp)

int	kernfs_init(struct vfsconf *);
int	kernfs_allocvp(struct kern_target *, struct mount *, struct vnode **);
struct kern_target 	*kernfs_findtarget(char *, int);
extern int (**kernfs_vnodeop_p)(void *);
extern struct vfsops kernfs_vfsops;
extern dev_t rrootdev;
#endif /* _KERNEL */
