/*	$OpenBSD: namei.h,v 1.12 2003/06/02 23:28:21 millert Exp $	*/
/*	$NetBSD: namei.h,v 1.11 1996/02/09 18:25:20 christos Exp $	*/

/*
 * Copyright (c) 1985, 1989, 1991, 1993
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
 *	@(#)namei.h	8.4 (Berkeley) 8/20/94
 */

#ifndef _SYS_NAMEI_H_
#define	_SYS_NAMEI_H_

#include <sys/queue.h>

/*
 * Encapsulation of namei parameters.
 */
struct nameidata {
	/*
	 * Arguments to namei/lookup.
	 */
	const char *ni_dirp;		/* pathname pointer */
	enum	uio_seg ni_segflg;	/* location of pathname */
     /* u_long	ni_nameiop;		   namei operation */
     /* u_long	ni_flags;		   flags to namei */
     /* struct	proc *ni_proc;		   process requesting lookup */
	/*
	 * Arguments to lookup.
	 */
     /* struct	ucred *ni_cred;		   credentials */
	struct	vnode *ni_startdir;	/* starting directory */
	struct	vnode *ni_rootdir;	/* logical root directory */
	/*
	 * Results: returned from/manipulated by lookup
	 */
	struct	vnode *ni_vp;		/* vnode of result */
	struct	vnode *ni_dvp;		/* vnode of intermediate directory */
	/*
	 * Shared between namei and lookup/commit routines.
	 */
	size_t	ni_pathlen;		/* remaining chars in path */
	char	*ni_next;		/* next location in pathname */
	u_long	ni_loopcnt;		/* count of symlinks encountered */
	/*
	 * Lookup parameters: this structure describes the subset of
	 * information from the nameidata structure that is passed
	 * through the VOP interface.
	 */
	struct componentname {
		/*
		 * Arguments to lookup.
		 */
		u_long	cn_nameiop;	/* namei operation */
		u_long	cn_flags;	/* flags to namei */
		struct	proc *cn_proc;	/* process requesting lookup */
		struct	ucred *cn_cred;	/* credentials */
		/*
		 * Shared between lookup and commit routines.
		 */
		char	*cn_pnbuf;	/* pathname buffer */
		char	*cn_nameptr;	/* pointer to looked up name */
		long	cn_namelen;	/* length of looked up component */
		u_long	cn_hash;	/* hash value of looked up name */
		long	cn_consume;	/* chars to consume in lookup() */
	} ni_cnd;
};

#ifdef _KERNEL
/*
 * namei operations
 */
#define	LOOKUP		0	/* perform name lookup only */
#define	CREATE		1	/* setup for file creation */
#define	DELETE		2	/* setup for file deletion */
#define	RENAME		3	/* setup for file renaming */
#define	OPMASK		3	/* mask for operation */
/*
 * namei operational modifier flags, stored in ni_cnd.flags
 */
#define	LOCKLEAF	0x0004	/* lock inode on return */
#define	LOCKPARENT	0x0008	/* want parent vnode returned locked */
#define	WANTPARENT	0x0010	/* want parent vnode returned unlocked */
#define	NOCACHE		0x0020	/* name must not be left in cache */
#define	FOLLOW		0x0040	/* follow symbolic links */
#define	NOFOLLOW	0x0000	/* do not follow symbolic links (pseudo) */
#define	MODMASK		0x00fc	/* mask of operational modifiers */
/*
 * Namei parameter descriptors.
 *
 * SAVENAME may be set by either the callers of namei or by VOP_LOOKUP.
 * If the caller of namei sets the flag (for example execve wants to
 * know the name of the program that is being executed), then it must
 * free the buffer. If VOP_LOOKUP sets the flag, then the buffer must
 * be freed by either the commit routine or the VOP_ABORT routine.
 * SAVESTART is set only by the callers of namei. It implies SAVENAME
 * plus the addition of saving the parent directory that contains the
 * name in ni_startdir. It allows repeated calls to lookup for the
 * name being sought. The caller is responsible for releasing the
 * buffer and for vrele'ing ni_startdir.
 */
#define	NOCROSSMOUNT	0x000100      /* do not cross mount points */
#define	RDONLY		0x000200      /* lookup with read-only semantics */
#define	HASBUF		0x000400      /* has allocated pathname buffer */
#define	SAVENAME	0x000800      /* save pathanme buffer */
#define	SAVESTART	0x001000      /* save starting directory */
#define ISDOTDOT	0x002000      /* current component name is .. */
#define MAKEENTRY	0x004000      /* entry is to be added to name cache */
#define ISLASTCN	0x008000      /* this is last component of pathname */
#define ISSYMLINK	0x010000      /* symlink needs interpretation */
#define	ISWHITEOUT	0x020000      /* found whiteout */
#define	DOWHITEOUT	0x040000      /* do whiteouts */
#define	REQUIREDIR	0x080000      /* must be a directory */
#define STRIPSLASHES    0x100000      /* strip trailing slashes */
#define PDIRUNLOCK	0x200000      /* vfs_lookup() unlocked parent dir */

/*
 * Initialization of an nameidata structure.
 */
#define NDINIT(ndp, op, flags, segflg, namep, p) { \
	(ndp)->ni_cnd.cn_nameiop = op; \
	(ndp)->ni_cnd.cn_flags = flags; \
	(ndp)->ni_segflg = segflg; \
	(ndp)->ni_dirp = namep; \
	(ndp)->ni_cnd.cn_proc = p; \
}
#endif

/*
 * This structure describes the elements in the cache of recent
 * names looked up by namei. NCHNAMLEN is sized to make structure
 * size a power of two to optimize malloc's. Minimum reasonable
 * size is 15.
 */

#define	NCHNAMLEN	31	/* maximum name segment length we bother with */

struct	namecache {
	LIST_ENTRY(namecache) nc_hash;	/* hash chain */
	TAILQ_ENTRY(namecache) nc_lru;	/* LRU chain */
	struct	vnode *nc_dvp;		/* vnode of parent of name */
	u_long	nc_dvpid;		/* capability number of nc_dvp */
	struct	vnode *nc_vp;		/* vnode the name refers to */
	u_long	nc_vpid;		/* capability number of nc_vp */
	char	nc_nlen;		/* length of name */
	char	nc_name[NCHNAMLEN];	/* segment name */
};

#ifdef _KERNEL
extern u_long nextvnodeid;
int	namei(struct nameidata *ndp);
int	lookup(struct nameidata *ndp);
int	relookup(struct vnode *dvp, struct vnode **vpp,
		      struct componentname *cnp);
void cache_purge(struct vnode *);
int cache_lookup(struct vnode *, struct vnode **, struct componentname *);
void cache_enter(struct vnode *, struct vnode *, struct componentname *);
void nchinit(void);
struct mount;
void cache_purgevfs(struct mount *);

#endif

/*
 * Stats on usefulness of namei caches.
 */
struct	nchstats {
	long	ncs_goodhits;		/* hits that we can really use */
	long	ncs_neghits;		/* negative hits that we can use */
	long	ncs_badhits;		/* hits we must drop */
	long	ncs_falsehits;		/* hits with id mismatch */
	long	ncs_miss;		/* misses */
	long	ncs_long;		/* long names that ignore cache */
	long	ncs_pass2;		/* names found with passes == 2 */
	long	ncs_2passes;		/* number of times we attempt it */
};

/* These sysctl names are only really used by sysctl(8) */
#define KERN_NCHSTATS_GOODHITS		1
#define KERN_NCHSTATS_NEGHITS		2
#define KERN_NCHSTATS_BADHITS		3
#define KERN_NCHSTATS_FALSEHITS		4
#define KERN_NCHSTATS_MISS		5
#define KERN_NCHSTATS_LONG		6
#define KERN_NCHSTATS_PASS2		7
#define KERN_NCHSTATS_2PASSES		8
#define KERN_NCHSTATS_MAXID		9

#define CTL_KERN_NCHSTATS_NAMES { \
	{ 0, 0 }, \
	{ "good_hits", CTLTYPE_INT }, \
	{ "negative_hits", CTLTYPE_INT }, \
	{ "bad_hits", CTLTYPE_INT }, \
	{ "false_hits", CTLTYPE_INT }, \
	{ "misses", CTLTYPE_INT }, \
	{ "long_names", CTLTYPE_INT }, \
	{ "pass2", CTLTYPE_INT }, \
	{ "2passes", CTLTYPE_INT }, \
}
#endif /* !_SYS_NAMEI_H_ */
