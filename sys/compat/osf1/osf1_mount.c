/*	$NetBSD: osf1_mount.c,v 1.5 1995/10/07 06:27:24 mycroft Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/syscallargs.h>

#include <compat/osf1/osf1_syscallargs.h>
#include <compat/osf1/osf1_util.h>

#include <net/if.h>
#include <netinet/in.h>

#include <machine/vmparam.h>

/* File system type numbers. */
#define	OSF1_MOUNT_NONE		0
#define	OSF1_MOUNT_UFS		1
#define	OSF1_MOUNT_NFS		2
#define	OSF1_MOUNT_MFS		3
#define	OSF1_MOUNT_PC		4
#define	OSF1_MOUNT_S5FS		5
#define	OSF1_MOUNT_CDFS		6
#define	OSF1_MOUNT_DFS		7
#define	OSF1_MOUNT_EFS		8
#define	OSF1_MOUNT_PROCFS	9
#define	OSF1_MOUNT_MSFS		10
#define	OSF1_MOUNT_FFM		11
#define	OSF1_MOUNT_FDFS		12
#define	OSF1_MOUNT_ADDON	13
#define	OSF1_MOUNT_MAXTYPE	OSF1_MOUNT_ADDON

#define	OSF1_MNT_WAIT		0x1
#define	OSF1_MNT_NOWAIT		0x2

#define	OSF1_MNT_FORCE		0x1
#define	OSF1_MNT_NOFORCE	0x2

/* acceptable flags for various calls */
#define	OSF1_GETFSSTAT_FLAGS	(OSF1_MNT_WAIT|OSF1_MNT_NOWAIT)
#define	OSF1_MOUNT_FLAGS	0xffffffff			/* XXX */
#define	OSF1_UNMOUNT_FLAGS	(OSF1_MNT_FORCE|OSF1_MNT_NOFORCE)

struct osf1_statfs {
	int16_t	f_type;				/*   0 */
	int16_t	f_flags;			/*   2 */
	int32_t	f_fsize;			/*   4 */
	int32_t	f_bsize;			/*   8 */
	int32_t	f_blocks;			/*  12 */
	int32_t	f_bfree;			/*  16 */
	int32_t	f_bavail;			/*  20 */
	int32_t	f_files;			/*  24 */
	int32_t	f_ffree;			/*  28 */
	int64_t	f_fsid;				/*  32 */
	int32_t	f_spare[9];			/*  40 (36 bytes) */
	char	f_mntonname[90];		/*  76 (90 bytes) */
	char	f_mntfromname[90];		/* 166 (90 bytes) */
	char	f_xxx[80];			/* 256 (80 bytes) XXX */
};

/* Arguments to mount() for various FS types. */
#ifdef notyet /* XXX */
struct osf1_ufs_args {
	char		*fspec;
	int32_t		exflags;
	u_int32_t	exroot;
};

struct osf1_cdfs_args {
	char		*fspec;
	int32_t		exflags;
	u_int32_t	exroot;
	int32_t		flags;
};
#endif

struct osf1_mfs_args {
	char		*name;
	caddr_t		base;
	u_int		size;
};

struct osf1_nfs_args {
	struct sockaddr_in	*addr;
	nfsv2fh_t		*fh;
	int32_t			flags;
	int32_t			wsize;
	int32_t			rsize;
	int32_t			timeo;
	int32_t			retrans;
	char			*hostname;
	int32_t			acregmin;
	int32_t			acregmax;
	int32_t			acdirmin;
	int32_t			acdirmax;
	char			*netname;
	void			*pathconf;
};

#define	OSF1_NFSMNT_SOFT	0x00001
#define	OSF1_NFSMNT_WSIZE	0x00002
#define	OSF1_NFSMNT_RSIZE	0x00004
#define	OSF1_NFSMNT_TIMEO	0x00008
#define	OSF1_NFSMNT_RETRANS	0x00010
#define	OSF1_NFSMNT_HOSTNAME	0x00020
#define	OSF1_NFSMNT_INT		0x00040
#define	OSF1_NFSMNT_NOCONN	0x00080
#define	OSF1_NFSMNT_NOAC	0x00100			/* ??? */
#define	OSF1_NFSMNT_ACREGMIN	0x00200			/* ??? */
#define	OSF1_NFSMNT_ACREGMAX	0x00400			/* ??? */
#define	OSF1_NFSMNT_ACDIRMIN	0x00800			/* ??? */
#define	OSF1_NFSMNT_ACDIRMAX	0x01000			/* ??? */
#define	OSF1_NFSMNT_NOCTO	0x02000			/* ??? */
#define	OSF1_NFSMNT_POSIX	0x04000			/* ??? */
#define	OSF1_NFSMNT_AUTO	0x08000			/* ??? */

#define OSF1_NFSMNT_FLAGS						\
	(OSF1_NFSMNT_SOFT|OSF1_NFSMNT_WSIZE|OSF1_NFSMNT_RSIZE|		\
	OSF1_NFSMNT_TIMEO|OSF1_NFSMNT_RETRANS|OSF1_NFSMNT_HOSTNAME|	\
	OSF1_NFSMNT_INT|OSF1_NFSMNT_NOCONN)

void
bsd2osf_statfs(bsfs, osfs)
	struct statfs *bsfs;
	struct osf1_statfs *osfs;
{

	bzero(osfs, sizeof (struct osf1_statfs));
	if (!strncmp(MOUNT_UFS, bsfs->f_fstypename, MFSNAMELEN))
		osfs->f_type = OSF1_MOUNT_UFS;
	else if (!strncmp(MOUNT_NFS, bsfs->f_fstypename, MFSNAMELEN))
		osfs->f_type = OSF1_MOUNT_NFS;
	else if (!strncmp(MOUNT_MFS, bsfs->f_fstypename, MFSNAMELEN))
		osfs->f_type = OSF1_MOUNT_MFS;
	else
		/* uh oh...  XXX = PC, CDFS, PROCFS, etc. */
		osfs->f_type = OSF1_MOUNT_ADDON;
	osfs->f_flags = bsfs->f_flags;		/* XXX translate */
	osfs->f_fsize = bsfs->f_bsize;
	osfs->f_bsize = bsfs->f_iosize;
	osfs->f_blocks = bsfs->f_blocks;
	osfs->f_bfree = bsfs->f_bfree;
	osfs->f_bavail = bsfs->f_bavail;
	osfs->f_files = bsfs->f_files;
	osfs->f_ffree = bsfs->f_ffree;
	bcopy(&bsfs->f_fsid, &osfs->f_fsid,
	    max(sizeof bsfs->f_fsid, sizeof osfs->f_fsid));
	/* osfs->f_spare zeroed above */
	bcopy(bsfs->f_mntonname, osfs->f_mntonname,
	    max(sizeof bsfs->f_mntonname, sizeof osfs->f_mntonname));
	bcopy(bsfs->f_mntfromname, osfs->f_mntfromname,
	    max(sizeof bsfs->f_mntfromname, sizeof osfs->f_mntfromname));
	/* XXX osfs->f_xxx should be filled in... */
}

int
osf1_sys_statfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_statfs_args /* {
		syscallarg(char *) path;
		syscallarg(struct osf1_statfs *) buf;
		syscallarg(int) len;
	} */ *uap = v;
	struct mount *mp;
	struct statfs *sp;
	struct osf1_statfs osfs;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	mp = nd.ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(nd.ni_vp);
	if (error = VFS_STATFS(mp, sp, p))
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	bsd2osf_statfs(sp, &osfs);
	return copyout(&osfs, SCARG(uap, buf), min(sizeof osfs,
	    SCARG(uap, len)));
}

int
osf1_sys_fstatfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_fstatfs_args /* {
		syscallarg(int) fd;
		syscallarg(struct osf1_statfs *) buf;   
		syscallarg(int) len;
	} */ *uap = v;
	struct file *fp;
	struct mount *mp;
	struct statfs *sp;
	struct osf1_statfs osfs;
	int error;

	if (error = getvnode(p->p_fd, uap->fd, &fp))
		return (error);
	mp = ((struct vnode *)fp->f_data)->v_mount;
	sp = &mp->mnt_stat;
	if (error = VFS_STATFS(mp, sp, p))
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	bsd2osf_statfs(sp, &osfs);
	return copyout(&osfs, SCARG(uap, buf), min(sizeof osfs,
	    SCARG(uap, len)));
}

int
osf1_sys_getfsstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct osf1_sys_getfsstat_args /* {
		syscallarg(struct osf1_statfs *) buf;
		syscallarg(long) bufsize;
		syscallarg(int) flags;
	} */ *uap = v;
	struct mount *mp, *nmp;
	struct statfs *sp;
	struct osf1_statfs osfs;
	caddr_t osf_sfsp;
	long count, maxcount, error;

	if (SCARG(uap, flags) & ~OSF1_GETFSSTAT_FLAGS)
		return (EINVAL);

	maxcount = SCARG(uap, bufsize) / sizeof(struct osf1_statfs);
	osf_sfsp = (caddr_t)SCARG(uap, buf);
	for (count = 0, mp = mountlist.cqh_first; mp != (void *)&mountlist;
	    mp = nmp) {
		nmp = mp->mnt_list.cqe_next;
		if (osf_sfsp && count < maxcount &&
		    ((mp->mnt_flag & MNT_MLOCK) == 0)) {
			sp = &mp->mnt_stat;
			/*
			 * If OSF1_MNT_NOWAIT is specified, do not refresh the
			 * fsstat cache.  OSF1_MNT_WAIT overrides
			 * OSF1_MNT_NOWAIT.
			 */
			if (((SCARG(uap, flags) & OSF1_MNT_NOWAIT) == 0 ||
			    (SCARG(uap, flags) & OSF1_MNT_WAIT)) &&
			    (error = VFS_STATFS(mp, sp, p)))
				continue;
			sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
			bsd2osf_statfs(sp, &osfs);
			if (error = copyout(&osfs, osf_sfsp,
			    sizeof (struct osf1_statfs)))
				return (error);
			osf_sfsp += sizeof (struct osf1_statfs);
		}
		count++;
	}
	if (osf_sfsp && count > maxcount)
		*retval = maxcount;
	else
		*retval = count;
	return (0);
}

int
osf1_sys_unmount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_unmount_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap = v;
	struct sys_unmount_args a;

	SCARG(&a, path) = SCARG(uap, path);

	if (SCARG(uap, flags) & ~OSF1_UNMOUNT_FLAGS)
		return (EINVAL);
	SCARG(&a, flags) = 0;
	if ((SCARG(uap, flags) & OSF1_MNT_FORCE) &&
	    (SCARG(uap, flags) & OSF1_MNT_NOFORCE) == 0)
		SCARG(&a, flags) |= MNT_FORCE;

	return sys_unmount(p, &a, retval);
}

int
osf1_sys_mount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_mount_args /* {
		syscallarg(int) type;
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(caddr_t) data;
	} */ *uap = v;
	struct sys_mount_args a;
	int error;

	SCARG(&a, path) = SCARG(uap, path);

	if (SCARG(uap, flags) & ~OSF1_MOUNT_FLAGS)
		return (EINVAL);
	SCARG(&a, flags) = SCARG(uap, flags);		/* XXX - xlate */

	switch (SCARG(uap, type)) {
	case OSF1_MOUNT_UFS:				/* XXX */
		return (EINVAL);
		break;

	case OSF1_MOUNT_NFS:				/* XXX */
		if (error = osf1_mount_nfs(p, uap, &a))
			return error;
		break;

	case OSF1_MOUNT_MFS:				/* XXX */
		if (error = osf1_mount_mfs(p, uap, &a))
			return error;
		break;

	case OSF1_MOUNT_CDFS:				/* XXX */
		return (EINVAL);
		break;

	case OSF1_MOUNT_PROCFS:				/* XXX */
		return (EINVAL);
		break;

	case OSF1_MOUNT_NONE:
	case OSF1_MOUNT_PC:
	case OSF1_MOUNT_S5FS:
	case OSF1_MOUNT_DFS:
	case OSF1_MOUNT_EFS:
	case OSF1_MOUNT_MSFS:
	case OSF1_MOUNT_FFM:
	case OSF1_MOUNT_FDFS:
	case OSF1_MOUNT_ADDON:
	default:
		return (EINVAL);
	}

	return sys_mount(p, &a, retval);
}

int
osf1_mount_mfs(p, osf_argp, bsd_argp)
	struct proc *p;
	struct osf1_sys_mount_args *osf_argp;
	struct sys_mount_args *bsd_argp;
{
	struct emul *e = p->p_emul;
	struct osf1_mfs_args osf_ma;
	struct mfs_args bsd_ma;
	caddr_t cp;
	int error;

	if (error = copyin(SCARG(osf_argp, data), &osf_ma, sizeof osf_ma))
		return error;

	bzero(&bsd_ma, sizeof bsd_ma);
	bsd_ma.fspec = osf_ma.name;
	/* XXX export args */
	bsd_ma.base = osf_ma.base;
	bsd_ma.size = osf_ma.size;

	cp = STACKGAPBASE;
	SCARG(bsd_argp, data) = cp;
	if (error = copyout(&bsd_ma, cp, sizeof bsd_ma))
		return error;
	cp += ALIGN(sizeof bsd_ma);

	SCARG(bsd_argp, type) = cp;
	if (error = copyout(MOUNT_MFS, cp, strlen(MOUNT_MFS) + 1))
		return error;

	return 0;
}

int
osf1_mount_nfs(p, osf_argp, bsd_argp)
	struct proc *p;
	struct osf1_sys_mount_args *osf_argp;
	struct sys_mount_args *bsd_argp;
{
	struct emul *e = p->p_emul;
	struct osf1_nfs_args osf_na;
	struct nfs_args bsd_na;
	caddr_t cp;
	int error;

	if (error = copyin(SCARG(osf_argp, data), &osf_na, sizeof osf_na))
		return error;

	bzero(&bsd_na, sizeof bsd_na);
	bsd_na.addr = (struct sockaddr *)osf_na.addr;
	bsd_na.addrlen = sizeof (struct sockaddr_in);
	bsd_na.sotype = SOCK_DGRAM; 
	bsd_na.proto = 0; 
	bsd_na.fh = osf_na.fh;

	if (osf_na.flags & ~OSF1_NFSMNT_FLAGS)
		return EINVAL;
	if (osf_na.flags & OSF1_NFSMNT_SOFT)
		bsd_na.flags |= NFSMNT_SOFT;
	if (osf_na.flags & OSF1_NFSMNT_WSIZE) {
		bsd_na.wsize = osf_na.wsize;
		bsd_na.flags |= NFSMNT_WSIZE;
	}
	if (osf_na.flags & OSF1_NFSMNT_RSIZE) {
		bsd_na.rsize = osf_na.rsize;
		bsd_na.flags |= NFSMNT_RSIZE;
	}
	if (osf_na.flags & OSF1_NFSMNT_TIMEO) {
		bsd_na.timeo = osf_na.timeo;
		bsd_na.flags |= NFSMNT_TIMEO;
	}
	if (osf_na.flags & OSF1_NFSMNT_RETRANS) {
		bsd_na.retrans = osf_na.retrans;
		bsd_na.flags |= NFSMNT_RETRANS;
	}
	if (osf_na.flags & OSF1_NFSMNT_HOSTNAME)
		bsd_na.hostname = osf_na.hostname;
	if (osf_na.flags & OSF1_NFSMNT_INT)
		bsd_na.flags |= NFSMNT_INT;
	if (osf_na.flags & OSF1_NFSMNT_NOCONN)
		bsd_na.flags |= NFSMNT_NOCONN;

	cp = STACKGAPBASE;
	SCARG(bsd_argp, data) = cp;
	if (error = copyout(&bsd_na, cp, sizeof bsd_na))
		return error;
	cp += ALIGN(sizeof bsd_na);

	SCARG(bsd_argp, type) = cp;
	if (error = copyout(MOUNT_NFS, cp, strlen(MOUNT_NFS) + 1))
		return error;

	return 0;
}
