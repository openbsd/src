/*	$OpenBSD: mount.h,v 1.67 2005/12/13 00:35:23 millert Exp $	*/
/*	$NetBSD: mount.h,v 1.48 1996/02/18 11:55:47 fvdl Exp $	*/

/*
 * Copyright (c) 1989, 1991, 1993
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
 *	@(#)mount.h	8.15 (Berkeley) 7/14/94
 */

#ifndef _SYS_MOUNT_H_
#define _SYS_MOUNT_H_

#include <sys/cdefs.h>
#ifndef _KERNEL
#include <sys/ucred.h>
#endif
#include <sys/queue.h>
#include <sys/lock.h>

typedef struct { int32_t val[2]; } fsid_t;	/* file system id type */

/*
 * File identifier.
 * These are unique per filesystem on a single machine.
 */
#define	MAXFIDSZ	16

struct fid {
	u_short		fid_len;		/* length of data in bytes */
	u_short		fid_reserved;		/* force longword alignment */
	char		fid_data[MAXFIDSZ];	/* data (variable length) */
};

/*
 * Export arguments for local filesystem mount calls.
 */
struct export_args {
	int	ex_flags;		/* export related flags */
	uid_t	ex_root;		/* mapping for root uid */
	struct	ucred ex_anon;		/* mapping for anonymous user */
	struct	sockaddr *ex_addr;	/* net address to which exported */
	int	ex_addrlen;		/* and the net address length */
	struct	sockaddr *ex_mask;	/* mask of valid bits in saddr */
	int	ex_masklen;		/* and the smask length */
};

/*
 * Arguments to mount UFS-based filesystems
 */
struct ufs_args {
	char	*fspec;			/* block special device to mount */
	struct	export_args export_info;/* network export information */
};

/*
 * Arguments to mount MFS
 */
struct mfs_args {
	char	*fspec;			/* name to export for statfs */
	struct	export_args export_info;/* if exported MFSes are supported */
	caddr_t	base;			/* base of file system in memory */
	u_long	size;			/* size of file system */
};

/*
 * Arguments to mount ISO 9660 filesystems.
 */
struct iso_args {
	char	*fspec;			/* block special device to mount */
	struct	export_args export_info;/* network export info */
	int	flags;			/* mounting flags, see below */
};
#define	ISOFSMNT_NORRIP	0x00000001	/* disable Rock Ridge Ext.*/
#define	ISOFSMNT_GENS	0x00000002	/* enable generation numbers */
#define	ISOFSMNT_EXTATT	0x00000004	/* enable extended attributes */
#define	ISOFSMNT_NOJOLIET 0x00000008	/* disable Joliet Ext.*/

/*
 * Arguments to mount NFS
 */
#define NFS_ARGSVERSION	4		/* change when nfs_args changes */
struct nfs_args {
	int		version;	/* args structure version number */
	struct sockaddr	*addr;		/* file server address */
	int		addrlen;	/* length of address */
	int		sotype;		/* Socket type */
	int		proto;		/* and Protocol */
	u_char		*fh;		/* File handle to be mounted */
	int		fhsize;		/* Size, in bytes, of fh */
	int		flags;		/* flags */
	int		wsize;		/* write size in bytes */
	int		rsize;		/* read size in bytes */
	int		readdirsize;	/* readdir size in bytes */
	int		timeo;		/* initial timeout in .1 secs */
	int		retrans;	/* times to retry send */
	int		maxgrouplist;	/* Max. size of group list */
	int		readahead;	/* # of blocks to readahead */
	int		leaseterm;	/* Term (sec) of lease */
	int		deadthresh;	/* Retrans threshold */
	char		*hostname;	/* server's name */
	int		acregmin;	/* Attr cache file recently modified */
	int		acregmax;	/* ac file not recently modified */
	int		acdirmin;	/* ac for dir recently modified */
	int		acdirmax;	/* ac for dir not recently modified */
};
/* NFS args version 3 (for backwards compatibility) */
struct nfs_args3 {
	int		version;	/* args structure version number */
	struct sockaddr	*addr;		/* file server address */
	int		addrlen;	/* length of address */
	int		sotype;		/* Socket type */
	int		proto;		/* and Protocol */
	u_char		*fh;		/* File handle to be mounted */
	int		fhsize;		/* Size, in bytes, of fh */
	int		flags;		/* flags */
	int		wsize;		/* write size in bytes */
	int		rsize;		/* read size in bytes */
	int		readdirsize;	/* readdir size in bytes */
	int		timeo;		/* initial timeout in .1 secs */
	int		retrans;	/* times to retry send */
	int		maxgrouplist;	/* Max. size of group list */
	int		readahead;	/* # of blocks to readahead */
	int		leaseterm;	/* Term (sec) of lease */
	int		deadthresh;	/* Retrans threshold */
	char		*hostname;	/* server's name */
};

/*
 * NFS mount option flags
 */
#ifndef _KERNEL
#define	NFSMNT_RESVPORT		0x00000000  /* always use reserved ports */
#endif /* ! _KERNEL */
#define	NFSMNT_SOFT		0x00000001  /* soft mount (hard is default) */
#define	NFSMNT_WSIZE		0x00000002  /* set write size */
#define	NFSMNT_RSIZE		0x00000004  /* set read size */
#define	NFSMNT_TIMEO		0x00000008  /* set initial timeout */
#define	NFSMNT_RETRANS		0x00000010  /* set number of request retries */
#define	NFSMNT_MAXGRPS		0x00000020  /* set maximum grouplist size */
#define	NFSMNT_INT		0x00000040  /* allow interrupts on hard mount */
#define	NFSMNT_NOCONN		0x00000080  /* Don't Connect the socket */
#define	NFSMNT_NQNFS		0x00000100  /* Use Nqnfs protocol */
#define	NFSMNT_NFSV3		0x00000200  /* Use NFS Version 3 protocol */
#define	NFSMNT_KERB		0x00000400  /* Use Kerberos authentication */
#define	NFSMNT_DUMBTIMR		0x00000800  /* Don't estimate rtt dynamically */
#define	NFSMNT_LEASETERM	0x00001000  /* set lease term (nqnfs) */
#define	NFSMNT_READAHEAD	0x00002000  /* set read ahead */
#define	NFSMNT_DEADTHRESH	0x00004000  /* set dead server retry thresh */
#ifdef _KERNEL /* Coming soon to a system call near you! */
#define	NFSMNT_NOAC		0x00008000  /* disable attribute cache */
#endif /* _KERNEL */
#define	NFSMNT_RDIRPLUS		0x00010000  /* Use Readdirplus for V3 */
#define	NFSMNT_READDIRSIZE	0x00020000  /* Set readdir size */

/* Flags valid only in mount syscall arguments */
#define NFSMNT_ACREGMIN		0x00040000  /* acregmin field valid */
#define NFSMNT_ACREGMAX		0x00080000  /* acregmax field valid */
#define NFSMNT_ACDIRMIN		0x00100000  /* acdirmin field valid */
#define NFSMNT_ACDIRMAX		0x00200000  /* acdirmax field valid */

/* Flags valid only in kernel */
#define	NFSMNT_INTERNAL		0xfffc0000  /* Bits set internally */
#define NFSMNT_HASWRITEVERF	0x00040000  /* Has write verifier for V3 */
#define NFSMNT_GOTPATHCONF	0x00080000  /* Got the V3 pathconf info */
#define NFSMNT_GOTFSINFO	0x00100000  /* Got the V3 fsinfo */
#define	NFSMNT_MNTD		0x00200000  /* Mnt server for mnt point */
#define	NFSMNT_DISMINPROG	0x00400000  /* Dismount in progress */
#define	NFSMNT_DISMNT		0x00800000  /* Dismounted */
#define	NFSMNT_SNDLOCK		0x01000000  /* Send socket lock */
#define	NFSMNT_WANTSND		0x02000000  /* Want above */
#define	NFSMNT_RCVLOCK		0x04000000  /* Rcv socket lock */
#define	NFSMNT_WANTRCV		0x08000000  /* Want above */
#define	NFSMNT_WAITAUTH		0x10000000  /* Wait for authentication */
#define	NFSMNT_HASAUTH		0x20000000  /* Has authenticator */
#define	NFSMNT_WANTAUTH		0x40000000  /* Wants an authenticator */
#define	NFSMNT_AUTHERR		0x80000000  /* Authentication error */

/*
 *  Arguments to mount MSDOS filesystems.
 */
struct msdosfs_args {
	char	*fspec;		/* blocks special holding the fs to mount */
	struct	export_args export_info;
				/* network export information */
	uid_t	uid;		/* uid that owns msdosfs files */
	gid_t	gid;		/* gid that owns msdosfs files */
	mode_t  mask;		/* mask to be applied for msdosfs perms */
	int	flags;		/* see below */
};

/*
 * Msdosfs mount options:
 */
#define	MSDOSFSMNT_SHORTNAME	0x01	/* Force old DOS short names only */
#define	MSDOSFSMNT_LONGNAME	0x02	/* Force Win'95 long names */
#define	MSDOSFSMNT_NOWIN95	0x04	/* Completely ignore Win95 entries */
#define	MSDOSFSMNT_GEMDOSFS	0x08	/* This is a gemdos-flavour */
#define MSDOSFSMNT_ALLOWDIRX	0x10	/* dir is mode +x if r */

/*
 * Arguments to mount amigados filesystems.
 */
struct adosfs_args {
	char	*fspec;		/* blocks special holding the fs to mount */
	struct	export_args export_info;
				/* network export information */
	uid_t	uid;		/* uid that owns adosfs files */
	gid_t	gid;		/* gid that owns adosfs files */
	mode_t	mask;		/* mask to be applied for adosfs perms */
};

/*
 * Arguments to mount ntfs filesystems
 */
struct ntfs_args {
	char	*fspec;			/* block special device to mount */
	struct	export_args export_info;/* network export information */
	uid_t	uid;			/* uid that owns ntfs files */
	gid_t	gid;			/* gid that owns ntfs files */
	mode_t	mode;			/* mask to be applied for ntfs perms */
	u_long	flag;			/* additional flags */
};

/*
 * ntfs mount options:
 */
#define	NTFS_MFLAG_CASEINS      0x00000001
#define	NTFS_MFLAG_ALLNAMES     0x00000002

/*
 * Arguments to mount UDF filesystems.
 */
struct udf_args {
	char	*fspec;			/* block special device to mount */
};

/*
 * Arguments to mount procfs filesystems
 */
struct procfs_args {
	int version;
	int flags;
};

/*
 * procfs mount options:
 */
#define PROCFS_ARGSVERSION      1
#define PROCFSMNT_LINUXCOMPAT   0x01


/*
 * file system statistics
 */

#define	MFSNAMELEN	16	/* length of fs type name, including nul */
#define	MNAMELEN	90	/* length of buffer for returned name */

/* per-filesystem mount options */
union mount_info {
	struct ufs_args ufs_args;
	struct mfs_args mfs_args;
	struct nfs_args nfs_args;
	struct iso_args iso_args;
	struct procfs_args procfs_args;
	struct msdosfs_args msdosfs_args;
	struct adosfs_args adosfs_args;
	struct ntfs_args ntfs_args;
	char __align[160];	/* 64-bit alignment and room to grow */
};

/* new statfs structure with mount options */
struct statfs {
	u_int32_t  f_flags;		/* copy of mount flags */
	int32_t    f_bsize;		/* fundamental file system block size */
	u_int32_t  f_iosize;		/* optimal transfer block size */
	u_int32_t  f_blocks;		/* total data blocks in file system */
	u_int32_t  f_bfree;		/* free blocks in fs */
	int32_t    f_bavail;		/* free blocks avail to non-superuser */
	u_int32_t  f_files;		/* total file nodes in file system */
	u_int32_t  f_ffree;		/* free file nodes in fs */
	fsid_t	   f_fsid;		/* file system id */
	uid_t	   f_owner;		/* user that mounted the file system */
	u_int32_t  f_syncwrites;	/* count of sync writes since mount */
	u_int32_t  f_asyncwrites;	/* count of async writes since mount */
	u_int32_t  f_ctime;		/* last mount [-u] time */
	u_int32_t  f_spare[3];		/* spare for later */
	char	   f_fstypename[MFSNAMELEN]; /* fs type name */
	char	   f_mntonname[MNAMELEN];    /* directory on which mounted */
	char	   f_mntfromname[MNAMELEN];  /* mounted file system */
	union mount_info mount_info;	    /* per-filesystem mount options */
};

/* old (pre-2.6) statfs structure */
struct ostatfs {
	short	f_type;			/* type of file system (unused; zero) */
	short	f_flags;		/* copy of mount flags */
	long	f_bsize;		/* fundamental file system block size */
	long	f_iosize;		/* optimal transfer block size */
	long	f_blocks;		/* total data blocks in file system */
	long	f_bfree;		/* free blocks in fs */
	long	f_bavail;		/* free blocks avail to non-superuser */
	long	f_files;		/* total file nodes in file system */
	long	f_ffree;		/* free file nodes in fs */
	fsid_t	f_fsid;			/* file system id */
	uid_t	f_owner;		/* user that mounted the file system */
	long	f_syncwrites;		/* count of sync writes since mount */
	long	f_asyncwrites;		/* count of async writes since mount */
	long	f_spare[2];		/* spare for later */
	char	f_fstypename[MFSNAMELEN]; /* fs type name */
	char	f_mntonname[MNAMELEN];	  /* directory on which mounted */
	char	f_mntfromname[MNAMELEN];  /* mounted file system */
};

/*
 * File system types.
 */
#define	MOUNT_FFS	"ffs"		/* UNIX "Fast" Filesystem */
#define	MOUNT_UFS	MOUNT_FFS	/* for compatibility */
#define	MOUNT_NFS	"nfs"		/* Network Filesystem */
#define	MOUNT_MFS	"mfs"		/* Memory Filesystem */
#define	MOUNT_MSDOS	"msdos"		/* MSDOS Filesystem */
#define	MOUNT_LFS	"lfs"		/* Log-based Filesystem */
#define	MOUNT_PORTAL	"portal"	/* Portal Filesystem */
#define	MOUNT_PROCFS	"procfs"	/* /proc Filesystem */
#define	MOUNT_AFS	"afs"		/* Andrew Filesystem */
#define	MOUNT_CD9660	"cd9660"	/* ISO9660 (aka CDROM) Filesystem */
#define	MOUNT_ADOSFS	"adosfs"	/* AmigaDOS Filesystem */
#define	MOUNT_EXT2FS	"ext2fs"	/* Second Extended Filesystem */
#define	MOUNT_NCPFS	"ncpfs"		/* NetWare Network File System */
#define	MOUNT_XFS	"xfs"		/* xfs */
#define	MOUNT_NTFS	"ntfs"		/* NTFS */
#define	MOUNT_UDF	"udf"		/* UDF */

/*
 * Structure per mounted file system.  Each mounted file system has an
 * array of operations and an instance record.  The file systems are
 * put on a doubly linked list.
 */
LIST_HEAD(vnodelst, vnode);

struct mount {
	CIRCLEQ_ENTRY(mount) mnt_list;		/* mount list */
	const struct vfsops *mnt_op;		/* operations on fs */
	struct vfsconf  *mnt_vfc;               /* configuration info */
	struct vnode	*mnt_vnodecovered;	/* vnode we mounted on */
	struct vnode    *mnt_syncer;            /* syncer vnode */
	struct vnodelst	mnt_vnodelist;		/* list of vnodes this mount */
	struct lock     mnt_lock;               /* mount structure lock */
	int		mnt_flag;		/* flags */
	int		mnt_maxsymlinklen;	/* max size of short symlink */
	struct statfs	mnt_stat;		/* cache of filesystem stats */
	void		*mnt_data;		/* private data */
};

/*
 * Mount flags.
 *
 * Unmount uses MNT_FORCE flag.
 */
#define	MNT_RDONLY	0x00000001	/* read only filesystem */
#define	MNT_SYNCHRONOUS	0x00000002	/* file system written synchronously */
#define	MNT_NOEXEC	0x00000004	/* can't exec from filesystem */
#define	MNT_NOSUID	0x00000008	/* don't honor setuid bits on fs */
#define	MNT_NODEV	0x00000010	/* don't interpret special files */
#define	MNT_ASYNC	0x00000040	/* file system written asynchronously */

/*
 * exported mount flags.
 */
#define	MNT_EXRDONLY	0x00000080	/* exported read only */
#define	MNT_EXPORTED	0x00000100	/* file system is exported */
#define	MNT_DEFEXPORTED	0x00000200	/* exported to the world */
#define	MNT_EXPORTANON	0x00000400	/* use anon uid mapping for everyone */
#define	MNT_EXKERB	0x00000800	/* exported with Kerberos uid mapping */

/*
 * Flags set by internal operations.
 */
#define	MNT_LOCAL	0x00001000	/* filesystem is stored locally */
#define	MNT_QUOTA	0x00002000	/* quotas are enabled on filesystem */
#define	MNT_ROOTFS	0x00004000	/* identifies the root filesystem */

/*
 * Extra post 4.4BSD-lite2 mount flags.
 */
#define MNT_NOATIME	0x00008000	/* don't update access times on fs */

/*
 * Mask of flags that are visible to statfs()
 */
#define	MNT_VISFLAGMASK	0x0400ffff

/*
 * filesystem control flags.
 */
#define	MNT_UPDATE	0x00010000	/* not a real mount, just an update */
#define	MNT_DELEXPORT	0x00020000	/* delete export host lists */
#define	MNT_RELOAD	0x00040000	/* reload filesystem data */
#define	MNT_FORCE	0x00080000	/* force unmount or readonly change */
#define MNT_WANTRDWR	0x02000000	/* want upgrade to read/write */
#define MNT_SOFTDEP     0x04000000      /* soft dependencies being done */
#define MNT_DOOMED	0x08000000	/* device behind filesystem is gone */

/*
 * Sysctl CTL_VFS definitions.
 *
 * Second level identifier specifies which filesystem. Second level
 * identifier VFS_GENERIC returns information about all filesystems.
 */
#define	VFS_GENERIC	0	/* generic filesystem information */
/*
 * Third level identifiers for VFS_GENERIC are given below; third
 * level identifiers for specific filesystems are given in their
 * mount specific header files.
 */
#define VFS_MAXTYPENUM	1	/* int: highest defined filesystem type */
#define VFS_CONF	2	/* struct: vfsconf for filesystem given
				   as next argument */
#define	VFSGEN_MAXID	3	/* max number of vfs.generic ids */

#define	CTL_VFSGENCTL_NAMES { \
	{ 0, 0 }, \
	{ "maxtypenum", CTLTYPE_INT }, \
	{ "conf", CTLTYPE_NODE } \
}

/*
 * Filesystem configuration information. One of these exists for each
 * type of filesystem supported by the kernel. These are searched at
 * mount time to identify the requested filesystem.
 */
struct vfsconf {
	const struct vfsops *vfc_vfsops; /* filesystem operations vector */
	char	vfc_name[MFSNAMELEN];	/* filesystem type name */
	int	vfc_typenum;		/* historic filesystem type number */
	int	vfc_refcount;		/* number mounted of this type */
	int	vfc_flags;		/* permanent flags */
	int	(*vfc_mountroot)(void);	/* if != NULL, routine to mount root */
	struct	vfsconf *vfc_next;	/* next in list */
};

/*
 * Operations supported on mounted file system.
 */
#ifdef _KERNEL
#ifdef __STDC__
struct nameidata;
struct mbuf;
#endif

extern int maxvfsconf;		/* highest defined filesystem type */
extern struct vfsconf *vfsconf;	/* head of list of filesystem types */

struct vfsops {
	int	(*vfs_mount)(struct mount *mp, const char *path,
				    void *data,
				    struct nameidata *ndp, struct proc *p);
	int	(*vfs_start)(struct mount *mp, int flags,
				    struct proc *p);
	int	(*vfs_unmount)(struct mount *mp, int mntflags,
				    struct proc *p);
	int	(*vfs_root)(struct mount *mp, struct vnode **vpp);
	int	(*vfs_quotactl)(struct mount *mp, int cmds, uid_t uid,
				    caddr_t arg, struct proc *p);
	int	(*vfs_statfs)(struct mount *mp, struct statfs *sbp,
				    struct proc *p);
	int	(*vfs_sync)(struct mount *mp, int waitfor,
				    struct ucred *cred, struct proc *p);
	int	(*vfs_vget)(struct mount *mp, ino_t ino,
				    struct vnode **vpp);
	int	(*vfs_fhtovp)(struct mount *mp, struct fid *fhp,
				     struct vnode **vpp);
	int	(*vfs_vptofh)(struct vnode *vp, struct fid *fhp);
	int	(*vfs_init)(struct vfsconf *);
	int     (*vfs_sysctl)(int *, u_int, void *, size_t *, void *,
				     size_t, struct proc *);
	int	(*vfs_checkexp)(struct mount *mp, struct mbuf *nam,
				    int *extflagsp, struct ucred **credanonp);
};

#define VFS_MOUNT(MP, PATH, DATA, NDP, P) \
	(*(MP)->mnt_op->vfs_mount)(MP, PATH, DATA, NDP, P)
#define VFS_START(MP, FLAGS, P)	  (*(MP)->mnt_op->vfs_start)(MP, FLAGS, P)
#define VFS_UNMOUNT(MP, FORCE, P) (*(MP)->mnt_op->vfs_unmount)(MP, FORCE, P)
#define VFS_ROOT(MP, VPP)	  (*(MP)->mnt_op->vfs_root)(MP, VPP)
#define VFS_QUOTACTL(MP,C,U,A,P)  (*(MP)->mnt_op->vfs_quotactl)(MP, C, U, A, P)
#define VFS_STATFS(MP, SBP, P)	  (*(MP)->mnt_op->vfs_statfs)(MP, SBP, P)
#define VFS_SYNC(MP, WAIT, C, P)  (*(MP)->mnt_op->vfs_sync)(MP, WAIT, C, P)
#define VFS_VGET(MP, INO, VPP)	  (*(MP)->mnt_op->vfs_vget)(MP, INO, VPP)
#define VFS_FHTOVP(MP, FIDP, VPP) \
	(*(MP)->mnt_op->vfs_fhtovp)(MP, FIDP, VPP)
#define	VFS_VPTOFH(VP, FIDP)	  (*(VP)->v_mount->mnt_op->vfs_vptofh)(VP, FIDP)
#define VFS_CHECKEXP(MP, NAM, EXFLG, CRED) \
	(*(MP)->mnt_op->vfs_checkexp)(MP, NAM, EXFLG, CRED)
#endif /* _KERNEL */

/*
 * Flags for various system call interfaces.
 *
 * waitfor flags to vfs_sync() and getfsstat()
 */
#define MNT_WAIT	1	/* synchronously wait for I/O to complete */
#define MNT_NOWAIT	2	/* start all I/O, but do not wait for it */
#define MNT_LAZY	3	/* push data not written by filesystem syncer */

/*
 * Generic file handle
 */
struct fhandle {
	fsid_t	fh_fsid;	/* File system id of mount point */
	struct	fid fh_fid;	/* File sys specific id */
};
typedef struct fhandle	fhandle_t;

#ifdef _KERNEL
#include <net/radix.h>
#include <sys/socket.h>		/* XXX for AF_MAX */

/*
 * Network address lookup element
 */
struct netcred {
	struct	radix_node netc_rnodes[2];
	int	netc_exflags;
	struct	ucred netc_anon;
};

/*
 * Network export information
 */
struct netexport {
	struct	netcred ne_defexported;		      /* Default export */
	struct	radix_node_head *ne_rtable[AF_MAX+1]; /* Individual exports */
};
#endif /* _KERNEL */

#ifdef _KERNEL
/*
 * exported vnode operations
 */
int	vfs_busy(struct mount *, int, struct simplelock *);
int     vfs_isbusy(struct mount *);
int     vfs_mount_foreach_vnode(struct mount *, int (*func)(struct vnode *,
				    void *), void *);
void	vfs_getnewfsid(struct mount *);
struct	mount *vfs_getvfs(fsid_t *);
int	vfs_mountedon(struct vnode *);
int	vfs_mountroot(void);
int	vfs_rootmountalloc(char *, char *, struct mount **);
void	vfs_unbusy(struct mount *);
void	vfs_unmountall(void);
extern	CIRCLEQ_HEAD(mntlist, mount) mountlist;
extern	struct simplelock mountlist_slock;

struct	mount *getvfs(fsid_t *);	    /* return vfs given fsid */
					    /* process mount export info */
int	vfs_export(struct mount *, struct netexport *, struct export_args *);
					    /* lookup host in fs export list */
struct	netcred *vfs_export_lookup(struct mount *, struct netexport *,
	    struct mbuf *);
int	vfs_allocate_syncvnode(struct mount *);
int	speedup_syncer(void);

int	vfs_syncwait(int);	/* sync and wait for complete */
void	vfs_shutdown(void);	/* unmount and sync file systems */
long	makefstype(char *);
int	dounmount(struct mount *, int, struct proc *, struct vnode *);
void	vfsinit(void);
#ifdef DEBUG
void	vfs_bufstats(void);
#endif
int	vfs_register(struct vfsconf *);
int	vfs_unregister(struct vfsconf *);
#else /* _KERNEL */

#ifndef _SYS_STAT_H_
struct stat;
#endif

__BEGIN_DECLS
int	fstatfs(int, struct statfs *);
int	getfh(const char *, fhandle_t *);
int	getfsstat(struct statfs *, size_t, int);
int	getmntinfo(struct statfs **, int);
int	mount(const char *, const char *, int, void *);
int	statfs(const char *, struct statfs *);
int	unmount(const char *, int);
#if __BSD_VISIBLE
int	fhopen(const fhandle_t *, int);
int	fhstat(const fhandle_t *, struct stat *);
int	fhstatfs(const fhandle_t *, struct statfs *);
#endif /* __BSD_VISIBLE */

__END_DECLS

#endif /* _KERNEL */
#endif /* !_SYS_MOUNT_H_ */
