/*	$OpenBSD: udf_vnops.c,v 1.27 2007/06/06 17:15:13 deraadt Exp $	*/

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
 * $FreeBSD: src/sys/fs/udf/udf_vnops.c,v 1.50 2005/01/28 14:42:16 phk Exp $
 */

/*
 * Ported to OpenBSD by Pedro Martelletto <pedro@openbsd.org> in February 2005.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/pool.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/queue.h>
#include <sys/unistd.h>
#include <sys/endian.h>

#include <miscfs/specfs/specdev.h>

#include <isofs/udf/ecma167-udf.h>
#include <isofs/udf/udf.h>
#include <isofs/udf/udf_extern.h>

int udf_bmap_internal(struct unode *, off_t, daddr64_t *, uint32_t *);

int (**udf_vnodeop_p)(void *);
struct vnodeopv_entry_desc udf_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_access_desc, udf_access },		/* access */
	{ &vop_bmap_desc, udf_bmap },			/* bmap */
	{ &vop_lookup_desc, udf_lookup },		/* lookup */
	{ &vop_getattr_desc, udf_getattr },		/* getattr */
	{ &vop_open_desc, udf_open },			/* open */
	{ &vop_close_desc, udf_close },			/* close */
	{ &vop_ioctl_desc, udf_ioctl },			/* ioctl */
	{ &vop_read_desc, udf_read },			/* read */
	{ &vop_readdir_desc, udf_readdir },		/* readdir */
	{ &vop_readlink_desc, udf_readlink },		/* readlink */
	{ &vop_inactive_desc, udf_inactive },		/* inactive */
	{ &vop_reclaim_desc, udf_reclaim },		/* reclaim */
	{ &vop_strategy_desc, udf_strategy },		/* strategy */
	{ &vop_lock_desc, udf_lock },			/* lock */
	{ &vop_unlock_desc, udf_unlock },		/* unlock */
	{ &vop_islocked_desc, udf_islocked },		/* islocked */
	{ &vop_print_desc, udf_print },			/* print */
	{ NULL, NULL }
};
struct vnodeopv_desc udf_vnodeop_opv_desc =
	{ &udf_vnodeop_p, udf_vnodeop_entries };

#define UDF_INVALID_BMAP	-1

/* Look up a unode based on the ino_t passed in and return its vnode */
int
udf_hashlookup(struct umount *ump, ino_t id, int flags, struct vnode **vpp)
{
	struct unode *up;
	struct udf_hash_lh *lh;
	struct proc *p = curproc;
	int error;

	*vpp = NULL;

loop:
	mtx_enter(&ump->um_hashmtx);
	lh = &ump->um_hashtbl[id & ump->um_hashsz];
	if (lh == NULL) {
		mtx_leave(&ump->um_hashmtx);
		return (ENOENT);
	}

	LIST_FOREACH(up, lh, u_le) {
		if (up->u_ino == id) {
			mtx_leave(&ump->um_hashmtx);
			error = vget(up->u_vnode, flags, p);
			if (error == ENOENT)
				goto loop;
			if (error)
				return (error);
			*vpp = up->u_vnode;
			return (0);
		}
	}

	mtx_leave(&ump->um_hashmtx);

	return (0);
}

int
udf_hashins(struct unode *up)
{
	struct umount *ump;
	struct udf_hash_lh *lh;
	struct proc *p = curproc;

	ump = up->u_ump;

	vn_lock(up->u_vnode, LK_EXCLUSIVE | LK_RETRY, p);
	mtx_enter(&ump->um_hashmtx);
	lh = &ump->um_hashtbl[up->u_ino & ump->um_hashsz];
	if (lh == NULL)
		LIST_INIT(lh);
	LIST_INSERT_HEAD(lh, up, u_le);
	mtx_leave(&ump->um_hashmtx);

	return (0);
}

int
udf_hashrem(struct unode *up)
{
	struct umount *ump;
	struct udf_hash_lh *lh;

	ump = up->u_ump;

	mtx_enter(&ump->um_hashmtx);
	lh = &ump->um_hashtbl[up->u_ino & ump->um_hashsz];
	if (lh == NULL)
		panic("hash entry is NULL, up->u_ino = %d", up->u_ino);
	LIST_REMOVE(up, u_le);
	mtx_leave(&ump->um_hashmtx);

	return (0);
}

int
udf_allocv(struct mount *mp, struct vnode **vpp, struct proc *p)
{
	int error;
	struct vnode *vp;

	error = getnewvnode(VT_UDF, mp, udf_vnodeop_p, &vp);
	if (error) {
		printf("udf_allocv: failed to allocate new vnode\n");
		return (error);
	}

	*vpp = vp;
	return (0);
}

/* Convert file entry permission (5 bits per owner/group/user) to a mode_t */
static mode_t
udf_permtomode(struct unode *up)
{
	uint32_t perm;
	uint16_t flags;
	mode_t mode;

	perm = letoh32(up->u_fentry->perm);
	flags = letoh16(up->u_fentry->icbtag.flags);

	mode = perm & UDF_FENTRY_PERM_USER_MASK;
	mode |= ((perm & UDF_FENTRY_PERM_GRP_MASK) >> 2);
	mode |= ((perm & UDF_FENTRY_PERM_OWNER_MASK) >> 4);
	mode |= ((flags & UDF_ICB_TAG_FLAGS_STICKY) << 4);
	mode |= ((flags & UDF_ICB_TAG_FLAGS_SETGID) << 6);
	mode |= ((flags & UDF_ICB_TAG_FLAGS_SETUID) << 8);

	return (mode);
}

int
udf_access(void *v)
{
	struct vop_access_args *ap = v;
	struct vnode *vp;
	struct unode *up;
	mode_t a_mode, mode;

	vp = ap->a_vp;
	up = VTOU(vp);
	a_mode = ap->a_mode;

	if (a_mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			return (EROFS);
			/* NOTREACHED */
		default:
			break;
		}
	}

	mode = udf_permtomode(up);

	return (vaccess(mode, up->u_fentry->uid, up->u_fentry->gid, a_mode,
	    ap->a_cred));
}

static int mon_lens[2][12] = {
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

static int
udf_isaleapyear(int year)
{
	int i;

	i = (year % 4) ? 0 : 1;
	i &= (year % 100) ? 1 : 0;
	i |= (year % 400) ? 0 : 1;

	return (i);
}

/*
 * This is just a rough hack.  Daylight savings isn't calculated and tv_nsec
 * is ignored.
 * Timezone calculation compliments of Julian Elischer <julian@elischer.org>.
 */
static void
udf_timetotimespec(struct timestamp *time, struct timespec *t)
{
	int i, lpyear, daysinyear, year;
	union {
		uint16_t	u_tz_offset;
		int16_t		s_tz_offset;
	} tz;

	t->tv_nsec = 0;

	/* DirectCD seems to like using bogus year values */
	year = letoh16(time->year);
	if (year < 1970) {
		t->tv_sec = 0;
		return;
	}

	/* Calculate the time and day */
	t->tv_sec = time->second;
	t->tv_sec += time->minute * 60;
	t->tv_sec += time->hour * 3600;
	t->tv_sec += time->day * 3600 * 24;

	/* Calculate the month */
	lpyear = udf_isaleapyear(year);
	for (i = 1; i < time->month; i++)
		t->tv_sec += mon_lens[lpyear][i] * 3600 * 24;

	/* Speed up the calculation */
	if (year > 1979)
		t->tv_sec += 315532800;
	if (year > 1989)
		t->tv_sec += 315619200;
	if (year > 1999)
		t->tv_sec += 315532800;
	for (i = 2000; i < year; i++) {
		daysinyear = udf_isaleapyear(i) + 365 ;
		t->tv_sec += daysinyear * 3600 * 24;
	}

	/*
	 * Calculate the time zone.  The timezone is 12 bit signed 2's
	 * compliment, so we gotta do some extra magic to handle it right.
	 */
	tz.u_tz_offset = letoh16(time->type_tz);
	tz.u_tz_offset &= 0x0fff;
	if (tz.u_tz_offset & 0x0800)
		tz.u_tz_offset |= 0xf000;	/* extend the sign to 16 bits */
	if ((time->type_tz & 0x1000) && (tz.s_tz_offset != -2047))
		t->tv_sec -= tz.s_tz_offset * 60;

	return;
}

int
udf_getattr(void *v)
{
	struct vop_getattr_args *ap = v;
	struct vnode *vp;
	struct unode *up;
	struct vattr *vap;
	struct file_entry *fentry;
	struct timespec ts;

	ts.tv_sec = 0;

	vp = ap->a_vp;
	vap = ap->a_vap;
	up = VTOU(vp);
	fentry = up->u_fentry;

	vap->va_fsid = up->u_dev;
	vap->va_fileid = up->u_ino;
	vap->va_mode = udf_permtomode(up);
	vap->va_nlink = letoh16(fentry->link_cnt);
	/*
	 * The spec says that -1 is valid for uid/gid and indicates an
	 * invalid uid/gid.  How should this be represented?
	 */
	vap->va_uid = (letoh32(fentry->uid) == -1) ? 0 : letoh32(fentry->uid);
	vap->va_gid = (letoh32(fentry->gid) == -1) ? 0 : letoh32(fentry->gid);
	udf_timetotimespec(&fentry->atime, &vap->va_atime);
	udf_timetotimespec(&fentry->mtime, &vap->va_mtime);
	vap->va_ctime = vap->va_mtime; /* Stored as an Extended Attribute */
	vap->va_rdev = 0;
	if (vp->v_type & VDIR) {
		vap->va_nlink++; /* Count a reference to ourselves */
		/*
		 * Directories that are recorded within their ICB will show
		 * as having 0 blocks recorded.  Since tradition dictates
		 * that directories consume at least one logical block,
		 * make it appear so.
		 */
		if (fentry->logblks_rec != 0) {
			vap->va_size =
			    letoh64(fentry->logblks_rec) * up->u_ump->um_bsize;
		} else {
			vap->va_size = up->u_ump->um_bsize;
		}
	} else {
		vap->va_size = letoh64(fentry->inf_len);
	}
	vap->va_flags = 0;
	vap->va_gen = 1;
	vap->va_blocksize = up->u_ump->um_bsize;
	vap->va_bytes = letoh64(fentry->inf_len);
	vap->va_type = vp->v_type;
	vap->va_filerev = 0;

	return (0);
}

int
udf_open(void *v)
{
	return (0); /* Nothing to be done at this point */
}

int
udf_close(void *v)
{
	return (0); /* Nothing to be done at this point */
}

/*
 * File specific ioctls.
 */
int
udf_ioctl(void *v)
{
	return (ENOTTY);
}

/*
 * I'm not sure that this has much value in a read-only filesystem, but
 * cd9660 has it too.
 */
#if 0
static int
udf_pathconf(struct vop_pathconf_args *a)
{

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = 65535;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = NAME_MAX;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		return (0);
	default:
		return (EINVAL);
	}
}
#endif

int
udf_read(void *v)
{
	struct vop_read_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	struct unode *up = VTOU(vp);
	struct buf *bp;
	uint8_t *data;
	off_t fsize, offset;
	int error = 0;
	int size;

	if (uio->uio_offset < 0)
		return (EINVAL);

	fsize = letoh64(up->u_fentry->inf_len);

	while (uio->uio_offset < fsize && uio->uio_resid > 0) {
		offset = uio->uio_offset;
		if (uio->uio_resid + offset <= fsize)
			size = uio->uio_resid;
		else
			size = fsize - offset;
		error = udf_readatoffset(up, &size, offset, &bp, &data);
		if (error == 0)
			error = uiomove(data, size, uio);
		if (bp != NULL)
			brelse(bp);
		if (error)
			break;
	};

	return (error);
}

/*
 * Translate the name from a CS0 dstring to a 16-bit Unicode String.
 * Hooks need to be placed in here to translate from Unicode to the encoding
 * that the kernel/user expects.  Return the length of the translated string.
 */
int
udf_transname(char *cs0string, char *destname, int len, struct umount *ump)
{
	unicode_t *transname;
	int i, unilen = 0, destlen;

	if (len > MAXNAMLEN) {
#ifdef DIAGNOSTIC
		printf("udf_transname(): name too long\n");
#endif
		return (0);
	}

	/* allocate a buffer big enough to hold an 8->16 bit expansion */
	transname = pool_get(&udf_trans_pool, PR_WAITOK);

	if ((unilen = udf_rawnametounicode(len, cs0string, transname)) == -1) {
#ifdef DIAGNOSTIC
		printf("udf_transname(): Unicode translation failed\n");
#endif
		pool_put(&udf_trans_pool, transname);
		return (0);
	}

	/* Pack it back to 8-bit Unicode. */
	for (i = 0; i < unilen ; i++)
		if (transname[i] & 0xff00)
			destname[i] = '?';	/* Fudge the 16bit chars */
		else
			destname[i] = transname[i] & 0xff;

	pool_put(&udf_trans_pool, transname);

	/* Don't forget to terminate the string. */
	destname[unilen] = 0;
	destlen = unilen;

	return (destlen);
}

/*
 * Compare a CS0 dstring with a name passed in from the VFS layer.  Return
 * 0 on a successful match, nonzero otherwise.  Unicode work may need to be
 * done here also.
 */
static int
udf_cmpname(char *cs0string, char *cmpname, int cs0len, int cmplen, struct umount *ump)
{
	char *transname;
	int error = 0;

	/* This is overkill, but not worth creating a new pool */
	transname = pool_get(&udf_trans_pool, PR_WAITOK);

	cs0len = udf_transname(cs0string, transname, cs0len, ump);

	/* Easy check.  If they aren't the same length, they aren't equal */
	if ((cs0len == 0) || (cs0len != cmplen))
		error = -1;
	else
		error = bcmp(transname, cmpname, cmplen);

	pool_put(&udf_trans_pool, transname);

	return (error);
}

struct udf_uiodir {
	struct dirent *dirent;
	u_long *cookies;
	int ncookies;
	int acookies;
	int eofflag;
};

static int
udf_uiodir(struct udf_uiodir *uiodir, int de_size, struct uio *uio, long cookie)
{
	if (uiodir->cookies != NULL) {
		if (++uiodir->acookies > uiodir->ncookies) {
			uiodir->eofflag = 0;
			return (-1);
		}
		*uiodir->cookies++ = cookie;
	}

	if (uio->uio_resid < de_size) {
		uiodir->eofflag = 0;
		return (-1);
	}

	return (uiomove(uiodir->dirent, de_size, uio));
}

static struct udf_dirstream *
udf_opendir(struct unode *up, int offset, int fsize, struct umount *ump)
{
	struct udf_dirstream *ds;

	ds = pool_get(&udf_ds_pool, PR_WAITOK);
	bzero(ds, sizeof(struct udf_dirstream));

	ds->node = up;
	ds->offset = offset;
	ds->ump = ump;
	ds->fsize = fsize;

	return (ds);
}

static struct fileid_desc *
udf_getfid(struct udf_dirstream *ds)
{
	struct fileid_desc *fid;
	int error, frag_size = 0, total_fid_size;

	/* End of directory? */
	if (ds->offset + ds->off >= ds->fsize) {
		ds->error = 0;
		return (NULL);
	}

	/* Grab the first extent of the directory */
	if (ds->off == 0) {
		ds->size = 0;
		error = udf_readatoffset(ds->node, &ds->size, ds->offset,
		    &ds->bp, &ds->data);
		if (error) {
			ds->error = error;
			if (ds->bp != NULL)
				brelse(ds->bp);
			return (NULL);
		}
	}

	/*
	 * Clean up from a previous fragmented FID.
	 * Is this the right place for this?
	 */
	if (ds->fid_fragment && ds->buf != NULL) {
		ds->fid_fragment = 0;
		free(ds->buf, M_UDFFID);
	}

	fid = (struct fileid_desc*)&ds->data[ds->off];

	/*
	 * Check to see if the fid is fragmented. The first test
	 * ensures that we don't wander off the end of the buffer
	 * looking for the l_iu and l_fi fields.
	 */
	if (ds->off + UDF_FID_SIZE > ds->size ||
	    ds->off + letoh16(fid->l_iu) + fid->l_fi + UDF_FID_SIZE > ds->size){

		/* Copy what we have of the fid into a buffer */
		frag_size = ds->size - ds->off;
		if (frag_size >= ds->ump->um_bsize) {
			printf("udf: invalid FID fragment\n");
			ds->error = EINVAL;
			return (NULL);
		}

		/*
		 * File ID descriptors can only be at most one
		 * logical sector in size.
		 */
		ds->buf = malloc(ds->ump->um_bsize, M_UDFFID, M_WAITOK);
		bzero(ds->buf, ds->ump->um_bsize);
		bcopy(fid, ds->buf, frag_size);

		/* Reduce all of the casting magic */
		fid = (struct fileid_desc*)ds->buf;

		if (ds->bp != NULL)
			brelse(ds->bp);

		/* Fetch the next allocation */
		ds->offset += ds->size;
		ds->size = 0;
		error = udf_readatoffset(ds->node, &ds->size, ds->offset,
		    &ds->bp, &ds->data);
		if (error) {
			ds->error = error;
			return (NULL);
		}

		/*
		 * If the fragment was so small that we didn't get
		 * the l_iu and l_fi fields, copy those in.
		 */
		if (frag_size < UDF_FID_SIZE)
			bcopy(ds->data, &ds->buf[frag_size],
			    UDF_FID_SIZE - frag_size);

		/*
		 * Now that we have enough of the fid to work with,
		 * copy in the rest of the fid from the new
		 * allocation.
		 */
		total_fid_size = UDF_FID_SIZE + letoh16(fid->l_iu) + fid->l_fi;
		if (total_fid_size > ds->ump->um_bsize) {
			printf("udf: invalid FID\n");
			ds->error = EIO;
			return (NULL);
		}
		bcopy(ds->data, &ds->buf[frag_size],
		    total_fid_size - frag_size);

		ds->fid_fragment = 1;
	} else {
		total_fid_size = letoh16(fid->l_iu) + fid->l_fi + UDF_FID_SIZE;
	}

	/*
	 * Update the offset. Align on a 4 byte boundary because the
	 * UDF spec says so.
	 */
	ds->this_off = ds->off;
	if (!ds->fid_fragment) {
		ds->off += (total_fid_size + 3) & ~0x03;
	} else {
		ds->off = (total_fid_size - frag_size + 3) & ~0x03;
	}

	return (fid);
}

static void
udf_closedir(struct udf_dirstream *ds)
{

	if (ds->bp != NULL)
		brelse(ds->bp);

	if (ds->fid_fragment && ds->buf != NULL)
		free(ds->buf, M_UDFFID);

	pool_put(&udf_ds_pool, ds);
}

int
udf_readdir(void *v)
{
	struct vop_readdir_args *ap = v;
	struct vnode *vp;
	struct uio *uio;
	struct dirent dir;
	struct unode *up;
	struct umount *ump;
	struct fileid_desc *fid;
	struct udf_uiodir uiodir;
	struct udf_dirstream *ds;
	u_long *cookies = NULL;
	int ncookies;
	int error = 0;

#define GENERIC_DIRSIZ(dp) \
    ((sizeof (struct dirent) - (MAXNAMLEN+1)) + (((dp)->d_namlen+1 + 3) &~ 3))

	vp = ap->a_vp;
	uio = ap->a_uio;
	up = VTOU(vp);
	ump = up->u_ump;
	uiodir.eofflag = 1;

	if (ap->a_ncookies != NULL) {
		/*
		 * Guess how many entries are needed.  If we run out, this
		 * function will be called again and thing will pick up were
		 * it left off.
		 */
		ncookies = uio->uio_resid / 8;
		MALLOC(cookies, u_long *, sizeof(u_long) * ncookies,
		    M_TEMP, M_WAITOK);
		uiodir.ncookies = ncookies;
		uiodir.cookies = cookies;
		uiodir.acookies = 0;
	} else {
		uiodir.cookies = NULL;
	}

	/*
	 * Iterate through the file id descriptors.  Give the parent dir
	 * entry special attention.
	 */
	ds = udf_opendir(up, uio->uio_offset,
	    letoh64(up->u_fentry->inf_len), up->u_ump);

	while ((fid = udf_getfid(ds)) != NULL) {

		/* Should we return an error on a bad fid? */
		if (udf_checktag(&fid->tag, TAGID_FID)) {
			printf("Invalid FID tag\n");
			error = EIO;
			break;
		}

		/* Is this a deleted file? */
		if (fid->file_char & UDF_FILE_CHAR_DEL)
			continue;

		if ((fid->l_fi == 0) && (fid->file_char & UDF_FILE_CHAR_PAR)) {
			/* Do up the '.' and '..' entries.  Dummy values are
			 * used for the cookies since the offset here is
			 * usually zero, and NFS doesn't like that value
			 */
			dir.d_fileno = up->u_ino;
			dir.d_type = DT_DIR;
			dir.d_name[0] = '.';
			dir.d_name[1] = '\0';
			dir.d_namlen = 1;
			dir.d_reclen = GENERIC_DIRSIZ(&dir);
			uiodir.dirent = &dir;
			error = udf_uiodir(&uiodir, dir.d_reclen, uio, 1);
			if (error)
				break;

			dir.d_fileno = udf_getid(&fid->icb);
			dir.d_type = DT_DIR;
			dir.d_name[0] = '.';
			dir.d_name[1] = '.';
			dir.d_name[2] = '\0';
			dir.d_namlen = 2;
			dir.d_reclen = GENERIC_DIRSIZ(&dir);
			uiodir.dirent = &dir;
			error = udf_uiodir(&uiodir, dir.d_reclen, uio, 2);
		} else {
			dir.d_namlen = udf_transname(&fid->data[fid->l_iu],
			    &dir.d_name[0], fid->l_fi, ump);
			dir.d_fileno = udf_getid(&fid->icb);
			dir.d_type = (fid->file_char & UDF_FILE_CHAR_DIR) ?
			    DT_DIR : DT_UNKNOWN;
			dir.d_reclen = GENERIC_DIRSIZ(&dir);
			uiodir.dirent = &dir;
			error = udf_uiodir(&uiodir, dir.d_reclen, uio,
			    ds->this_off);
		}
		if (error) {
			printf("uiomove returned %d\n", error);
			break;
		}

	}

#undef GENERIC_DIRSIZ

	/* tell the calling layer whether we need to be called again */
	*ap->a_eofflag = uiodir.eofflag;
	uio->uio_offset = ds->offset + ds->off;

	if (!error)
		error = ds->error;

	udf_closedir(ds);

	if (ap->a_ncookies != NULL) {
		if (error)
			FREE(cookies, M_TEMP);
		else {
			*ap->a_ncookies = uiodir.acookies;
			*ap->a_cookies = cookies;
		}
	}

	return (error);
}

/* Are there any implementations out there that do soft-links? */
int
udf_readlink(void *v)
{
	return (EOPNOTSUPP);
}

int
udf_strategy(void *v)
{
	struct vop_strategy_args *ap = v;
	struct buf *bp;
	struct vnode *vp;
	struct unode *up;
	int maxsize, s, error;

	bp = ap->a_bp;
	vp = bp->b_vp;
	up = VTOU(vp);

	/* cd9660 has this test reversed, but it seems more logical this way */
	if (bp->b_blkno != bp->b_lblkno) {
		/*
		 * Files that are embedded in the fentry don't translate well
		 * to a block number.  Reject.
		 */
		if (udf_bmap_internal(up, bp->b_lblkno * up->u_ump->um_bsize,
		    &bp->b_lblkno, &maxsize)) {
			clrbuf(bp);
			bp->b_blkno = -1;
		}
	} else {
		error = VOP_BMAP(vp, bp->b_lblkno, NULL, &bp->b_blkno, NULL);
		if (error) {
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
			s = splbio();
			biodone(bp);
			splx(s);
			return (error);
		}

		if ((long)bp->b_blkno == -1)
			clrbuf(bp);
	}

	if ((long)bp->b_blkno == -1) {
		s = splbio();
		biodone(bp);
		splx(s);
	} else {
		bp->b_dev = vp->v_rdev;
		VOCALL(up->u_devvp->v_op, VOFFSET(vop_strategy), ap);
	}

	return (0);
}

int
udf_lock(void *v)
{
	struct vop_lock_args *ap = v;

	struct vnode *vp = ap->a_vp;

	return (lockmgr(&VTOU(vp)->u_lock, ap->a_flags, NULL));
}

int
udf_unlock(void *v)
{
	struct vop_unlock_args *ap = v;

	struct vnode *vp = ap->a_vp;

	return (lockmgr(&VTOU(vp)->u_lock, ap->a_flags | LK_RELEASE, NULL));
}

int
udf_islocked(void *v)
{
	struct vop_islocked_args *ap = v;

	return (lockstatus(&VTOU(ap->a_vp)->u_lock));
}

int
udf_print(void *v)
{
	struct vop_print_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct unode *up = VTOU(vp);

	/*
	 * Complete the information given by vprint().
	 */
	printf("tag VT_UDF, hash id %u\n", up->u_ino);
#ifdef DIAGNOSTIC
	lockmgr_printinfo(&up->u_lock);
	printf("\n");
#endif
	return (0);
}

int
udf_bmap(void *v)
{
	struct vop_bmap_args *ap = v;
	struct unode *up;
	uint32_t max_size;
	daddr64_t lsector;
	int error;

	up = VTOU(ap->a_vp);

	if (ap->a_vpp != NULL)
		*ap->a_vpp = up->u_devvp;
	if (ap->a_bnp == NULL)
		return (0);

	error = udf_bmap_internal(up, ap->a_bn * up->u_ump->um_bsize,
	    &lsector, &max_size);
	if (error)
		return (error);

	/* Translate logical to physical sector number */
	*ap->a_bnp = lsector << (up->u_ump->um_bshift - DEV_BSHIFT);

	/* Punt on read-ahead for now */
	if (ap->a_runp)
		*ap->a_runp = 0;

	return (0);
}

/*
 * The all powerful VOP_LOOKUP().
 */
int
udf_lookup(void *v)
{
	struct vop_lookup_args *ap = v;
	struct vnode *dvp;
	struct vnode *tdp = NULL;
	struct vnode **vpp = ap->a_vpp;
	struct unode *up;
	struct umount *ump;
	struct fileid_desc *fid = NULL;
	struct udf_dirstream *ds;
	struct proc *p;
	u_long nameiop;
	u_long flags;
	char *nameptr;
	long namelen;
	ino_t id = 0;
	int offset, error = 0;
	int numdirpasses, fsize;

	extern struct nchstats nchstats;

	dvp = ap->a_dvp;
	up = VTOU(dvp);
	ump = up->u_ump;
	nameiop = ap->a_cnp->cn_nameiop;
	flags = ap->a_cnp->cn_flags;
	nameptr = ap->a_cnp->cn_nameptr;
	namelen = ap->a_cnp->cn_namelen;
	fsize = letoh64(up->u_fentry->inf_len);
	p = ap->a_cnp->cn_proc;
	*vpp = NULL;

	/*
	 * Make sure the process can scan the requested directory.
	 */
	error = VOP_ACCESS(dvp, VEXEC, ap->a_cnp->cn_cred, p);
	if (error)
		return (error);

	/*
	 * Check if the (directory, name) tuple has been already cached.
	 */
	error = cache_lookup(dvp, vpp, ap->a_cnp);
	if (error >= 0)
		return (error);
	else
		error = 0;

	/*
	 * If dvp is what's being looked up, then return it.
	 */
	if (ap->a_cnp->cn_namelen == 1 && ap->a_cnp->cn_nameptr[0] == '.') {
		VREF(dvp);
		*vpp = dvp;
		return (0);
	}

	/*
	 * If this is a LOOKUP and we've already partially searched through
	 * the directory, pick up where we left off and flag that the
	 * directory may need to be searched twice.  For a full description,
	 * see /sys/isofs/cd9660/cd9660_lookup.c:cd9660_lookup()
	 */
	if (nameiop != LOOKUP || up->u_diroff == 0 || up->u_diroff > fsize) {
		offset = 0;
		numdirpasses = 1;
	} else {
		offset = up->u_diroff;
		numdirpasses = 2;
		nchstats.ncs_2passes++;
	}

lookloop:
	ds = udf_opendir(up, offset, fsize, ump);

	while ((fid = udf_getfid(ds)) != NULL) {
		/* Check for a valid FID tag. */
		if (udf_checktag(&fid->tag, TAGID_FID)) {
			printf("udf_lookup: Invalid tag\n");
			error = EIO;
			break;
		}

		/* Is this a deleted file? */
		if (fid->file_char & UDF_FILE_CHAR_DEL)
			continue;

		if ((fid->l_fi == 0) && (fid->file_char & UDF_FILE_CHAR_PAR)) {
			if (flags & ISDOTDOT) {
				id = udf_getid(&fid->icb);
				break;
			}
		} else {
			if (!(udf_cmpname(&fid->data[fid->l_iu],
			    nameptr, fid->l_fi, namelen, ump))) {
				id = udf_getid(&fid->icb);
				break;
			}
		}
	}

	if (!error)
		error = ds->error;

	if (error) {
		udf_closedir(ds);
		return (error);
	}

	/* Did we have a match? */
	if (id) {
		error = udf_vget(ump->um_mountp, id, &tdp);
		if (!error) {
			/*
			 * Remember where this entry was if it's the final
			 * component.
			 */
			if ((flags & ISLASTCN) && nameiop == LOOKUP)
				up->u_diroff = ds->offset + ds->off;
			if (numdirpasses == 2)
				nchstats.ncs_pass2++;
			if (!(flags & LOCKPARENT) || !(flags & ISLASTCN)) {
				ap->a_cnp->cn_flags |= PDIRUNLOCK;
				VOP_UNLOCK(dvp, 0, p);
			}

			*vpp = tdp;
		}
	} else {
		/* Name wasn't found on this pass.  Do another pass? */
		if (numdirpasses == 2) {
			numdirpasses--;
			offset = 0;
			udf_closedir(ds);
			goto lookloop;
		}

		if ((flags & ISLASTCN) &&
		    (nameiop == CREATE || nameiop == RENAME)) {
			error = EROFS;
		} else {
			error = ENOENT;
		}
	}

	/*
	 * Cache the result of this lookup.
	 */
	if (flags & MAKEENTRY)
		cache_enter(dvp, *vpp, ap->a_cnp);

	udf_closedir(ds);

	return (error);
}

int
udf_inactive(void *v)
{
	struct vop_inactive_args *ap = v;
	struct vnode *vp = ap->a_vp;
	struct proc *p = ap->a_p;

	/*
	 * No need to sync anything, so just unlock the vnode and return.
	 */
	VOP_UNLOCK(vp, 0, p);

	return (0);
}

int
udf_reclaim(void *v)
{
	struct vop_reclaim_args *ap = v;
	struct vnode *vp;
	struct unode *up;

	vp = ap->a_vp;
	up = VTOU(vp);

	if (up != NULL) {
		udf_hashrem(up);
		if (up->u_devvp) {
			vrele(up->u_devvp);
			up->u_devvp = 0;
		}

		if (up->u_fentry != NULL)
			free(up->u_fentry, M_UDFFENTRY);

		pool_put(&unode_pool, up);
		vp->v_data = NULL;
	}
	
	return (0);
}

/*
 * Read the block and then set the data pointer to correspond with the
 * offset passed in.  Only read in at most 'size' bytes, and then set 'size'
 * to the number of bytes pointed to.  If 'size' is zero, try to read in a
 * whole extent.
 *
 * Note that *bp may be assigned error or not.
 *
 */
int
udf_readatoffset(struct unode *up, int *size, off_t offset,
    struct buf **bp, uint8_t **data)
{
	struct umount *ump;
	struct file_entry *fentry = NULL;
	struct buf *bp1;
	uint32_t max_size;
	daddr64_t sector;
	int error;

	ump = up->u_ump;

	*bp = NULL;
	error = udf_bmap_internal(up, offset, &sector, &max_size);
	if (error == UDF_INVALID_BMAP) {
		/*
		 * This error means that the file *data* is stored in the
		 * allocation descriptor field of the file entry.
		 */
		fentry = up->u_fentry;
		*data = &fentry->data[letoh32(fentry->l_ea)];
		*size = letoh32(fentry->l_ad);
		return (0);
	} else if (error != 0) {
		return (error);
	}

	/* Adjust the size so that it is within range */
	if (*size == 0 || *size > max_size)
		*size = max_size;
	*size = min(*size, MAXBSIZE);

	if ((error = udf_readlblks(ump, sector, *size, bp))) {
		printf("warning: udf_readlblks returned error %d\n", error);
		/* note: *bp may be non-NULL */
		return (error);
	}

	bp1 = *bp;
	*data = (uint8_t *)&bp1->b_data[offset % ump->um_bsize];
	return (0);
}

/*
 * Translate a file offset into a logical block and then into a physical
 * block.
 */
int
udf_bmap_internal(struct unode *up, off_t offset, daddr64_t *sector,
    uint32_t *max_size)
{
	struct umount *ump;
	struct file_entry *fentry;
	void *icb;
	struct icb_tag *tag;
	uint32_t icblen = 0;
	daddr64_t lsector;
	int ad_offset, ad_num = 0;
	int i, p_offset;

	ump = up->u_ump;
	fentry = up->u_fentry;
	tag = &fentry->icbtag;

	switch (letoh16(tag->strat_type)) {
	case 4:
		break;

	case 4096:
		printf("Cannot deal with strategy4096 yet!\n");
		return (ENODEV);

	default:
		printf("Unknown strategy type %d\n", tag->strat_type);
		return (ENODEV);
	}

	switch (letoh16(tag->flags) & 0x7) {
	case 0:
		/*
		 * The allocation descriptor field is filled with short_ad's.
		 * If the offset is beyond the current extent, look for the
		 * next extent.
		 */
		do {
			offset -= icblen;
			ad_offset = sizeof(struct short_ad) * ad_num;
			if (ad_offset > letoh32(fentry->l_ad)) {
				printf("File offset out of bounds\n");
				return (EINVAL);
			}
			icb = GETICB(short_ad, fentry,
			    letoh32(fentry->l_ea) + ad_offset);
			icblen = GETICBLEN(short_ad, icb);
			ad_num++;
		} while(offset >= icblen);

		lsector = (offset  >> ump->um_bshift) +
		    letoh32(((struct short_ad *)(icb))->pos);

		*max_size = GETICBLEN(short_ad, icb);

		break;
	case 1:
		/*
		 * The allocation descriptor field is filled with long_ad's
		 * If the offset is beyond the current extent, look for the
		 * next extent.
		 */
		do {
			offset -= icblen;
			ad_offset = sizeof(struct long_ad) * ad_num;
			if (ad_offset > letoh32(fentry->l_ad)) {
				printf("File offset out of bounds\n");
				return (EINVAL);
			}
			icb = GETICB(long_ad, fentry,
			    letoh32(fentry->l_ea) + ad_offset);
			icblen = GETICBLEN(long_ad, icb);
			ad_num++;
		} while(offset >= icblen);

		lsector = (offset >> ump->um_bshift) +
		    letoh32(((struct long_ad *)(icb))->loc.lb_num);

		*max_size = GETICBLEN(long_ad, icb);

		break;
	case 3:
		/*
		 * This type means that the file *data* is stored in the
		 * allocation descriptor field of the file entry.
		 */
		*max_size = 0;
		*sector = up->u_ino + ump->um_start;

		return (UDF_INVALID_BMAP);
	case 2:
		/* DirectCD does not use extended_ad's */
	default:
		printf("Unsupported allocation descriptor %d\n",
		       tag->flags & 0x7);
		return (ENODEV);
	}

	*sector = lsector + ump->um_start;

	/*
	 * Check the sparing table.  Each entry represents the beginning of
	 * a packet.
	 */
	if (ump->um_stbl != NULL) {
		for (i = 0; i< ump->um_stbl_len; i++) {
			p_offset =
			    lsector - letoh32(ump->um_stbl->entries[i].org);
			if ((p_offset < ump->um_psecs) && (p_offset >= 0)) {
				*sector =
				   letoh32(ump->um_stbl->entries[i].map) +
				    p_offset;
				break;
			}
		}
	}

	return (0);
}
