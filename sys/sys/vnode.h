/*	$OpenBSD: vnode.h,v 1.88 2007/06/14 20:36:34 otto Exp $	*/
/*	$NetBSD: vnode.h,v 1.38 1996/02/29 20:59:05 cgd Exp $	*/

/*
 * Copyright (c) 1989, 1993
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
 *	@(#)vnode.h	8.11 (Berkeley) 11/21/94
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/selinfo.h>

#include <uvm/uvm.h>
#include <uvm/uvm_vnode.h>

/*
 * The vnode is the focus of all file activity in UNIX.  There is a
 * unique vnode allocated for each active file, each current directory,
 * each mounted-on file, text file, and the root.
 */

/*
 * Vnode types.  VNON means no type.
 */
enum vtype	{ VNON, VREG, VDIR, VBLK, VCHR, VLNK, VSOCK, VFIFO, VBAD };

#define	VTYPE_NAMES \
    "VNON", "VREG", "VDIR", "VBLK", "VCHR", "VLNK", "VSOCK", "VFIFO", "VBAD"

/*
 * Vnode tag types.
 * These are for the benefit of external programs only (e.g., pstat)
 * and should NEVER be inspected by the kernel.
 *
 * Note that v_tag is actually used to tell MFS from FFS, and EXT2FS from
 * the rest, so don't believe the above comment!
 */
enum vtagtype	{
	VT_NON, VT_UFS, VT_NFS, VT_MFS, VT_MSDOSFS,
	VT_PORTAL, VT_PROCFS, VT_AFS, VT_ISOFS, VT_ADOSFS,
	VT_EXT2FS, VT_VFS, VT_XFS, VT_NTFS, VT_UDF
};

#define	VTAG_NAMES \
    "NON", "UFS", "NFS", "MFS", "MSDOSFS",			\
    "PORTAL", "PROCFS", "AFS", "ISOFS", "ADOSFS",		\
    "EXT2FS", "VFS", "XFS", "NTFS", "UDF"

/*
 * Each underlying filesystem allocates its own private area and hangs
 * it from v_data.  If non-null, this area is freed in getnewvnode().
 */
LIST_HEAD(buflists, buf);

struct vnode {
	struct uvm_vnode v_uvm;			/* uvm data */
	int	(**v_op)(void *);		/* vnode operations vector */
	enum	vtype v_type;			/* vnode type */
	u_int	v_flag;				/* vnode flags (see below) */
	u_int   v_usecount;			/* reference count of users */
	/* reference count of writers */
	u_int   v_writecount;
	/* Flags that can be read/written in interrupts */
	u_int   v_bioflag;
	u_int   v_holdcnt;			/* buffer references */
	u_int   v_id;				/* capability identifier */
	struct	mount *v_mount;			/* ptr to vfs we are in */
	TAILQ_ENTRY(vnode) v_freelist;		/* vnode freelist */
	LIST_ENTRY(vnode) v_mntvnodes;		/* vnodes for mount point */
	struct	buflists v_cleanblkhd;		/* clean blocklist head */
	struct	buflists v_dirtyblkhd;		/* dirty blocklist head */
	u_int   v_numoutput;			/* num of writes in progress */
	LIST_ENTRY(vnode) v_synclist;		/* vnode with dirty buffers */
	union {
		struct mount	*vu_mountedhere;/* ptr to mounted vfs (VDIR) */
		struct socket	*vu_socket;	/* unix ipc (VSOCK) */
		struct specinfo	*vu_specinfo;	/* device (VCHR, VBLK) */
		struct fifoinfo	*vu_fifoinfo;	/* fifo (VFIFO) */
	} v_un;

	enum	vtagtype v_tag;			/* type of underlying data */
	void	*v_data;			/* private data for fs */
	struct	selinfo v_selectinfo;		/* identity of poller(s) */
};
#define	v_mountedhere	v_un.vu_mountedhere
#define	v_socket	v_un.vu_socket
#define	v_specinfo	v_un.vu_specinfo
#define	v_fifoinfo	v_un.vu_fifoinfo

/*
 * Vnode flags.
 */
#define	VROOT		0x0001	/* root of its file system */
#define	VTEXT		0x0002	/* vnode is a pure text prototype */
#define	VSYSTEM		0x0004	/* vnode being used by kernel */
#define	VISTTY		0x0008	/* vnode represents a tty */
#define	VXLOCK		0x0100	/* vnode is locked to change underlying type */
#define	VXWANT		0x0200	/* process is waiting for vnode */
#define	VCLONED		0x0400	/* vnode was cloned */
#define	VALIASED	0x0800	/* vnode has an alias */
#define	VLOCKSWORK	0x4000	/* FS supports locking discipline */
#define	VBITS	"\010\001ROOT\002TEXT\003SYSTEM\004ISTTY\010XLOCK" \
    "\011XWANT\013ALIASED\016LOCKSWORK"

/*
 * (v_bioflag) Flags that may be manipulated by interrupt handlers
 */
#define	VBIOWAIT	0x0001	/* waiting for output to complete */
#define VBIOONSYNCLIST	0x0002	/* Vnode is on syncer worklist */
#define VBIOONFREELIST  0x0004  /* Vnode is on a free list */

/*
 * Vnode attributes.  A field value of VNOVAL represents a field whose value
 * is unavailable (getattr) or which is not to be changed (setattr).
 */
struct vattr {
	enum vtype	va_type;	/* vnode type (for create) */
	mode_t		va_mode;	/* files access mode and type */
	nlink_t		va_nlink;	/* number of references to file */
	uid_t		va_uid;		/* owner user id */
	gid_t		va_gid;		/* owner group id */
	long		va_fsid;	/* file system id (dev for now) */
	long		va_fileid;	/* file id */
	u_quad_t	va_size;	/* file size in bytes */
	long		va_blocksize;	/* blocksize preferred for i/o */
	struct timespec	va_atime;	/* time of last access */
	struct timespec	va_mtime;	/* time of last modification */
	struct timespec	va_ctime;	/* time file changed */
	u_long		va_gen;		/* generation number of file */
	u_long		va_flags;	/* flags defined for file */
	dev_t		va_rdev;	/* device the special file represents */
	u_quad_t	va_bytes;	/* bytes of disk space held by file */
	u_quad_t	va_filerev;	/* file modification number */
	u_int		va_vaflags;	/* operations flags, see below */
	long		va_spare;	/* remain quad aligned */
};

/*
 * Flags for va_cflags.
 */
#define	VA_UTIMES_NULL	0x01		/* utimes argument was NULL */
#define VA_EXCLUSIVE    0x02		/* exclusive create request */
/*
 * Flags for ioflag.
 */
#define	IO_UNIT		0x01		/* do I/O as atomic unit */
#define	IO_APPEND	0x02		/* append write to end */
#define	IO_SYNC		0x04		/* do I/O synchronously */
#define	IO_NODELOCKED	0x08		/* underlying node already locked */
#define	IO_NDELAY	0x10		/* FNDELAY flag set in file table */
#define	IO_NOLIMIT	0x20		/* don't enforce limits on i/o */

/*
 *  Modes.  Some values same as Ixxx entries from inode.h for now.
 */
#define	VSUID	04000		/* set user id on execution */
#define	VSGID	02000		/* set group id on execution */
#define	VSVTX	01000		/* save swapped text even after use */
#define	VREAD	00400		/* read, write, execute permissions */
#define	VWRITE	00200
#define	VEXEC	00100

/*
 * Token indicating no attribute value yet assigned.
 */
#define	VNOVAL	(-1)

/*
 * Structure returned by the KERN_VNODE sysctl
 */
struct e_vnode {
	struct vnode *vptr;
	struct vnode vnode;
};

#ifdef _KERNEL
/*
 * Convert between vnode types and inode formats (since POSIX.1
 * defines mode word of stat structure in terms of inode formats).
 */
extern enum vtype	iftovt_tab[];
extern int		vttoif_tab[];
#define IFTOVT(mode)	(iftovt_tab[((mode) & S_IFMT) >> 12])
#define VTTOIF(indx)	(vttoif_tab[(int)(indx)])
#define MAKEIMODE(indx, mode)	(int)(VTTOIF(indx) | (mode))

/*
 * Flags to various vnode functions.
 */
#define	SKIPSYSTEM	0x0001		/* vflush: skip vnodes marked VSYSTEM */
#define	FORCECLOSE	0x0002		/* vflush: force file closeure */
#define	WRITECLOSE	0x0004		/* vflush: only close writeable files */
#define	DOCLOSE		0x0008		/* vclean: close active files */
#define	V_SAVE		0x0001		/* vinvalbuf: sync file first */
#define	V_SAVEMETA	0x0002		/* vinvalbuf: leave indirect blocks */

#define REVOKEALL	0x0001		/* vop_revoke: revoke all aliases */


TAILQ_HEAD(freelst, vnode);
extern struct freelst vnode_hold_list;	/* free vnodes referencing buffers */
extern struct freelst vnode_free_list;	/* vnode free list */

#define	VATTR_NULL(vap)	vattr_null(vap)
#define	VREF(vp)	vref(vp)		/* increase reference */
#define	NULLVP	((struct vnode *)NULL)
#define	VN_KNOTE(vp, b)					\
	KNOTE(&vp->v_selectinfo.si_note, (b))

/*
 * Global vnode data.
 */
extern	struct vnode *rootvnode;	/* root (i.e. "/") vnode */
extern	int desiredvnodes;		/* XXX number of vnodes desired */
extern	int maxvnodes;			/* XXX number of vnodes to allocate */
extern	time_t syncdelay;		/* time to delay syncing vnodes */
extern	int rushjob;			/* # of slots syncer should run ASAP */
#endif /* _KERNEL */


/*
 * Mods for exensibility.
 */

/*
 * Flags for vdesc_flags:
 */
#define VDESC_MAX_VPS		16
/* Low order 16 flag bits are reserved for willrele flags for vp arguments. */
#define VDESC_VP0_WILLRELE      0x00000001
#define VDESC_VP1_WILLRELE      0x00000002
#define VDESC_VP2_WILLRELE      0x00000004
#define VDESC_VP3_WILLRELE      0x00000008
#define VDESC_VP0_WILLUNLOCK    0x00000100
#define VDESC_VP1_WILLUNLOCK    0x00000200
#define VDESC_VP2_WILLUNLOCK    0x00000400
#define VDESC_VP3_WILLUNLOCK    0x00000800
#define VDESC_VP0_WILLPUT       0x00000101
#define VDESC_VP1_WILLPUT       0x00000202
#define VDESC_VP2_WILLPUT       0x00000404
#define VDESC_VP3_WILLPUT       0x00000808
#define VDESC_NOMAP_VPP         0x00010000
#define VDESC_VPP_WILLRELE      0x00020000

/*
 * VDESC_NO_OFFSET is used to identify the end of the offset list
 * and in places where no such field exists.
 */
#define VDESC_NO_OFFSET -1

/*
 * This structure describes the vnode operation taking place.
 */
struct vnodeop_desc {
	int	vdesc_offset;		/* offset in vector--first for speed */
	char    *vdesc_name;		/* a readable name for debugging */
	int	vdesc_flags;		/* VDESC_* flags */

	/*
	 * These ops are used by bypass routines to map and locate arguments.
	 * Creds and procs are not needed in bypass routines, but sometimes
	 * they are useful to (for example) transport layers.
	 * Nameidata is useful because it has a cred in it.
	 */
	int	*vdesc_vp_offsets;	/* list ended by VDESC_NO_OFFSET */
	int	vdesc_vpp_offset;	/* return vpp location */
	int	vdesc_cred_offset;	/* cred location, if any */
	int	vdesc_proc_offset;	/* proc location, if any */
	int	vdesc_componentname_offset; /* if any */
	/*
	 * Finally, we've got a list of private data (about each operation)
	 * for each transport layer.  (Support to manage this list is not
	 * yet part of BSD.)
	 */
	caddr_t	*vdesc_transports;
};

#ifdef _KERNEL
/*
 * A list of all the operation descs.
 */
extern struct vnodeop_desc *vnodeop_descs[];


/*
 * This macro is very helpful in defining those offsets in the vdesc struct.
 *
 * This is stolen from X11R4.  I ingored all the fancy stuff for
 * Crays, so if you decide to port this to such a serious machine,
 * you might want to consult Intrisics.h's XtOffset{,Of,To}.
 */
#define VOPARG_OFFSET(p_type,field) \
	((int) (((char *) (&(((p_type)NULL)->field))) - ((char *) NULL)))
#define VOPARG_OFFSETOF(s_type,field) \
	VOPARG_OFFSET(s_type*,field)
#define VOPARG_OFFSETTO(S_TYPE,S_OFFSET,STRUCT_P) \
	((S_TYPE)(((char *)(STRUCT_P))+(S_OFFSET)))


/*
 * This structure is used to configure the new vnodeops vector.
 */
struct vnodeopv_entry_desc {
	struct vnodeop_desc *opve_op;   /* which operation this is */
	int (*opve_impl)(void *);	/* code implementing this operation */
};
struct vnodeopv_desc {
			/* ptr to the ptr to the vector where op should go */
	int (***opv_desc_vector_p)(void *);
	struct vnodeopv_entry_desc *opv_desc_ops;   /* null terminated list */
};

/*
 * A default routine which just returns an error.
 */
int vn_default_error(void *);

/*
 * A generic structure.
 * This can be used by bypass routines to identify generic arguments.
 */
struct vop_generic_args {
	struct vnodeop_desc *a_desc;
	/* other random data follows, presumably */
};

/*
 * VOCALL calls an op given an ops vector.  We break it out because BSD's
 * vclean changes the ops vector and then wants to call ops with the old
 * vector.
 */
#define VOCALL(OPSV,OFF,AP) (( *((OPSV)[(OFF)])) (AP))

/*
 * This call works for vnodes in the kernel.
 */
#define VCALL(VP,OFF,AP) VOCALL((VP)->v_op,(OFF),(AP))
#define VDESC(OP) (& __CONCAT(OP,_desc))
#define VOFFSET(OP) (VDESC(OP)->vdesc_offset)

/*
 * Finally, include the default set of vnode operations.
 */
#include <sys/vnode_if.h>

/*
 * Public vnode manipulation functions.
 */
struct file;
struct filedesc;
struct mount;
struct nameidata;
struct proc;
struct stat;
struct ucred;
struct uio;
struct vattr;
struct vnode;

/* vfs_subr */
int	bdevvp(dev_t, struct vnode **);
int	cdevvp(dev_t, struct vnode **);
struct vnode *checkalias(struct vnode *, dev_t, struct mount *);
int	getnewvnode(enum vtagtype, struct mount *, int (**vops)(void *),
	    struct vnode **);
int	vaccess(mode_t, uid_t, gid_t, mode_t, struct ucred *);
void	vattr_null(struct vattr *);
void	vdevgone(int, int, int, enum vtype);
int	vcount(struct vnode *);
int	vfinddev(dev_t, enum vtype, struct vnode **);
void	vflushbuf(struct vnode *, int);
int	vflush(struct mount *, struct vnode *, int);
int	vget(struct vnode *, int, struct proc *);
void	vgone(struct vnode *);
void	vgonel(struct vnode *, struct proc *);
int	vinvalbuf(struct vnode *, int, struct ucred *, struct proc *,
	    int, int);
void	vntblinit(void);
int	vwaitforio(struct vnode *, int, char *, int);
void	vwakeup(struct vnode *);
void	vput(struct vnode *);
int	vrecycle(struct vnode *, struct proc *);
void	vrele(struct vnode *);
void	vref(struct vnode *);
void	vprint(char *, struct vnode *);

/* vfs_getcwd.c */
int vfs_getcwd_scandir(struct vnode **, struct vnode **, char **, char *,
    struct proc *);
int vfs_getcwd_common(struct vnode *, struct vnode *, char **, char *, int,
    int, struct proc *);
int vfs_getcwd_getcache(struct vnode **, struct vnode **, char **, char *);

/* vfs_default.c */
int	vop_generic_abortop(void *);
int	vop_generic_bwrite(void *);
int	vop_generic_islocked(void *);
int	vop_generic_lock(void *);
int	vop_generic_unlock(void *);
int	vop_generic_revoke(void *);
int	vop_generic_kqfilter(void *);

/* vfs_vnops.c */
int	vn_isunder(struct vnode *, struct vnode *, struct proc *);
int	vn_close(struct vnode *, int, struct ucred *, struct proc *);
int	vn_open(struct nameidata *, int, int);
int	vn_rdwr(enum uio_rw, struct vnode *, caddr_t, int, off_t,
	    enum uio_seg, int, struct ucred *, size_t *, struct proc *);
int	vn_stat(struct vnode *, struct stat *, struct proc *);
int	vn_statfile(struct file *, struct stat *, struct proc *);
int	vn_lock(struct vnode *, int, struct proc *);
int	vn_writechk(struct vnode *);
int	vn_ioctl(struct file *, u_long, caddr_t, struct proc *);
void	vn_marktext(struct vnode *);

/* vfs_sync.c */
void	sched_sync(struct proc *);
void	vn_initialize_syncerd(void);
void	vn_syncer_add_to_worklist(struct vnode *, int);

/* misc */
int	vn_isdisk(struct vnode *, int *);
int	softdep_fsync(struct vnode *);
int 	getvnode(struct filedesc *, int, struct file **);

#endif /* _KERNEL */
