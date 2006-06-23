/*	$OpenBSD: udf_vfsops.c,v 1.10 2006/06/23 11:21:29 pedro Exp $	*/

/*
 * Copyright (c) 2001, 2002 Scott Long <scottl@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/fs/udf/udf_vfsops.c,v 1.25 2005/01/25 15:52:03 phk Exp $
 */

/*
 * Ported to OpenBSD by Pedro Martelletto <pedro@openbsd.org> in February 2005.
 */

/*
 * Ok, here's how it goes.  The UDF specs are pretty clear on how each data
 * structure is made up, but not very clear on how they relate to each other.
 * Here is the skinny... This demostrates a filesystem with one file in the
 * root directory.  Subdirectories are treated just as normal files, but they
 * have File Id Descriptors of their children as their file data.  As for the
 * Anchor Volume Descriptor Pointer, it can exist in two of the following three
 * places: sector 256, sector n (the max sector of the disk), or sector
 * n - 256.  It's a pretty good bet that one will exist at sector 256 though.
 * One caveat is unclosed CD media.  For that, sector 256 cannot be written,
 * so the Anchor Volume Descriptor Pointer can exist at sector 512 until the
 * media is closed.
 *
 *  Sector:
 *     256:
 *       n: Anchor Volume Descriptor Pointer
 * n - 256:	|
 *		|
 *		|-->Main Volume Descriptor Sequence
 *			|	|
 *			|	|
 *			|	|-->Logical Volume Descriptor
 *			|			  |
 *			|-->Partition Descriptor  |
 *				|		  |
 *				|		  |
 *				|-->Fileset Descriptor
 *					|
 *					|
 *					|-->Root Dir File Entry
 *						|
 *						|
 *						|-->File data:
 *						    File Id Descriptor
 *							|
 *							|
 *							|-->File Entry
 *								|
 *								|
 *								|-->File data
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/pool.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/queue.h>
#include <sys/vnode.h>
#include <sys/endian.h>

#include <miscfs/specfs/specdev.h>

#include <isofs/udf/ecma167-udf.h>
#include <isofs/udf/udf.h>
#include <isofs/udf/udf_extern.h>

struct pool udf_trans_pool;
struct pool udf_node_pool;
struct pool udf_ds_pool;

int udf_find_partmaps(struct udf_mnt *, struct logvol_desc *);
int udf_get_vpartmap(struct udf_mnt *, struct part_map_virt *);
int udf_get_spartmap(struct udf_mnt *, struct part_map_spare *);

const struct vfsops udf_vfsops = {
	.vfs_fhtovp =		udf_fhtovp,
	.vfs_init =		udf_init,
	.vfs_mount =		udf_mount,
	.vfs_start =		udf_start,
	.vfs_root =		udf_root,
	.vfs_quotactl =		udf_quotactl,
	.vfs_statfs =		udf_statfs,
	.vfs_sync =		udf_sync,
	.vfs_unmount =		udf_unmount,
	.vfs_vget =		udf_vget,
	.vfs_vptofh =		udf_vptofh,
	.vfs_sysctl =		udf_sysctl,
	.vfs_checkexp =		udf_checkexp,
};

int udf_mountfs(struct vnode *, struct mount *, struct proc *);

int
udf_init(struct vfsconf *foo)
{
	pool_init(&udf_trans_pool, MAXNAMLEN * sizeof(unicode_t), 0, 0, 0,
	    "udftrpl", &pool_allocator_nointr);
	pool_init(&udf_node_pool, sizeof(struct udf_node), 0, 0, 0,
	    "udfndpl", &pool_allocator_nointr);
	pool_init(&udf_ds_pool, sizeof(struct udf_dirstream), 0, 0, 0,
	    "udfdspl", &pool_allocator_nointr);

	return (0);
}

int
udf_start(struct mount *mp, int flags, struct proc *p)
{
	return (0);
}

int
udf_mount(struct mount *mp, const char *path, void *data,
    struct nameidata *ndp,  struct proc *p)
{
	struct vnode *devvp;	/* vnode of the mount device */
	struct udf_args args;
	size_t len;
	int error;

	if ((mp->mnt_flag & MNT_RDONLY) == 0) {
		mp->mnt_flag |= MNT_RDONLY;
		printf("udf_mount: enforcing read-only mode\n");
	}

	/*
	 * No root filesystem support.  Probably not a big deal, since the
	 * bootloader doesn't understand UDF.
	 */
	if (mp->mnt_flag & MNT_ROOTFS)
		return (EOPNOTSUPP);

	error = copyin(data, &args, sizeof(struct udf_args));
	if (error)
		return (error);

	if (args.fspec == NULL)
		return (EINVAL);

	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, args.fspec, p);
	if ((error = namei(ndp)))
		return (error);

	devvp = ndp->ni_vp;
	if (devvp->v_type != VBLK) {
		vrele(devvp);
		return (ENOTBLK);
	}

	if (major(devvp->v_rdev) >= nblkdev) {
		vrele(devvp);
		return (ENXIO);
	}

	/* Check the access rights on the mount device */
	if (p->p_ucred->cr_uid) {
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
		error = VOP_ACCESS(devvp, VREAD, p->p_ucred, p);
		VOP_UNLOCK(devvp, 0, p);
		if (error) {
			vrele(devvp);
			return (error);
		}
	}

	if ((error = udf_mountfs(devvp, mp, p))) {
		vrele(devvp);
		return (error);
	}

	/*
	 * Keep a copy of the mount information.
	 */
	copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &len);
	bzero(mp->mnt_stat.f_mntonname + len, MNAMELEN - len);
	copyinstr(args.fspec, mp->mnt_stat.f_mntfromname, MNAMELEN - 1, &len);
	bzero(mp->mnt_stat.f_mntfromname + len, MNAMELEN - len);

	return (0);
};

/*
 * Check the descriptor tag for both the correct id and correct checksum.
 * Return zero if all is good, EINVAL if not.
 */
int
udf_checktag(struct desc_tag *tag, uint16_t id)
{
	uint8_t *itag;
	uint8_t i, cksum = 0;

	itag = (uint8_t *)tag;

	if (letoh16(tag->id) != id)
		return (EINVAL);

	for (i = 0; i < 15; i++)
		cksum = cksum + itag[i];
	cksum = cksum - itag[4];

	if (cksum == tag->cksum)
		return (0);

	return (EINVAL);
}

int
udf_mountfs(struct vnode *devvp, struct mount *mp, struct proc *p)
{
	struct buf *bp = NULL;
	struct anchor_vdp avdp;
	struct udf_mnt *udfmp = NULL;
	struct part_desc *pd;
	struct logvol_desc *lvd;
	struct fileset_desc *fsd;
	struct file_entry *root_fentry;
	uint32_t sector, size, mvds_start, mvds_end;
	uint32_t fsd_offset = 0;
	uint16_t part_num = 0, fsd_part = 0;
	int error = EINVAL;
	int logvol_found = 0, part_found = 0, fsd_found = 0;
	int bsize;

	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	if ((error = vfs_mountedon(devvp)))
		return (error);
	if (vcount(devvp) > 1 && devvp != rootvp)
		return (EBUSY);
	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
	error = vinvalbuf(devvp, V_SAVE, p->p_ucred, p, 0, 0);
	VOP_UNLOCK(devvp, 0, p);
	if (error)
		return (error);

	error = VOP_OPEN(devvp, FREAD, FSCRED, p);
	if (error)
		return (error);

	MALLOC(udfmp, struct udf_mnt *, sizeof(struct udf_mnt), M_UDFMOUNT,
	    M_WAITOK);
	bzero(udfmp, sizeof(struct udf_mnt));

	mp->mnt_data = (qaddr_t)udfmp;
	mp->mnt_stat.f_fsid.val[0] = devvp->v_rdev;
	mp->mnt_stat.f_fsid.val[1] = makefstype(MOUNT_UDF);
	mp->mnt_flag |= MNT_LOCAL;
	udfmp->im_mountp = mp;
	udfmp->im_dev = devvp->v_rdev;
	udfmp->im_devvp = devvp;

	bsize = 2048;	/* XXX Should probe the media for its size. */

	/* 
	 * Get the Anchor Volume Descriptor Pointer from sector 256.
	 * XXX Should also check sector n - 256, n, and 512.
	 */
	sector = 256;
	if ((error = bread(devvp, sector * btodb(bsize), bsize, NOCRED,
			   &bp)) != 0)
		goto bail;
	if ((error = udf_checktag((struct desc_tag *)bp->b_data, TAGID_ANCHOR)))
		goto bail;

	bcopy(bp->b_data, &avdp, sizeof(struct anchor_vdp));
	brelse(bp);
	bp = NULL;

	/*
	 * Extract the Partition Descriptor and Logical Volume Descriptor
	 * from the Volume Descriptor Sequence.
	 * XXX Should we care about the partition type right now?
	 * XXX What about multiple partitions?
	 */
	mvds_start = letoh32(avdp.main_vds_ex.loc);
	mvds_end = mvds_start + (letoh32(avdp.main_vds_ex.len) - 1) / bsize;
	for (sector = mvds_start; sector < mvds_end; sector++) {
		if ((error = bread(devvp, sector * btodb(bsize), bsize, 
				   NOCRED, &bp)) != 0) {
			printf("Can't read sector %d of VDS\n", sector);
			goto bail;
		}
		lvd = (struct logvol_desc *)bp->b_data;
		if (!udf_checktag(&lvd->tag, TAGID_LOGVOL)) {
			udfmp->bsize = letoh32(lvd->lb_size);
			udfmp->bmask = udfmp->bsize - 1;
			udfmp->bshift = ffs(udfmp->bsize) - 1;
			fsd_part = letoh16(lvd->_lvd_use.fsd_loc.loc.part_num);
			fsd_offset = letoh32(lvd->_lvd_use.fsd_loc.loc.lb_num);
			if (udf_find_partmaps(udfmp, lvd))
				break;
			logvol_found = 1;
		}
		pd = (struct part_desc *)bp->b_data;
		if (!udf_checktag(&pd->tag, TAGID_PARTITION)) {
			part_found = 1;
			part_num = letoh16(pd->part_num);
			udfmp->part_len = letoh32(pd->part_len);
			udfmp->part_start = letoh32(pd->start_loc);
		}

		brelse(bp); 
		bp = NULL;
		if ((part_found) && (logvol_found))
			break;
	}

	if (!part_found || !logvol_found) {
		error = EINVAL;
		goto bail;
	}

	if (fsd_part != part_num) {
		printf("FSD does not lie within the partition!\n");
		error = EINVAL;
		goto bail;
	}


	/*
	 * Grab the Fileset Descriptor
	 * Thanks to Chuck McCrobie <mccrobie@cablespeed.com> for pointing
	 * me in the right direction here.
	 */
	sector = udfmp->part_start + fsd_offset;
	if ((error = RDSECTOR(devvp, sector, udfmp->bsize, &bp)) != 0) {
		printf("Cannot read sector %d of FSD\n", sector);
		goto bail;
	}
	fsd = (struct fileset_desc *)bp->b_data;
	if (!udf_checktag(&fsd->tag, TAGID_FSD)) {
		fsd_found = 1;
		bcopy(&fsd->rootdir_icb, &udfmp->root_icb,
		    sizeof(struct long_ad));
	}

	brelse(bp);
	bp = NULL;

	if (!fsd_found) {
		printf("Couldn't find the fsd\n");
		error = EINVAL;
		goto bail;
	}

	/*
	 * Find the file entry for the root directory.
	 */
	sector = letoh32(udfmp->root_icb.loc.lb_num) + udfmp->part_start;
	size = letoh32(udfmp->root_icb.len);
	if ((error = udf_readlblks(udfmp, sector, size, &bp)) != 0) {
		printf("Cannot read sector %d\n", sector);
		goto bail;
	}

	root_fentry = (struct file_entry *)bp->b_data;
	if ((error = udf_checktag(&root_fentry->tag, TAGID_FENTRY))) {
		printf("Invalid root file entry!\n");
		goto bail;
	}

	brelse(bp);
	bp = NULL;

	mtx_init(&udfmp->hash_mtx, IPL_NONE);
	udfmp->hashtbl = hashinit(UDF_HASHTBLSIZE, M_UDFMOUNT, M_WAITOK,
	    &udfmp->hashsz);

	devvp->v_specmountpoint = mp;

	return (0);

bail:
	if (udfmp != NULL) {
		FREE(udfmp, M_UDFMOUNT);
		mp->mnt_data = NULL;
		mp->mnt_flag &= ~MNT_LOCAL;
	}
	if (bp != NULL)
		brelse(bp);
	VOP_CLOSE(devvp, FREAD, FSCRED, p);

	return (error);
}

int
udf_unmount(struct mount *mp, int mntflags, struct proc *p)
{
	struct udf_mnt *udfmp;
	struct vnode *devvp;
	int error, flags = 0;

	udfmp = VFSTOUDFFS(mp);
	devvp = udfmp->im_devvp;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	if ((error = vflush(mp, NULL, flags)))
		return (error);

	vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY, p);
	vinvalbuf(devvp, V_SAVE, NOCRED, p, 0, 0);
	error = VOP_CLOSE(devvp, FREAD, NOCRED, p);
	VOP_UNLOCK(devvp, 0, p);
	if (error)
		return (error);

	devvp->v_specmountpoint = NULL;
	vrele(devvp);

	if (udfmp->s_table != NULL)
		free(udfmp->s_table, M_UDFMOUNT);

	if (udfmp->hashtbl != NULL)
		FREE(udfmp->hashtbl, M_UDFMOUNT);

	FREE(udfmp, M_UDFMOUNT);

	mp->mnt_data = (qaddr_t)0;
	mp->mnt_flag &= ~MNT_LOCAL;

	return (0);
}

int
udf_root(struct mount *mp, struct vnode **vpp)
{
	struct udf_mnt *udfmp;
	struct vnode *vp;
	ino_t id;
	int error;

	udfmp = VFSTOUDFFS(mp);

	id = udf_getid(&udfmp->root_icb);

	error = udf_vget(mp, id, vpp);
	if (error)
		return (error);

	vp = *vpp;
	vp->v_flag |= VROOT;
	udfmp->root_vp = vp;

	return (0);
}

int
udf_quotactl(struct mount *mp, int cmds, uid_t uid, caddr_t arg,
    struct proc *p)
{
	return (EOPNOTSUPP);
}

int
udf_statfs(struct mount *mp, struct statfs *sbp, struct proc *p)
{
	struct udf_mnt *udfmp;

	udfmp = VFSTOUDFFS(mp);

	sbp->f_bsize = udfmp->bsize;
	sbp->f_iosize = udfmp->bsize;
	sbp->f_blocks = udfmp->part_len;
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 0;
	sbp->f_ffree = 0;

	return (0);
}

int
udf_sync(struct mount *mp, int waitfor, struct ucred *cred, struct proc *p)
{
	return (0);
}

int
udf_vget(struct mount *mp, ino_t ino, struct vnode **vpp)
{
	struct buf *bp;
	struct vnode *devvp;
	struct udf_mnt *udfmp;
	struct proc *p;
	struct vnode *vp;
	struct udf_node *unode;
	struct file_entry *fe;
	int error, sector, size;

	p = curproc;
	bp = NULL;
	*vpp = NULL;
	udfmp = VFSTOUDFFS(mp);

	/* See if we already have this in the cache */
	if ((error = udf_hashlookup(udfmp, ino, LK_EXCLUSIVE, vpp)) != 0)
		return (error);
	if (*vpp != NULL)
		return (0);

	/*
	 * Allocate memory and check the tag id's before grabbing a new
	 * vnode, since it's hard to roll back if there is a problem.
	 */
	unode = pool_get(&udf_node_pool, PR_WAITOK);
	bzero(unode, sizeof(struct udf_node));

	/*
	 * Copy in the file entry.  Per the spec, the size can only be 1 block.
	 */
	sector = ino + udfmp->part_start;
	devvp = udfmp->im_devvp;
	if ((error = RDSECTOR(devvp, sector, udfmp->bsize, &bp)) != 0) {
		printf("Cannot read sector %d\n", sector);
		pool_put(&udf_node_pool, unode);
		if (bp != NULL)
			brelse(bp);
		return (error);
	}

	fe = (struct file_entry *)bp->b_data;
	if (udf_checktag(&fe->tag, TAGID_FENTRY)) {
		printf("Invalid file entry!\n");
		pool_put(&udf_node_pool, unode);
		brelse(bp);
		return (ENOMEM);
	}
	size = UDF_FENTRY_SIZE + letoh32(fe->l_ea) + letoh32(fe->l_ad);
	MALLOC(unode->fentry, struct file_entry *, size, M_UDFFENTRY,
	    M_NOWAIT);
	if (unode->fentry == NULL) {
		printf("Cannot allocate file entry block\n");
		pool_put(&udf_node_pool, unode);
		brelse(bp);
		return (ENOMEM);
	}

	bcopy(bp->b_data, unode->fentry, size);
	
	brelse(bp);
	bp = NULL;

	if ((error = udf_allocv(mp, &vp, p))) {
		printf("Error from udf_allocv\n");
		FREE(unode->fentry, M_UDFFENTRY);
		pool_put(&udf_node_pool, unode);
		return (error);
	}

	unode->i_vnode = vp;
	unode->hash_id = ino;
	unode->i_devvp = udfmp->im_devvp;
	unode->i_dev = udfmp->im_dev;
	unode->udfmp = udfmp;
	vp->v_data = unode;
	VREF(udfmp->im_devvp);

	lockinit(&unode->i_lock, PINOD, "unode", 0, 0);

	/*
	 * udf_hashins() will lock the vnode for us.
	 */
	udf_hashins(unode);

	switch (unode->fentry->icbtag.file_type) {
	default:
		vp->v_type = VBAD;
		break;
	case UDF_ICB_TYPE_DIR:
		vp->v_type = VDIR;
		break;
	case UDF_ICB_TYPE_FILE:
		vp->v_type = VREG;
		break;
	case UDF_ICB_TYPE_BLKDEV:
		vp->v_type = VBLK;
		break;
	case UDF_ICB_TYPE_CHRDEV:
		vp->v_type = VCHR;
		break;
	case UDF_ICB_TYPE_FIFO:
		vp->v_type = VFIFO;
		break;
	case UDF_ICB_TYPE_SOCKET:
		vp->v_type = VSOCK;
		break;
	case UDF_ICB_TYPE_SYMLINK:
		vp->v_type = VLNK;
		break;
	}

	*vpp = vp;

	return (0);
}

struct ifid {
	u_short	ifid_len;
	u_short	ifid_pad;
	int	ifid_ino;
	long	ifid_start;
};

int
udf_fhtovp(struct mount *mp, struct fid *fhp, struct vnode **vpp)
{
	struct ifid *ifhp;
	struct vnode *nvp;
	int error;

	ifhp = (struct ifid *)fhp;

	if ((error = VFS_VGET(mp, ifhp->ifid_ino, &nvp)) != 0) {
		*vpp = NULLVP;
		return (error);
	}

	*vpp = nvp;

	return (0);
}

int
udf_vptofh(struct vnode *vp, struct fid *fhp)
{
	struct udf_node *node;
	struct ifid *ifhp;

	node = VTON(vp);
	ifhp = (struct ifid *)fhp;
	ifhp->ifid_len = sizeof(struct ifid);
	ifhp->ifid_ino = node->hash_id;

	return (0);
}

int
udf_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	return (EINVAL);
}

int
udf_checkexp(struct mount *mp, struct mbuf *nam, int *exflagsp,
    struct ucred **credanonp)
{
	return (EACCES); /* For the time being */
}

/* Handle a virtual partition map */
int
udf_get_vpartmap(struct udf_mnt *udfmp, struct part_map_virt *pmv)
{
	return (EOPNOTSUPP); /* Not supported yet */
}

/* Handle a sparable partition map */
int
udf_get_spartmap(struct udf_mnt *udfmp, struct part_map_spare *pms)
{
	struct buf *bp;
	int i, error;

	udfmp->s_table = malloc(letoh32(pms->st_size), M_UDFMOUNT, M_NOWAIT);
	if (udfmp->s_table == NULL)
		return (ENOMEM);

	bzero(udfmp->s_table, letoh32(pms->st_size));

	/* Calculate the number of sectors per packet */
	udfmp->p_sectors = letoh16(pms->packet_len) / udfmp->bsize;

	error = udf_readlblks(udfmp, letoh32(pms->st_loc[0]),
	    letoh32(pms->st_size), &bp);

	if (error) {
		if (bp != NULL)
			brelse(bp);
		free(udfmp->s_table, M_UDFMOUNT);
		return (error); /* Failed to read sparing table */
	}

	bcopy(bp->b_data, udfmp->s_table, letoh32(pms->st_size));
	brelse(bp);

	if (udf_checktag(&udfmp->s_table->tag, 0)) {
		free(udfmp->s_table, M_UDFMOUNT);
		return (EINVAL); /* Invalid sparing table found */
	}

	/*
	 * See how many valid entries there are here. The list is
	 * supposed to be sorted, 0xfffffff0 and higher are not valid.
	 */
	for (i = 0; i < letoh16(udfmp->s_table->rt_l); i++) {
		udfmp->s_table_entries = i;
		if (letoh32(udfmp->s_table->entries[i].org) >= 0xfffffff0)
			break;
	}

	return (0);
}

/* Scan the partition maps */
int
udf_find_partmaps(struct udf_mnt *udfmp, struct logvol_desc *lvd)
{
	struct regid *pmap_id;
	unsigned char regid_id[UDF_REGID_ID_SIZE + 1];
	int i, ptype, psize, error;
	uint8_t *pmap = (uint8_t *) &lvd->maps[0];

	for (i = 0; i < letoh32(lvd->n_pm); i++) {
		ptype = pmap[0];
		psize = pmap[1];

		if (ptype != 1 && ptype != 2)
			return (EINVAL); /* Invalid partition map type */

		if (psize != UDF_PMAP_TYPE1_SIZE &&
		    psize != UDF_PMAP_TYPE2_SIZE)
			return (EINVAL); /* Invalid partition map size */

		if (ptype == 1) {
			pmap += UDF_PMAP_TYPE1_SIZE;
			continue;
		}

		/* Type 2 map. Find out the details */
		pmap_id = (struct regid *) &pmap[4];
		bzero(&regid_id[0], UDF_REGID_ID_SIZE);
		bcopy(&pmap_id->id[0], &regid_id[0], UDF_REGID_ID_SIZE);

		if (!bcmp(&regid_id[0], "*UDF Virtual Partition",
		    UDF_REGID_ID_SIZE))
			error = udf_get_vpartmap(udfmp,
			    (struct part_map_virt *) pmap);
		else if (!bcmp(&regid_id[0], "*UDF Sparable Partition",
		    UDF_REGID_ID_SIZE))
			error = udf_get_spartmap(udfmp,
			    (struct part_map_spare *) pmap);
		else
			return (EINVAL); /* Unsupported partition map */

		if (error)
			return (error); /* Error getting partition */

		pmap += UDF_PMAP_TYPE2_SIZE;
	}

	return (0);
}
