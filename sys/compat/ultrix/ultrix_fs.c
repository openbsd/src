/*	$OpenBSD: ultrix_fs.c,v 1.11 2002/07/20 19:24:57 art Exp $	*/
/*	$NetBSD: ultrix_fs.c,v 1.4 1996/04/07 17:23:06 jonathan Exp $	*/

/*
 * Copyright (c) 1995
 *	Jonathan Stone (hereinafter referred to as the author)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/exec.h>
#include <sys/namei.h>
#include <sys/mount.h>
#include <net/if.h>
#include <netinet/in.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <sys/syscallargs.h>
#include <compat/ultrix/ultrix_syscallargs.h>

#include <uvm/uvm_extern.h>

#define	ULTRIX_MAXPATHLEN	1024

/**
 ** Ultrix filesystem operations: mount(), getmnt().
 ** These are included purely so one can place an (ECOFF or ELF)
 ** NetBSD/pmax kernel in an Ultrix root filesystem, boot it,
 ** and over-write the Ultrix root parition with NetBSD binaries.
 **/

/*
 * Ultrix file system data structure, as modified by
 * Ultrix getmntent(). This  structure is padded to 2560 bytes, for
 * compatibility with the size the Ultrix kernel and user apps expect.
 */
struct ultrix_fs_data {
	u_int32_t	ufsd_flags;	/* how mounted */
	u_int32_t	ufsd_mtsize;	/* max transfer size in bytes */
	u_int32_t	ufsd_otsize;	/* optimal transfer size in bytes */
	u_int32_t	ufsd_bsize;	/* fs block size (bytes) for vm code */
	u_int32_t	ufsd_fstype;	/* see ../h/fs_types.h  */
	u_int32_t	ufsd_gtot;	/* total number of gnodes */
	u_int32_t	ufsd_gfree;	/* # of free gnodes */
	u_int32_t	ufsd_btot;	/* total number of 1K blocks */
	u_int32_t	ufsd_bfree;	/* # of free 1K blocks */
	u_int32_t	ufsd_bfreen;	/* user consumable 1K blocks */
	u_int32_t	ufsd_pgthresh;	/* min size in bytes before paging*/
	int32_t		ufsd_uid;	/* uid that mounted me */
	int16_t		ufsd_dev;	/* major/minor of fs */
	int16_t		ufsd_exroot;	/* root mapping from exports */
	char		ufsd_devname[ULTRIX_MAXPATHLEN + 4]; /* name of dev */
	char		ufsd_path[ULTRIX_MAXPATHLEN + 4]; /* name of mnt point */
	u_int32_t	ufsd_nupdate;	/* number of writes */
	u_int32_t	ufsd_pad[112];	/* pad to 2560 bytes. */
};

/*
 * Get statistics on mounted filesystems.
 */
#if 0
struct ultrix_getmnt_args {
	int32_t *start;
	struct ultrix_fs_data *buf;
	int32_t bufsize;
	int32_t mode;
	char *path;
};

#endif
/*
 * Ultrix getmnt() flags.
 * The operation getmnt() should perform is incoded in the flag
 * argument.  There are two independent attributes.
 *
 * ULTRIX_NOSTAT_xxx will never hang, but it may not return
 * up-to-date statistics. (For NFS clients, it returns whatever is
 * in the cache.) ULTRIX_STAT_xxx returns up-to-date info but may
 * hang (e.g., on dead NFS servers).
 *
 * ULTRIX_xxSTAT_ONE returns statistics on just one filesystem, determined
 * by the parth argument.  ULTRIX_xxSTAT_MANY ignores the path argument and
 * returns info on as many  filesystems fit in the structure.
 * the start argument, which should be zero on the first call,
 * can be used to iterate over all filesystems.
 *
 */
#define	ULTRIX_NOSTAT_MANY	1
#define	ULTRIX_STAT_MANY	2
#define	ULTRIX_STAT_ONE		3
#define	ULTRIX_NOSTAT_ONE	4

/*
 * Ultrix gnode-layer  filesystem codes.
 */
#define ULTRIX_FSTYPE_UNKNOWN	0x0
#define ULTRIX_FSTYPE_ULTRIX	0x1	/*  Ultrix UFS: basically 4.2bsd FFS */
#define ULTRIX_FSTYPE_NFS	0x5	/*  NFS v2 */

/*
 * Ultrix mount(2) options
 */
#define ULTRIX_NM_RONLY    0x0001  /* mount read-only */
#define ULTRIX_NM_SOFT     0x0002  /* soft mount (hard is default) */
#define ULTRIX_NM_WSIZE    0x0004  /* set write size */
#define ULTRIX_NM_RSIZE    0x0008  /* set read size */
#define ULTRIX_NM_TIMEO    0x0010  /* set initial timeout */
#define ULTRIX_NM_RETRANS  0x0020  /* set number of request retrys */
#define ULTRIX_NM_HOSTNAME 0x0040  /* set hostname for error printf */
#define ULTRIX_NM_PGTHRESH 0x0080  /* set page threshold for exec */
#define ULTRIX_NM_INT      0x0100  /* allow hard mount keyboard interrupts */
#define ULTRIX_NM_NOAC     0x0200  /* don't cache attributes */
									

/*
 * Construct an Ultrix getmnt() ultrix_fs_data from the native NetBSD
 * struct statfs.
 */
static void
make_ultrix_mntent(sp, tem)
	register struct statfs *sp;
	register struct ultrix_fs_data *tem;
{

	bzero(tem, sizeof (*tem));

	tem->ufsd_flags = sp->f_flags;		/* XXX translate */
	tem->ufsd_mtsize = sp->f_bsize;		/* XXX max transfer size */
	tem->ufsd_otsize = sp->f_iosize;
	tem->ufsd_bsize = sp->f_bsize;
	/*
	 * Translate file system type. NetBSD/1.1 has f_type zero,
	 * and uses an fstype string instead.
	 * For now, map types not in Ultrix (kernfs, null, procfs...)
	 * to UFS, since Ultrix mout will try and call mount_unknown
	 * for ULTRIX_FSTYPE_UNKNOWN, but lacks a mount_unknown binary.
	 */
	tem->ufsd_fstype = ULTRIX_FSTYPE_NFS;
	if (strcmp(sp->f_fstypename, "ffs") == 0)
		tem->ufsd_fstype = ULTRIX_FSTYPE_ULTRIX;

	tem->ufsd_gtot = sp->f_files;		/* total "gnodes" */
	tem->ufsd_gfree = sp->f_ffree;		/* free "gnodes" */
	tem->ufsd_btot = sp->f_blocks;		/* total 1k blocks */
#ifdef needsmorethought	/* XXX */
	/* tem->ufsd_bfree = sp->f_bfree; */	/* free 1k blocks */
	/* tem->ufsd_bfree = sp->f_bavail; */	/* free 1k blocks */
#endif

	tem->ufsd_bfreen = sp->f_bavail;	/* blocks available to users */
	tem->ufsd_pgthresh = 0;			/* not relevant */
	tem->ufsd_uid = 0;			/* XXX kept where ?*/
	tem->ufsd_dev = 0;			/* ?? */
	tem->ufsd_exroot  = 0;			/* ?? */
	strncpy(tem->ufsd_path, sp->f_mntonname, ULTRIX_MAXPATHLEN);
	strncpy(tem->ufsd_devname, sp->f_mntfromname, ULTRIX_MAXPATHLEN);
#if 0
	/* In NetBSD-1.1, filesystem type is unused and always 0 */
	printf("mntent: %s type %d\n", tem->ufsd_devname, tem->ufsd_fstype);
	printf("mntent: %s tot %d free %d user%d\n",
	 tem->ufsd_devname, sp->f_blocks, sp->f_bfree, sp->f_bavail);
#endif
}

int
ultrix_sys_getmnt(p, v, retval)
	struct proc *p;
	void *v;
	int *retval;
{
	struct ultrix_sys_getmnt_args *uap = v;
	struct mount *mp, *nmp;
	struct statfs *sp;
	struct ultrix_fs_data *sfsp;
	char *path;
	int mntflags;
	int skip;
	int start;
	long count, maxcount;
	int error = 0;

	path = NULL;
	error = 0;
	maxcount = SCARG(uap, bufsize) / sizeof(struct ultrix_fs_data);
	sfsp = SCARG(uap, buf);

	if (SCARG(uap, mode) == ULTRIX_STAT_ONE ||
	    SCARG(uap, mode) == ULTRIX_STAT_MANY)
		mntflags = MNT_WAIT;
	else
		mntflags = MNT_NOWAIT;

	if (SCARG(uap, mode) == ULTRIX_STAT_ONE || SCARG(uap, mode) == ULTRIX_NOSTAT_ONE) {
		/*
		 * Only get info on mountpoints that matches the path
		 * provided.
		 */
		MALLOC(path, char *, MAXPATHLEN, M_TEMP, M_WAITOK);
		if ((error = copyinstr(SCARG(uap, path), path,
				       MAXPATHLEN, NULL)) != 0)
			goto bad;
		maxcount = 1;
	} else {
		/*
		 * Get info on any mountpoints, somewhat like readdir().
		 * Find out how many mount list entries to skip, and skip
		 * them.
		 */
		if ((error = copyin((caddr_t)SCARG(uap, start), &start,
				    sizeof(*SCARG(uap, start))))  != 0)
			goto bad;
		for (skip = start, mp = mountlist.cqh_first;
		    mp != (void *)&mountlist && skip-- > 0; mp = nmp)
			nmp = mp->mnt_list.cqe_next;
	}

	for (count = 0, mp = mountlist.cqh_first;
	    mp != (void *)&mountlist && count < maxcount; mp = nmp) {
		nmp = mp->mnt_list.cqe_next;
		if (sfsp != NULL) {
			struct ultrix_fs_data tem;
			sp = &mp->mnt_stat;

			/*
			 * If requested, refresh the fsstat cache.
			 */
			if ((mntflags & MNT_WAIT) != 0 &&
			    (error = VFS_STATFS(mp, sp, p)) != 0)
				continue;

			/*
			 * XXX what does this do? -- cgd
			 */
			sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
			if (path == NULL ||
			    strcmp(path, sp->f_mntonname) == 0) {
				make_ultrix_mntent(sp, &tem);
				if ((error = copyout((caddr_t)&tem, sfsp,
						     sizeof(tem))) != 0)
					goto bad;
				sfsp++;
				count++;
			}
		}
	}

	if (sfsp != NULL && count > maxcount)
		*retval = maxcount;
	else
		*retval = count;

bad:
	if (path)
		FREE(path, M_TEMP);
	return (error);
}



/* Old-style inet sockaddr (no len field) as passed to Ultrix mount(2) */
struct osockaddr_in {
	short   sin_family;
	u_short sin_port;
	struct  in_addr sin_addr;
	char    sin_zero[8];
};


/*
 * fstype-dependent structure passed to Ultrix mount(2) when
 * mounting NFS filesystems
 */
struct	ultrix_nfs_args {
	struct	osockaddr_in *addr;	/* file server address */
	void	*fh;			/* file handle to be mounted */
	int	flags;			/* flags */
	int	wsize;			/* write size in bytes */
	int	rsize;			/* read size in bytes */
	int	timeo;			/* initial timeout in .1 secs */
	int	retrans;		/* times to retry send */
	char	*hostname;		/* server's hostname */
	char	*optstr;		/* string of nfs mount options*/
	int	gfs_flags;		/* gnode flags (ugh) */
	int	pg_thresh;		/* paging threshold ? */
};


/*
 * fstype-dependent structure passed to Ultrix mount(2) when
 * mounting local (4.2bsd FFS) filesystems
 */
struct ultrix_ufs_args {
	u_long ufs_flags;		/* mount flags?*/
	u_long ufs_pgthresh;		/* minimum file size to page */
};

int
ultrix_sys_mount(p, v, retval)
	struct proc *p;
	void *v;
	int *retval;
{
	struct ultrix_sys_mount_args *uap = v;

	int error;
	int otype = SCARG(uap, type);
	char fsname[MFSNAMELEN];
	char * fstype;
	struct sys_mount_args nuap;
	caddr_t sg = stackgap_init(p->p_emul);
	caddr_t usp = stackgap_alloc(&sg, 1024 /* XXX */);

	bzero(&nuap, sizeof(nuap));
	SCARG(&nuap, flags) = 0;

	/*
	 * Translate Ultrix integer mount codes for UFS and NFS to
	 * NetBSD fstype strings.  Other Ultrix filesystem types
	 *  (msdos, DEC ods-2) are not supported.
	 */
	if (otype == ULTRIX_FSTYPE_ULTRIX)
		fstype = "ufs";
	else if (otype == ULTRIX_FSTYPE_NFS)
		fstype = "nfs";
	else
		return (EINVAL);

	/* Translate the Ultrix mount-readonly option parameter */
	if (SCARG(uap, rdonly))
		SCARG(&nuap, flags) |= MNT_RDONLY;

	/* Copy string-ified version of mount type back out to user space */
	SCARG(&nuap, type) = (char *)usp;
	if ((error = copyout(fstype, (void *)SCARG(&nuap, type),
			    strlen(fstype)+1)) != 0) {
		return (error);
	}
	usp += strlen(fstype)+1;

#ifdef later
	parse ultrix mount option string and set NetBSD flags
#endif
	SCARG(&nuap, path) = SCARG(uap, dir);

	if (otype == ULTRIX_FSTYPE_ULTRIX) {
		/* attempt to mount a native, rather than 4.2bsd, ffs */
		struct ufs_args ua;

		ua.fspec = SCARG(uap, special);
		bzero(&ua.export_info, sizeof(ua.export_info));
		SCARG(&nuap, data) = usp;
	
		if ((error = copyout(&ua, SCARG(&nuap, data),
				     sizeof ua)) !=0) {
			return(error);
		}
		/*
		 * Ultrix mount has no MNT_UPDATE flag.
		 * Attempt to see if this is the root we're mounting,
		 * and if so, set MNT_UPDATE so we can mount / read-write.
		 */
		fsname[0] = 0;
		if ((error = copyinstr((caddr_t)SCARG(&nuap, path), fsname,
				      sizeof fsname, (u_int *)0)) != 0)
			return(error);
		if (strcmp(fsname, "/") == 0) {
			SCARG(&nuap, flags) |= MNT_UPDATE;
			printf("COMPAT_ULTRIX: mount with MNT_UPDATE on %s\n",
			       fsname);
		}
	} else if (otype == ULTRIX_FSTYPE_NFS) {
		struct ultrix_nfs_args una;
		struct nfs_args na;
		struct osockaddr_in osa;
		struct sockaddr_in *sap = (struct sockaddr_in *)& osa;

		bzero(&osa, sizeof(osa));
		bzero(&una, sizeof(una));
		if ((error = copyin(SCARG(uap, data), &una, sizeof una)) !=0) {
			return (error);
		}
		/*
		 * This is the only syscall boundary the
		 * address of the server passes, so do backwards
		 * compatibility on 4.3style sockaddrs here.
		 */
		if ((error = copyin(una.addr, &osa, sizeof osa)) != 0) {
			printf("ultrix_mount: nfs copyin osa\n");
			return (error);
		}
		sap->sin_family = (u_char)osa.sin_family;
		sap->sin_len = sizeof(*sap);
		/* allocate space above caller's stack for nfs_args */
		SCARG(&nuap, data) = usp;
		usp +=  sizeof (na);
		/* allocate space above caller's stack for server sockaddr */
		na.version = NFS_ARGSVERSION;
		na.addr = (struct sockaddr *)usp;
		usp += sizeof(*sap);
		na.addrlen = sap->sin_len;
		na.sotype = SOCK_DGRAM;
		na.proto = IPPROTO_UDP;
		na.fh = una.fh;
		na.fhsize = NFSX_V2FH;
		na.flags = /*una.flags;*/ NFSMNT_NOCONN;
		na.wsize = una.wsize;
		na.rsize = una.rsize;
		na.timeo = una.timeo;
		na.retrans = una.retrans;
		na.hostname = una.hostname;
		if ((error = copyout(sap, na.addr, sizeof (*sap) )) != 0)
			return (error);
		if ((error = copyout(&na, SCARG(&nuap, data), sizeof na)) != 0)
			return (error);
	}
	return (sys_mount(p, &nuap, retval));
}
