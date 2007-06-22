/*	$OpenBSD: procfs.h,v 1.24 2007/06/22 09:38:53 jasper Exp $	*/
/*	$NetBSD: procfs.h,v 1.17 1996/02/12 15:01:41 christos Exp $	*/

/*
 * Copyright (c) 1993 Jan-Simon Pendry
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
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
 *	@(#)procfs.h	8.7 (Berkeley) 6/15/94
 */

/*
 * The different types of node in a procfs filesystem
 */
typedef enum {
	Proot,		/* the filesystem root */
	Pcurproc,	/* symbolic link for curproc */
	Pself,		/* like curproc, but this is the Linux name */
	Pproc,		/* a process-specific sub-directory */
	Pfile,		/* the executable file */
	Pmem,		/* the process's memory image */
	Pregs,		/* the process's register set */
	Pfpregs,	/* the process's FP register set */
	Pctl,		/* process control */
	Pstatus,	/* process status */
	Pnote,		/* process notifier */
	Pnotepg,	/* process group notifier */
	Pcmdline,	/* process command line args */
	Pmeminfo,	/* system memory info (if -o linux) */
	Pcpuinfo	/* CPU info (if -o linux) */
} pfstype;

/*
 * control data for the proc file system.
 */
struct pfsnode {
	TAILQ_ENTRY(pfsnode)	list;
	struct vnode	*pfs_vnode;	/* vnode associated with this pfsnode */
	pfstype		pfs_type;	/* type of procfs node */
	pid_t		pfs_pid;	/* associated process */
	mode_t		pfs_mode;	/* mode bits for stat() */
	u_long		pfs_flags;	/* open flags */
	u_long		pfs_fileno;	/* unique file id */
};

#define PROCFS_NOTELEN	64	/* max length of a note (/proc/$pid/note) */
#define PROCFS_CTLLEN 	8	/* max length of a ctl msg (/proc/$pid/ctl */

/*
 * Kernel stuff follows
 */
#ifdef _KERNEL
#define CNEQ(cnp, s, len) \
	 ((cnp)->cn_namelen == (len) && \
	  (bcmp((s), (cnp)->cn_nameptr, (len)) == 0))

#define UIO_MX 32

#define PROCFS_FILENO(pid, type) \
	(((type) < Pproc) ? \
			((type) + 4) : \
			((((pid)+1) << 5) + ((int) (type))))

struct procfsmount {
	void *pmnt_exechook;
	int pmnt_flags;
};

#define VFSTOPROC(mp)	((struct procfsmount *)(mp)->mnt_data)

/*
 * Convert between pfsnode vnode
 */
#define VTOPFS(vp)	((struct pfsnode *)(vp)->v_data)
#define PFSTOV(pfs)	((pfs)->pfs_vnode)

typedef struct vfs_namemap vfs_namemap_t;
struct vfs_namemap {
	const char *nm_name;
	int nm_val;
};

int vfs_getuserstr(struct uio *, char *, int *);
const vfs_namemap_t *vfs_findname(const vfs_namemap_t *, char *, int);

int procfs_allocvp(struct mount *, struct vnode **, pid_t, pfstype);
int procfs_doctl(struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio);
int procfs_dofpregs(struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio);
int procfs_donote(struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio);
int procfs_doregs(struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio);
int procfs_dostatus(struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio);
int procfs_docmdline(struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio);
int procfs_domeminfo(struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio);
int procfs_docpuinfo(struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio);
int procfs_domap(struct proc *, struct proc *, struct pfsnode *pfsp, struct uio *uio, int);
int procfs_freevp(struct vnode *);
int procfs_getcpuinfstr(char *, int *);
int procfs_poll(void *);

/* functions to check whether or not files should be displayed */
int procfs_validfile(struct proc *, struct mount *);
int procfs_validfpregs(struct proc *, struct mount *);
int procfs_validregs(struct proc *, struct mount *);
int procfs_validmap(struct proc *, struct mount *);

int procfs_rw(void *);

#define PROCFS_LOCKED	0x01
#define PROCFS_WANT	0x02

extern int (**procfs_vnodeop_p)(void *);
extern const struct vfsops procfs_vfsops;

struct vfsconf;

int	procfs_init(struct vfsconf *);
int	procfs_root(struct mount *, struct vnode **);

#endif /* _KERNEL */
