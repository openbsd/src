/*	$OpenBSD: kernfs_vnops.c,v 1.39 2004/06/12 17:53:18 deraadt Exp $	*/
/*	$NetBSD: kernfs_vnops.c,v 1.43 1996/03/16 23:52:47 christos Exp $	*/

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
 *	@(#)kernfs_vnops.c	8.9 (Berkeley) 6/15/94
 */

/*
 * Kernel parameter filesystem (/kern)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/msgbuf.h>
#include <sys/poll.h>
#include <miscfs/kernfs/kernfs.h>

#include <uvm/uvm_extern.h>

#define KSTRING	256		/* Largest I/O available via this filesystem */
#define	UIO_MX 32

#define	READ_MODE	(S_IRUSR|S_IRGRP|S_IROTH)
#define	WRITE_MODE	(S_IWUSR|READ_MODE)
#define DIR_MODE	(S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)

static int	byteorder = BYTE_ORDER;
static int	posix = _POSIX_VERSION;
static int	osrev = OpenBSD;
extern int	ncpus;
extern char machine[], cpu_model[];

#ifdef IPSEC
extern int ipsp_kern(int, char **, int);
#endif

const struct kern_target kern_targets[] = {
/* NOTE: The name must be less than UIO_MX-16 chars in length */
#define N(s) sizeof(s)-1, s
     /*        name            data          tag           type  ro/rw */
     { DT_DIR, N("."),         0,            KTT_NULL,     VDIR, DIR_MODE   },
     { DT_DIR, N(".."),        0,            KTT_NULL,     VDIR, DIR_MODE   },
     { DT_REG, N("boottime"),  &boottime.tv_sec, KTT_INT,  VREG, READ_MODE  },
     { DT_REG, N("byteorder"), &byteorder,   KTT_INT,      VREG, READ_MODE  },
     { DT_REG, N("copyright"), (void*)copyright,KTT_STRING,   VREG, READ_MODE  },
     { DT_REG, N("hostname"),  0,            KTT_HOSTNAME, VREG, WRITE_MODE },
     { DT_REG, N("domainname"),0,            KTT_DOMAIN,   VREG, WRITE_MODE },
     { DT_REG, N("hz"),        &hz,          KTT_INT,      VREG, READ_MODE  },
     { DT_REG, N("loadavg"),   0,            KTT_AVENRUN,  VREG, READ_MODE  },
     { DT_REG, N("machine"),   machine,      KTT_STRING,   VREG, READ_MODE  },
     { DT_REG, N("model"),     cpu_model,    KTT_STRING,   VREG, READ_MODE  },
     { DT_REG, N("msgbuf"),    0,	     KTT_MSGBUF,   VREG, READ_MODE  },
     { DT_REG, N("ncpu"),      &ncpus,       KTT_INT,      VREG, READ_MODE  },
     { DT_REG, N("ostype"),    (void*)&ostype,KTT_STRING,   VREG, READ_MODE  },
     { DT_REG, N("osrelease"), (void*)&osrelease,KTT_STRING,VREG, READ_MODE  },
     { DT_REG, N("osrev"),     &osrev,	     KTT_INT,      VREG, READ_MODE  },
     { DT_REG, N("pagesize"),  &uvmexp.pagesize, KTT_INT,  VREG, READ_MODE  },
     { DT_REG, N("physmem"),   &physmem,     KTT_PHYSMEM,  VREG, READ_MODE  },
     { DT_REG, N("posix"),     &posix,       KTT_INT,      VREG, READ_MODE  },
#if 0
     { DT_DIR, N("root"),      0,            KTT_NULL,     VDIR, DIR_MODE   },
#endif
     { DT_BLK, N("rootdev"),   &rootdev,     KTT_DEVICE,   VBLK, READ_MODE  },
     { DT_CHR, N("rrootdev"),  &rrootdev,    KTT_DEVICE,   VCHR, READ_MODE  },
     { DT_REG, N("time"),      0,            KTT_TIME,     VREG, READ_MODE  },
     { DT_REG, N("usermem"),   0,            KTT_USERMEM,  VREG, READ_MODE  },
     { DT_REG, N("version"),   (void*)version,KTT_STRING,  VREG, READ_MODE  },
#ifdef IPSEC
     { DT_REG, N("ipsec"),     0,            KTT_IPSECSPI, VREG, READ_MODE  },
#endif
#undef N
};
static const int nkern_targets = sizeof(kern_targets) / sizeof(kern_targets[0]);

int	kernfs_badop(void *);

int	kernfs_lookup(void *);
#define	kernfs_create	eopnotsupp
#define	kernfs_mknod	eopnotsupp
int	kernfs_open(void *);
#define	kernfs_close	nullop
int	kernfs_access(void *);
int	kernfs_getattr(void *);
int	kernfs_setattr(void *);
int	kernfs_read(void *);
int	kernfs_write(void *);
#define	kernfs_ioctl	(int (*)(void *))enoioctl
#define	kernfs_mmap	eopnotsupp
#define	kernfs_fsync	nullop
#define	kernfs_seek	nullop
#define	kernfs_remove	eopnotsupp
int	kernfs_link(void *);
#define	kernfs_rename	eopnotsupp
#define kernfs_revoke   vop_generic_revoke
#define	kernfs_mkdir	eopnotsupp
#define	kernfs_rmdir	eopnotsupp
int	kernfs_symlink(void *);
int	kernfs_readdir(void *);
#define	kernfs_readlink	eopnotsupp
int	kernfs_inactive(void *);
int	kernfs_reclaim(void *);
#define	kernfs_lock	vop_generic_lock
#define	kernfs_unlock	vop_generic_unlock
#define	kernfs_bmap	kernfs_badop
#define	kernfs_strategy	kernfs_badop
int	kernfs_print(void *);
#define	kernfs_islocked	vop_generic_islocked
int	kernfs_pathconf(void *);
#define	kernfs_advlock	eopnotsupp
#define	kernfs_blkatoff	eopnotsupp
#define	kernfs_valloc	eopnotsupp
int	kernfs_vfree(void *);
#define	kernfs_truncate	eopnotsupp
#define	kernfs_update	eopnotsupp
#define	kernfs_bwrite	eopnotsupp

int	kernfs_xread(const struct kern_target *, int, char **, int);
int	kernfs_xwrite(const struct kern_target *, char *, int);
int	kernfs_freevp(struct vnode *, struct proc *);

int (**kernfs_vnodeop_p)(void *);
struct vnodeopv_entry_desc kernfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, kernfs_lookup },	/* lookup */
	{ &vop_create_desc, kernfs_create },	/* create */
	{ &vop_mknod_desc, kernfs_mknod },	/* mknod */
	{ &vop_open_desc, kernfs_open },	/* open */
	{ &vop_close_desc, kernfs_close },	/* close */
	{ &vop_access_desc, kernfs_access },	/* access */
	{ &vop_getattr_desc, kernfs_getattr },	/* getattr */
	{ &vop_setattr_desc, kernfs_setattr },	/* setattr */
	{ &vop_read_desc, kernfs_read },	/* read */
	{ &vop_write_desc, kernfs_write },	/* write */
	{ &vop_ioctl_desc, kernfs_ioctl },	/* ioctl */
	{ &vop_poll_desc, kernfs_poll },	/* poll */
	{ &vop_revoke_desc, kernfs_revoke },    /* revoke */
	{ &vop_fsync_desc, kernfs_fsync },	/* fsync */
	{ &vop_remove_desc, kernfs_remove },	/* remove */
	{ &vop_link_desc, kernfs_link },	/* link */
	{ &vop_rename_desc, kernfs_rename },	/* rename */
	{ &vop_mkdir_desc, kernfs_mkdir },	/* mkdir */
	{ &vop_rmdir_desc, kernfs_rmdir },	/* rmdir */
	{ &vop_symlink_desc, kernfs_symlink },	/* symlink */
	{ &vop_readdir_desc, kernfs_readdir },	/* readdir */
	{ &vop_readlink_desc, kernfs_readlink },/* readlink */
	{ &vop_abortop_desc, vop_generic_abortop },	/* abortop */
	{ &vop_inactive_desc, kernfs_inactive },/* inactive */
	{ &vop_reclaim_desc, kernfs_reclaim },	/* reclaim */
	{ &vop_lock_desc, kernfs_lock },	/* lock */
	{ &vop_unlock_desc, kernfs_unlock },	/* unlock */
	{ &vop_bmap_desc, kernfs_bmap },	/* bmap */
	{ &vop_strategy_desc, kernfs_strategy },/* strategy */
	{ &vop_print_desc, kernfs_print },	/* print */
	{ &vop_islocked_desc, kernfs_islocked },/* islocked */
	{ &vop_pathconf_desc, kernfs_pathconf },/* pathconf */
	{ &vop_advlock_desc, kernfs_advlock },	/* advlock */
	{ &vop_bwrite_desc, kernfs_bwrite },	/* bwrite */
	{ NULL, NULL }
};
struct vnodeopv_desc kernfs_vnodeop_opv_desc =
	{ &kernfs_vnodeop_p, kernfs_vnodeop_entries };

TAILQ_HEAD(, kernfs_node) kfshead;
static struct lock kfscache_lock;

int
kernfs_init(vfsp)
	struct vfsconf *vfsp;
{
	lockinit(&kfscache_lock, PVFS, "kfs", 0, 0);
	TAILQ_INIT(&kfshead);
	return(0);
}

int
kernfs_allocvp(kt, mp, vpp)
	const struct kern_target *kt;
	struct mount *mp;
	struct vnode **vpp;
{
	struct proc *p = curproc;
	struct kernfs_node *kf;
	struct vnode *vp;
	int error = 0;

#ifdef KERNFS_DIAGNOSTIC
	/* this should never happen */
	if (kt == NULL) 
		panic("kernfs_allocvp passed NULL kt, mp %p, vpp %p!", mp, vpp);

	printf("kernfs_allocvp: looking for %s\n", kt->kt_name);
#endif
	if ((error = lockmgr(&kfscache_lock, LK_EXCLUSIVE, NULL, p)) != 0)
		return(error);

loop:
	for (kf = TAILQ_FIRST(&kfshead); kf != NULL; kf = TAILQ_NEXT(kf, list)) {
		vp = KERNTOV(kf);
		if (vp->v_mount == mp && kt->kt_namlen == kf->kf_namlen &&
			bcmp(kf->kf_name, kt->kt_name, kt->kt_namlen) == 0) {
#ifdef KERNFS_DIAGNOSTIC
			printf("kernfs_allocvp: hit %s\n", kt->kt_name);
#endif
			if (vget(vp, 0, p)) 
				goto loop;
			*vpp = vp;
			goto out;
		}
	}

	MALLOC(kf, void *, sizeof(struct kernfs_node), M_TEMP, M_WAITOK);
	error = getnewvnode(VT_KERNFS, mp, kernfs_vnodeop_p, vpp);
	if (error) {
		FREE(kf, M_TEMP);
		goto out;
	}
	vp = *vpp;
	kf->kf_kt = kt;
	kf->kf_vnode = vp;
	vp->v_type = kf->kf_vtype;
	vp->v_data = kf; 
	if (kf->kf_namlen == 1 && bcmp(kf->kf_name, ".", 1) == 0) 
		vp->v_flag |= VROOT;

	TAILQ_INSERT_TAIL(&kfshead, kf, list);
out:
	lockmgr(&kfscache_lock, LK_RELEASE, NULL, p);

#ifdef KERNFS_DIAGNOSTIC
	if (error) 
		printf("kernfs_allocvp: error %d\n", error);
#endif
	return(error);
}

int
kernfs_freevp(vp, p)
	struct vnode *vp;
	struct proc *p;
{
	struct kernfs_node *kf = VTOKERN(vp);

	TAILQ_REMOVE(&kfshead, kf, list);
	FREE(vp->v_data, M_TEMP);
	vp->v_data = 0;
	return(0);
}

const struct kern_target *
kernfs_findtarget(name, namlen)
	char	*name;
	int	namlen;
{
	const struct kern_target *kt = NULL;
	int i;
	
	for (i = 0; i < nkern_targets; i++) {
		if (kern_targets[i].kt_namlen == namlen && 
			bcmp(kern_targets[i].kt_name, name, namlen) == 0) {
			kt = &kern_targets[i];
			break;
		}
	}

#ifdef KERNFS_DIAGNOSTIC
	if (i == nkern_targets || kt == NULL)
		printf("kernfs_findtarget: no match for %s\n", name);
#endif
	return(kt);
}
	
	
int
kernfs_xread(kt, off, bufp, len)
	const struct kern_target *kt;
	int off;
	char **bufp;
	int len;
{

	switch (kt->kt_tag) {
	case KTT_TIME: {
		struct timeval tv;

		microtime(&tv);
		snprintf(*bufp, len, "%ld %ld\n", tv.tv_sec, tv.tv_usec);
		break;
	}

	case KTT_INT: {
		int *ip = kt->kt_data;

		snprintf(*bufp, len, "%d\n", *ip);
		break;
	}

	case KTT_STRING: {
		char *cp = kt->kt_data;
		size_t end = strlen(cp);

		if (end && cp[end - 1] != '\n') {
			strlcpy(*bufp, cp, len - 1);
			strlcat(*bufp, "\n", len);
		} else
			*bufp = cp;

		break;
	}

	case KTT_MSGBUF: {
		extern struct msgbuf *msgbufp;
		long n;

		if (msgbufp == NULL || msgbufp->msg_magic != MSG_MAGIC)
			return (ENXIO);

		/*
		 * Note that reads of /kern/msgbuf won't necessarily yield
		 * consistent results, if the message buffer is modified
		 * while the read is in progress.  The worst that can happen
		 * is that incorrect data will be read.  There's no way
		 * that this can crash the system unless the values in the
		 * message buffer header are corrupted, but that'll cause
		 * the system to die anyway.
		 */
		if (msgbufp->msg_bufl < msgbufp->msg_bufs) {
			if (off >= msgbufp->msg_bufx)
				return (0);
			n = off;
			len = msgbufp->msg_bufx - n;
		} else {
			if (off >= msgbufp->msg_bufs)
				return (0);
			n = msgbufp->msg_bufx + off;
			if (n >= msgbufp->msg_bufs)
				n -= msgbufp->msg_bufs;
			len = min(msgbufp->msg_bufs - n, msgbufp->msg_bufs - off);
		}
		*bufp = msgbufp->msg_bufc + n;
		return (len);
	}

	case KTT_HOSTNAME: {
		char *cp = hostname;
		int xlen = hostnamelen;

		if (xlen >= (len-2))
			return (EINVAL);

		bcopy(cp, *bufp, xlen);
		(*bufp)[xlen] = '\n';
		(*bufp)[xlen+1] = '\0';
		break;
	}

	case KTT_DOMAIN: {
		char *cp = domainname;
		int xlen = domainnamelen;

		if (xlen >= (len-2))
			return (EINVAL);

		bcopy(cp, *bufp, xlen);
		(*bufp)[xlen] = '\n';
		(*bufp)[xlen+1] = '\0';
		break;
	}

	case KTT_AVENRUN:
		averunnable.fscale = FSCALE;
		snprintf(*bufp, len, "%d %d %d %ld\n",
		    averunnable.ldavg[0], averunnable.ldavg[1],
		    averunnable.ldavg[2], averunnable.fscale);
		break;

	case KTT_USERMEM:
		snprintf(*bufp, len, "%u\n", ctob(physmem - uvmexp.wired));
		break;

	case KTT_PHYSMEM:
		snprintf(*bufp, len, "%u\n", ctob(physmem));
		break;

#ifdef IPSEC
	case KTT_IPSECSPI:
		return(ipsp_kern(off, bufp, len));
#endif
	default:
		return (0);
	}

	len = strlen(*bufp);
	if (len <= off)
		return (0);
	*bufp += off;
	return (len - off);
}

int
kernfs_xwrite(kt, buf, len)
	const struct kern_target *kt;
	char *buf;
	int len;
{

	switch (kt->kt_tag) {
	case KTT_DOMAIN:
		if (buf[len-1] == '\n')
			--len;
		bcopy(buf, domainname, len);
		domainname[len] = '\0';
		domainnamelen = len;
		return (0);

	case KTT_HOSTNAME:
		if (buf[len-1] == '\n')
			--len;
		bcopy(buf, hostname, len);
		hostname[len] = '\0';
		hostnamelen = len;
		return (0);

	default:
		return (EIO);
	}
}


/*
 * vp is the current namei directory
 * ndp is the name to locate in that directory...
 */
int
kernfs_lookup(v)
	void *v;
{
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap = v;
	struct componentname *cnp = ap->a_cnp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	char *pname = cnp->cn_nameptr;
	struct proc *p = cnp->cn_proc;
	const struct kern_target *kt;
	struct vnode *vp;
	int error, wantpunlock;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_lookup(%p)\n", ap);
	printf("kernfs_lookup(dp = %p, vpp = %p, cnp = %p)\n", dvp, vpp, ap->a_cnp);
	printf("kernfs_lookup(%s)\n", pname);
#endif

	*vpp = NULLVP;
	cnp->cn_flags &= ~PDIRUNLOCK;

	if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
		return (EROFS);

	if (cnp->cn_namelen == 1 && *pname == '.') { 
		*vpp = dvp;
		VREF(dvp);
		return (0);
	}

	wantpunlock = (~cnp->cn_flags & (LOCKPARENT | ISLASTCN));

	kt = kernfs_findtarget(pname, cnp->cn_namelen);
	if (kt == NULL) {
		/* not found */
		return(cnp->cn_nameiop == LOOKUP ? ENOENT : EROFS);
	}

	if (kt->kt_tag == KTT_DEVICE) {
		dev_t *dp = kt->kt_data;
		
	loop:
		if (*dp == NODEV || !vfinddev(*dp, kt->kt_vtype, &vp)) 
			return(ENOENT);
		
		*vpp = vp;
		if (vget(vp, LK_EXCLUSIVE, p)) 
			goto loop;
		if (wantpunlock) {
			VOP_UNLOCK(dvp, 0, p);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		return(0);
	}


	if ((error = kernfs_allocvp(kt, dvp->v_mount, vpp)) != 0) {
		return(error);
	}

	vn_lock(*vpp, LK_EXCLUSIVE | LK_RETRY, p);

	if (wantpunlock) {
		VOP_UNLOCK(dvp, 0, p);
		cnp->cn_flags |= PDIRUNLOCK;
	}
	return (0);
}
		
/*ARGSUSED*/
int
kernfs_open(v)
	void *v;
{
	/* Only need to check access permissions. */
	return (0);
}

int
kernfs_access(v)
	void *v;
{
	struct vop_access_args /* {
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	
	mode_t fmode = (vp->v_flag & VROOT) ? DIR_MODE : VTOKERN(vp)->kf_mode;

	return (vaccess(fmode, (uid_t)0, (gid_t)0, ap->a_mode, ap->a_cred));
}

int
kernfs_getattr(v)
	void *v;
{
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	int error = 0;
	char strbuf[KSTRING], *buf;


	bzero(vap, sizeof(*vap));
	vattr_null(vap);
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_size = 0;
	vap->va_blocksize = DEV_BSIZE;
	TIMEVAL_TO_TIMESPEC(&time, &vap->va_atime);
	vap->va_mtime = vap->va_atime;
	vap->va_ctime = vap->va_atime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = 0;
	vap->va_bytes = 0;

	if (vp->v_flag & VROOT) {
#ifdef KERNFS_DIAGNOSTIC
		struct kern_target *kt = VTOKERN(vp)->kf_kt;
		printf("kernfs_getattr: stat rootdir (%s)\n", kt->kt_name);
#endif
		vap->va_type = VDIR;
		vap->va_mode = DIR_MODE;
		vap->va_nlink = 2;
		vap->va_fileid = 2;
		vap->va_size = DEV_BSIZE;
	} else {
		const struct kern_target *kt = VTOKERN(vp)->kf_kt;
		int nbytes, total;
#ifdef KERNFS_DIAGNOSTIC
		printf("kernfs_getattr: stat target %s\n", kt->kt_name);
#endif
		if (kt == &kern_targets[2]) {
			/* set boottime times to boottime */
			TIMEVAL_TO_TIMESPEC(&boottime, &vap->va_atime);
			vap->va_mtime = vap->va_atime;
			vap->va_ctime = vap->va_atime;
		}
		vap->va_type = kt->kt_vtype;
		vap->va_mode = kt->kt_mode;
		vap->va_nlink = 1;
		vap->va_fileid = 3 + (kt - kern_targets);
		total = 0;
		while (buf = strbuf,
		       nbytes = kernfs_xread(kt, total, &buf, sizeof(strbuf)))
			total += nbytes;
		vap->va_size = total;
	}

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_getattr: return error %d\n", error);
#endif
	return (error);
}

/*ARGSUSED*/
int
kernfs_setattr(v)
	void *v;
{
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap = v;

	if (ap->a_vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);

	/*
	 * Silently ignore attribute changes.
	 * This allows for open with truncate to have no
	 * effect until some data is written.  I want to
	 * do it this way because all writes are atomic.
	 */
	return (0);
}

int
kernfs_read(v)
	void *v;
{
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	const struct kern_target *kt;
	char strbuf[KSTRING], *buf;
	int off, len;
	int error;

	if (vp->v_type == VDIR)
		return (EOPNOTSUPP);

	kt = VTOKERN(vp)->kf_kt;

#ifdef KERNFS_DIAGNOSTIC
	printf("kern_read %s\n", kt->kt_name);
#endif

	off = uio->uio_offset;
#if 0
	while (buf = strbuf,
#else
	if (buf = strbuf,
#endif
	    len = kernfs_xread(kt, off, &buf, sizeof(strbuf))) {
		if ((error = uiomove(buf, len, uio)) != 0)
			return (error);
		off += len;
	}
	return (0);
}

int
kernfs_write(v)
	void *v;
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	const struct kern_target *kt;
	int error, xlen;
	char strbuf[KSTRING];

	if (vp->v_type == VDIR)
		return (EOPNOTSUPP);

	kt = VTOKERN(vp)->kf_kt;

#ifdef KERNFS_DIAGNOSTIC
	printf("kernfs_write %s\n", kt->kt_name);
#endif

	if (uio->uio_offset != 0)
		return (EINVAL);

	xlen = min(uio->uio_resid, KSTRING-1);
	if ((error = uiomove(strbuf, xlen, uio)) != 0)
		return (error);

	if (uio->uio_resid != 0)
		return (EIO);

	strbuf[xlen] = '\0';
	xlen = strlen(strbuf);
	return (kernfs_xwrite(kt, strbuf, xlen));
}

int
kernfs_readdir(v)
	void *v;
{
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int a_ncookies;
	} */ *ap = v;
	int error, i;
	struct uio *uio = ap->a_uio;
	struct dirent d;
	const struct kern_target *kt;

	if (ap->a_vp->v_type != VDIR)
		return (ENOTDIR);

	if (uio->uio_resid < UIO_MX)
		return (EINVAL);
	if (uio->uio_offset < 0)
		return (EINVAL);

	error = 0;
	i = uio->uio_offset;
	bzero(&d, UIO_MX);
	d.d_reclen = UIO_MX;

	for (kt = &kern_targets[i];
	     uio->uio_resid >= UIO_MX && i < nkern_targets; kt++, i++) {
#ifdef KERNFS_DIAGNOSTIC
		printf("kernfs_readdir: i = %d\n", i);
#endif

		if (kt->kt_tag == KTT_DEVICE) {
			dev_t *dp = kt->kt_data;
			struct vnode *fvp;

			if (*dp == NODEV || !vfinddev(*dp, kt->kt_vtype, &fvp))
				continue;
		}

		d.d_fileno = i + 3;
		d.d_namlen = kt->kt_namlen;
		bcopy(kt->kt_name, d.d_name, kt->kt_namlen + 1);
		d.d_type = kt->kt_type;

		if ((error = uiomove(&d, UIO_MX, uio)) != 0)
			break;
	}

	uio->uio_offset = i;
	return (error);
}

int
kernfs_inactive(v)
	void *v;
{
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
#ifdef KERNFS_DIAGNOSTIC
	struct kernfs_node *kf;

	kf = VTOKERN(vp);

	printf("kernfs_inactive(%p) %s\n", vp, kf->kf_name);
#endif
	VOP_UNLOCK(vp, 0, ap->a_p);
	return (0);
}

int
kernfs_reclaim(v)
	void *v;
{
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap = v;
	struct vnode *vp = ap->a_vp;
	struct proc *p = curproc;
#ifdef KERNFS_DIAGNOSTIC
	struct kernfs_node *kf;

	kf = VTOKERN(vp);

	printf("kernfs_reclaim(%p) %s\n", vp, kf->kf_name);
#endif
	return(kernfs_freevp(vp, p));
}

/*
 * Return POSIX pathconf information applicable to special devices.
 */
int
kernfs_pathconf(v)
	void *v;
{
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		register_t *a_retval;
	} */ *ap = v;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_MAX_CANON:
		*ap->a_retval = MAX_CANON;
		return (0);
	case _PC_MAX_INPUT:
		*ap->a_retval = MAX_INPUT;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_VDISABLE:
		*ap->a_retval = _POSIX_VDISABLE;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Print out some details of a kernfs vnode.
 */
/* ARGSUSED */
int
kernfs_print(v)
	void *v;
{
	struct vop_print_args /*
		struct vnode *a_vp;
	} */ *ap = v;
	struct kernfs_node *kf = VTOKERN(ap->a_vp);

	printf("tag VT_KERNFS, kernfs vnode: name %s, vp %p, tag %d\n", 
		kf->kf_name, kf->kf_vnode, kf->kf_tag); 
		
	return (0);
}

/*ARGSUSED*/
int
kernfs_vfree(v)
	void *v;
{
	return (0);
}

int
kernfs_link(v) 
	void *v;
{
	struct vop_link_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;  
		struct componentname *a_cnp;
	} */ *ap = v;
 
	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	vput(ap->a_dvp);
	return (EROFS);
}

int
kernfs_symlink(v)
	void *v;
{
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap = v;

	VOP_ABORTOP(ap->a_dvp, ap->a_cnp);
	vput(ap->a_dvp);
	return (EROFS);
}

/*
 * kernfs "should never get here" operation
 */
/*ARGSUSED*/
int
kernfs_badop(v)
	void *v;
{

	panic("kernfs: bad op");
	return 0;
}

int
kernfs_poll(v)
	void *v;
{
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int a_events;  
		struct proc *a_p;   
	} */ *ap = v;

	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}
