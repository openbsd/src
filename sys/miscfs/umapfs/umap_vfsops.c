/*	$OpenBSD: umap_vfsops.c,v 1.27 2004/05/28 15:41:41 mpech Exp $	*/
/*	$NetBSD: umap_vfsops.c,v 1.35 2002/09/21 18:09:31 christos Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * the UCLA Ficus project.
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
 *	from: @(#)null_vfsops.c       1.5 (Berkeley) 7/10/92
 *	@(#)umap_vfsops.c	8.8 (Berkeley) 5/14/95
 */

/*
 * Umap Layer
 * (See mount_umap(8) for a description of this layer.)
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <miscfs/umapfs/umap.h>
#include <miscfs/genfs/layer_extern.h>

int	umapfs_mount(struct mount *, const char *, void *,
	  struct nameidata *, struct proc *);
int	umapfs_unmount(struct mount *, int, struct proc *);

/*
 * Mount umap layer
 */
int
umapfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	const char *path;
	void *data;
	struct nameidata *ndp;
	struct proc *p;
{
	struct umap_args args;
	struct vnode *lowerrootvp, *vp;
	struct umap_mount *amp;
	size_t size;
	int error;
#ifdef UMAPFS_DIAGNOSTIC
	int i;
#endif
#if 0
	if (mp->mnt_flag & MNT_GETARGS) {
		amp = MOUNTTOUMAPMOUNT(mp);
		if (amp == NULL)
			return EIO;
		args.la.target = NULL;
		vfs_showexport(mp, &args.la.export, &amp->umapm_export);
		args.nentries = amp->info_nentries;
		args.gnentries = amp->info_gnentries;
		return copyout(&args, data, sizeof(args));
	}
#endif

	/* only for root */
	if ((error = suser(p, 0)) != 0)
		return error;

#ifdef UMAPFS_DIAGNOSTIC
	printf("umapfs_mount(mp = %p)\n", mp);
#endif

	/*
	 * Get argument
	 */
	error = copyin(data, &args, sizeof(struct umap_args));
	if (error)
		return (error);

	/*
	 * Update only does export updating.
	 */
	if (mp->mnt_flag & MNT_UPDATE) {
		amp = MOUNTTOUMAPMOUNT(mp);
		if (args.umap_target == 0)
			return (vfs_export(mp, &amp->umapm_export,
					&args.umap_export));
		else
			return (EOPNOTSUPP);
	}

	/*
	 * Find lower node
	 */
	NDINIT(ndp, LOOKUP, FOLLOW|LOCKLEAF,
		UIO_USERSPACE, args.umap_target, p);
	if ((error = namei(ndp)) != 0)
		return (error);

	/*
	 * Sanity check on lower vnode
	 */
	lowerrootvp = ndp->ni_vp;
#ifdef UMAPFS_DIAGNOSTIC
	printf("vp = %p, check for VDIR...\n", lowerrootvp);
#endif

	if (lowerrootvp->v_type != VDIR) {
		vput(lowerrootvp);
		return (EINVAL);
	}

#ifdef UMAPFS_DIAGNOSTIC
	printf("mp = %p\n", mp);
#endif

	amp = malloc(sizeof(struct umap_mount), M_MISCFSMNT, M_WAITOK);
	memset(amp, 0, sizeof(struct umap_mount));

	mp->mnt_data = amp;
	amp->umapm_vfs = lowerrootvp->v_mount;
	if (amp->umapm_vfs->mnt_flag & MNT_LOCAL)
		mp->mnt_flag |= MNT_LOCAL;

	/* 
	 * Now copy in the number of entries and maps for umap mapping.
	 */
	if (args.unentries > UMAPFILEENTRIES || args.gnentries > GMAPFILEENTRIES) {
		vput(lowerrootvp);
		return (error);
	}

	amp->info_unentries = args.unentries;
	amp->info_gnentries = args.gnentries;
	error = copyin(args.umapdata, amp->info_umapdata, 
	    2*sizeof(u_long)*args.unentries);
	if (error) {
		vput(lowerrootvp);
		return (error);
	}

#ifdef UMAPFS_DIAGNOSTIC
	printf("umap_mount:unentries %d\n",args.unentries);
	for (i = 0; i < args.unentries; i++)
		printf("   %ld maps to %ld\n", amp->info_umapdata[i][0],
	 	    amp->info_umapdata[i][1]);
#endif

	error = copyin(args.gmapdata, amp->info_gmapdata, 
	    2*sizeof(u_long)*args.gnentries);
	if (error) {
		vput(lowerrootvp);
		return (error);
	}

#ifdef UMAPFS_DIAGNOSTIC
	printf("umap_mount:gnentries %d\n",args.gnentries);
	for (i = 0; i < args.gnentries; i++)
		printf("\tgroup %ld maps to %ld\n", 
		    amp->info_gmapdata[i][0],
	 	    amp->info_gmapdata[i][1]);
#endif

	/*
	 * Make sure the mount point's sufficiently initialized
	 * that the node create call will work.
	 */
	vfs_getnewfsid(mp);
	amp->umapm_size = sizeof(struct umap_node);
	amp->umapm_tag = VT_UMAP;
	amp->umapm_bypass = umap_bypass;
	amp->umapm_alloc = layer_node_alloc;	/* the default alloc is fine */
	amp->umapm_vnodeop_p = umapfs_vnodeop_p;
	simple_lock_init(&amp->umapm_hashlock);
	amp->umapm_node_hashtbl = hashinit(NUMAPNODECACHE, M_CACHE,
	    M_WAITOK, &amp->umapm_node_hash);


	/*
	 * fix up umap node for root vnode.
	 */
	error = layer_node_create(mp, lowerrootvp, &vp);
	/*
	 * Make sure the node alias worked
	 */
	if (error) {
		vput(lowerrootvp);
		free(amp, M_MISCFSMNT);
		return (error);
	}
	/*
	 * Unlock the node (either the lower or the alias)
	 */
	VOP_UNLOCK(vp, 0, p);

	/*
	 * Keep a held reference to the root vnode.
	 * It is vrele'd in umapfs_unmount.
	 */
	vp->v_flag |= VROOT;
	amp->umapm_rootvp = vp;

	(void) copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	memset(mp->mnt_stat.f_mntonname + size, 0, MNAMELEN - size);
	(void) copyinstr(args.umap_target, mp->mnt_stat.f_mntfromname,
		MNAMELEN - 1, &size);
	memset(mp->mnt_stat.f_mntfromname + size, 0, MNAMELEN - size);
#ifdef UMAPFS_DIAGNOSTIC
	printf("umapfs_mount: lower %s, alias at %s\n",
		mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname);
#endif
	return (0);
}

/*
 * Free reference to umap layer
 */
int
umapfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	struct vnode *rootvp = MOUNTTOUMAPMOUNT(mp)->umapm_rootvp;
	int error;
	int flags = 0;

#ifdef UMAPFS_DIAGNOSTIC
	printf("umapfs_unmount(mp = %p)\n", mp);
#endif

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/*
	 * Clear out buffer cache.  I don't think we
	 * ever get anything cached at this level at the
	 * moment, but who knows...
	 */
#ifdef notyet
	mntflushbuf(mp, 0); 
	if (mntinvalbuf(mp, 1))
		return (EBUSY);
#endif
	if (rootvp->v_usecount > 1 && !(flags & FORCECLOSE))
		return (EBUSY);
	if ((error = vflush(mp, rootvp, flags)) != 0)
		return (error);

#ifdef UMAPFS_DIAGNOSTIC
	vprint("alias root of lower", rootvp);
#endif	 
	/*
	 * Release reference on underlying root vnode
	 */
	vrele(rootvp);
	/*
	 * And blow it away for future re-use
	 */
	vgone(rootvp);
	/*
	 * Finally, throw away the umap_mount structure
	 */
	free(mp->mnt_data, M_MISCFSMNT);
	mp->mnt_data = 0;
	return (0);
}

extern const struct vnodeopv_desc umapfs_vnodeop_opv_desc;

const struct vnodeopv_desc * const umapfs_vnodeopv_descs[] = {
	&umapfs_vnodeop_opv_desc,
	NULL,
};

const struct vfsops umapfs_vfsops = {
	umapfs_mount,
	layerfs_start,
	umapfs_unmount,
	layerfs_root,
	layerfs_quotactl,
	layerfs_statfs,
	layerfs_sync,
	layerfs_vget,
	layerfs_fhtovp,
	layerfs_vptofh,
	layerfs_init,
	layerfs_sysctl,
	layerfs_checkexp,
};
