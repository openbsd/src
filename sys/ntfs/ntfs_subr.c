/*	$OpenBSD: ntfs_subr.c,v 1.24 2011/07/04 04:30:41 tedu Exp $	*/
/*	$NetBSD: ntfs_subr.c,v 1.4 2003/04/10 21:37:32 jdolecek Exp $	*/

/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko (semenu@FreeBSD.org)
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
 *	Id: ntfs_subr.c,v 1.4 1999/05/12 09:43:01 semenu Exp
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/rwlock.h>

#include <miscfs/specfs/specdev.h>

/* #define NTFS_DEBUG 1 */
#include <ntfs/ntfs.h>
#include <ntfs/ntfsmount.h>
#include <ntfs/ntfs_inode.h>
#include <ntfs/ntfs_vfsops.h>
#include <ntfs/ntfs_subr.h>
#include <ntfs/ntfs_compr.h>
#include <ntfs/ntfs_ihash.h>

#if defined(NTFS_DEBUG)
int ntfs_debug = NTFS_DEBUG;
#endif

#ifdef MALLOC_DEFINE
MALLOC_DEFINE(M_NTFSNTVATTR, "NTFS vattr", "NTFS file attribute information");
MALLOC_DEFINE(M_NTFSRDATA, "NTFS res data", "NTFS resident data");
MALLOC_DEFINE(M_NTFSRUN, "NTFS vrun", "NTFS vrun storage");
MALLOC_DEFINE(M_NTFSDECOMP, "NTFS decomp", "NTFS decompression temporary");
#endif

/* Local struct used in ntfs_ntlookupfile() */
struct ntfs_lookup_ctx {
	u_int32_t	aoff;
	u_int32_t	rdsize;
	cn_t		cn;
	struct ntfs_lookup_ctx *prev;
};

static int ntfs_ntlookupattr(struct ntfsmount *, const char *, int, int *, char **);
static int ntfs_findvattr(struct ntfsmount *, struct ntnode *, struct ntvattr **, struct ntvattr **, u_int32_t, const char *, size_t, cn_t);
static int ntfs_uastricmp(struct ntfsmount *, const wchar *, size_t, const char *, size_t);
static int ntfs_uastrcmp(struct ntfsmount *, const wchar *, size_t, const char *, size_t);

/* table for mapping Unicode chars into uppercase; it's filled upon first
 * ntfs mount, freed upon last ntfs umount */
static wchar *ntfs_toupper_tab;
#define NTFS_U28(ch)		((((ch) & 0xE0) == 0) ? '_' : (ch) & 0xFF)
#define NTFS_TOUPPER(ch)	(ntfs_toupper_tab[(unsigned char)(ch)])
struct rwlock ntfs_toupper_lock = RWLOCK_INITIALIZER("ntfs_toupper");
static signed int ntfs_toupper_usecount;

/* support macro for ntfs_ntvattrget() */
#define NTFS_AALPCMP(aalp,type,name,namelen) (				\
  (aalp->al_type == type) && (aalp->al_namelen == namelen) &&		\
  !ntfs_uastrcmp(ntmp, aalp->al_name,aalp->al_namelen,name,namelen) )

/*
 * 
 */
int
ntfs_ntvattrrele(vap)
	struct ntvattr * vap;
{
	dprintf(("ntfs_ntvattrrele: ino: %d, type: 0x%x\n",
		 vap->va_ip->i_number, vap->va_type));

	ntfs_ntrele(vap->va_ip);

	return (0);
}

/*
 * find the attribute in the ntnode
 */
static int
ntfs_findvattr(ntmp, ip, lvapp, vapp, type, name, namelen, vcn)
	struct ntfsmount *ntmp;
	struct ntnode *ip;
	struct ntvattr **lvapp, **vapp;
	u_int32_t type;
	const char *name;
	size_t namelen;
	cn_t vcn;
{
	int error;
	struct ntvattr *vap;

	if((ip->i_flag & IN_LOADED) == 0) {
		dprintf(("ntfs_findvattr: node not loaded, ino: %d\n",
		       ip->i_number));
		error = ntfs_loadntnode(ntmp,ip);
		if (error) {
			printf("ntfs_findvattr: FAILED TO LOAD INO: %d\n",
			       ip->i_number);
			return (error);
		}
	}

	*lvapp = NULL;
	*vapp = NULL;
	LIST_FOREACH(vap, &ip->i_valist, va_list) {
		ddprintf(("ntfs_findvattr: type: 0x%x, vcn: %d - %d\n", \
			  vap->va_type, (u_int32_t) vap->va_vcnstart, \
			  (u_int32_t) vap->va_vcnend));
		if ((vap->va_type == type) &&
		    (vap->va_vcnstart <= vcn) && (vap->va_vcnend >= vcn) &&
		    (vap->va_namelen == namelen) &&
		    (strncmp(name, vap->va_name, namelen) == 0)) {
			*vapp = vap;
			ntfs_ntref(vap->va_ip);
			return (0);
		}
		if (vap->va_type == NTFS_A_ATTRLIST)
			*lvapp = vap;
	}

	return (-1);
}

/*
 * Search attribute specified in ntnode (load ntnode if necessary).
 * If not found but ATTR_A_ATTRLIST present, read it in and search through.
 * VOP_VGET node needed, and lookup through its ntnode (load if necessary).
 *
 * ntnode should be locked
 */
int
ntfs_ntvattrget(
		struct ntfsmount * ntmp,
		struct ntnode * ip,
		u_int32_t type,
		const char *name,
		cn_t vcn,
		struct ntvattr ** vapp)
{
	struct ntvattr *lvap = NULL;
	struct attr_attrlist *aalp;
	struct attr_attrlist *nextaalp;
	struct vnode   *newvp;
	struct ntnode  *newip;
	caddr_t         alpool;
	size_t		namelen, len;
	int             error;

	*vapp = NULL;

	if (name) {
		dprintf(("ntfs_ntvattrget: " \
			 "ino: %d, type: 0x%x, name: %s, vcn: %d\n", \
			 ip->i_number, type, name, (u_int32_t) vcn));
		namelen = strlen(name);
	} else {
		dprintf(("ntfs_ntvattrget: " \
			 "ino: %d, type: 0x%x, vcn: %d\n", \
			 ip->i_number, type, (u_int32_t) vcn));
		name = "";
		namelen = 0;
	}

	error = ntfs_findvattr(ntmp, ip, &lvap, vapp, type, name, namelen, vcn);
	if (error >= 0)
		return (error);

	if (!lvap) {
		dprintf(("ntfs_ntvattrget: UNEXISTED ATTRIBUTE: " \
		       "ino: %d, type: 0x%x, name: %s, vcn: %d\n", \
		       ip->i_number, type, name, (u_int32_t) vcn));
		return (ENOENT);
	}
	/* Scan $ATTRIBUTE_LIST for requested attribute */
	len = lvap->va_datalen;
	alpool = malloc(len, M_TEMP, M_WAITOK);
	error = ntfs_readntvattr_plain(ntmp, ip, lvap, 0, len, alpool, &len,
			NULL);
	if (error)
		goto out;

	aalp = (struct attr_attrlist *) alpool;
	nextaalp = NULL;

	for(; len > 0; aalp = nextaalp) {
		dprintf(("ntfs_ntvattrget: " \
			 "attrlist: ino: %d, attr: 0x%x, vcn: %d\n", \
			 aalp->al_inumber, aalp->al_type, \
			 (u_int32_t) aalp->al_vcnstart));

		if (len > aalp->reclen) {
			nextaalp = NTFS_NEXTREC(aalp, struct attr_attrlist *);
		} else {
			nextaalp = NULL;
		}
		len -= aalp->reclen;

		if (!NTFS_AALPCMP(aalp, type, name, namelen) ||
		    (nextaalp && (nextaalp->al_vcnstart <= vcn) &&
		     NTFS_AALPCMP(nextaalp, type, name, namelen)))
			continue;

		dprintf(("ntfs_ntvattrget: attribute in ino: %d\n",
				 aalp->al_inumber));

		/* this is not a main record, so we can't use just plain
		   vget() */
		error = ntfs_vgetex(ntmp->ntm_mountp, aalp->al_inumber,
				NTFS_A_DATA, NULL, LK_EXCLUSIVE,
				VG_EXT, curproc, &newvp);
		if (error) {
			printf("ntfs_ntvattrget: CAN'T VGET INO: %d\n",
			       aalp->al_inumber);
			goto out;
		}
		newip = VTONT(newvp);
		/* XXX have to lock ntnode */
		error = ntfs_findvattr(ntmp, newip, &lvap, vapp,
				type, name, namelen, vcn);
		vput(newvp);
		if (error == 0)
			goto out;
		printf("ntfs_ntvattrget: ATTRLIST ERROR.\n");
		break;
	}
	error = ENOENT;

	dprintf(("ntfs_ntvattrget: UNEXISTED ATTRIBUTE: " \
	       "ino: %d, type: 0x%x, name: %.*s, vcn: %d\n", \
	       ip->i_number, type, (int) namelen, name, (u_int32_t) vcn));
out:
	free(alpool, M_TEMP);
	return (error);
}

/*
 * Read ntnode from disk, make ntvattr list.
 *
 * ntnode should be locked
 */
int
ntfs_loadntnode(
	      struct ntfsmount * ntmp,
	      struct ntnode * ip)
{
	struct filerec  *mfrp;
	daddr64_t         bn;
	int		error,off;
	struct attr    *ap;
	struct ntvattr *nvap;

	dprintf(("ntfs_loadntnode: loading ino: %d\n",ip->i_number));

	mfrp = malloc(ntfs_bntob(ntmp->ntm_bpmftrec), M_TEMP, M_WAITOK);

	if (ip->i_number < NTFS_SYSNODESNUM) {
		struct buf     *bp;

		dprintf(("ntfs_loadntnode: read system node\n"));

		bn = ntfs_cntobn(ntmp->ntm_mftcn) +
			ntmp->ntm_bpmftrec * ip->i_number;

		error = bread(ntmp->ntm_devvp, bn,
		    ntfs_bntob(ntmp->ntm_bpmftrec), &bp);
		if (error) {
			printf("ntfs_loadntnode: BREAD FAILED\n");
			brelse(bp);
			goto out;
		}
		memcpy(mfrp, bp->b_data, ntfs_bntob(ntmp->ntm_bpmftrec));
		brelse(bp);
	} else {
		struct vnode   *vp;

		vp = ntmp->ntm_sysvn[NTFS_MFTINO];
		error = ntfs_readattr(ntmp, VTONT(vp), NTFS_A_DATA, NULL,
			       ip->i_number * ntfs_bntob(ntmp->ntm_bpmftrec),
			       ntfs_bntob(ntmp->ntm_bpmftrec), mfrp, NULL);
		if (error) {
			printf("ntfs_loadntnode: ntfs_readattr failed\n");
			goto out;
		}
	}

	/* Check if magic and fixups are correct */
	error = ntfs_procfixups(ntmp, NTFS_FILEMAGIC, (caddr_t)mfrp,
				ntfs_bntob(ntmp->ntm_bpmftrec));
	if (error) {
		printf("ntfs_loadntnode: BAD MFT RECORD %d\n",
		       (u_int32_t) ip->i_number);
		goto out;
	}

	dprintf(("ntfs_loadntnode: load attrs for ino: %d\n",ip->i_number));
	off = mfrp->fr_attroff;
	ap = (struct attr *) ((caddr_t)mfrp + off);

	LIST_INIT(&ip->i_valist);
	
	while (ap->a_hdr.a_type != -1) {
		error = ntfs_attrtontvattr(ntmp, &nvap, ap);
		if (error)
			break;
		nvap->va_ip = ip;

		LIST_INSERT_HEAD(&ip->i_valist, nvap, va_list);

		off += ap->a_hdr.reclen;
		ap = (struct attr *) ((caddr_t)mfrp + off);
	}
	if (error) {
		printf("ntfs_loadntnode: failed to load attr ino: %d\n",
		       ip->i_number);
		goto out;
	}

	ip->i_mainrec = mfrp->fr_mainrec;
	ip->i_nlink = mfrp->fr_nlink;
	ip->i_frflag = mfrp->fr_flags;

	ip->i_flag |= IN_LOADED;

out:
	free(mfrp, M_TEMP);
	return (error);
}

/*
 * Routine locks ntnode and increase usecount, just opposite of
 * ntfs_ntput().
 */
int
ntfs_ntget(
	struct ntnode *ip,
	struct proc *p
	)
{
	dprintf(("ntfs_ntget: get ntnode %d: %p, usecount: %d\n",
		ip->i_number, ip, ip->i_usecount));

	ip->i_usecount++;

	rw_enter_write(&ip->i_lock);

	return 0;
}

/*
 * Routine search ntnode in hash, if found: lock, inc usecount and return.
 * If not in hash allocate structure for ntnode, prefill it, lock,
 * inc count and return.
 *
 * ntnode returned locked
 */
int
ntfs_ntlookup(
	   struct ntfsmount * ntmp,
	   ino_t ino,
	   struct ntnode ** ipp,
	   struct proc * p)
{
	struct ntnode  *ip;

	dprintf(("ntfs_ntlookup: looking for ntnode %d\n", ino));

	do {
		if ((ip = ntfs_nthashlookup(ntmp->ntm_dev, ino)) != NULL) {
			ntfs_ntget(ip, p);
			dprintf(("ntfs_ntlookup: ntnode %d: %p, usecount: %d\n",
				ino, ip, ip->i_usecount));
			*ipp = ip;
			return (0);
		}
	} while (rw_enter(&ntfs_hashlock, RW_WRITE | RW_SLEEPFAIL));

	ip = malloc(sizeof(*ip), M_NTFSNTNODE, M_WAITOK | M_ZERO);
	ddprintf(("ntfs_ntlookup: allocating ntnode: %d: %p\n", ino, ip));

	/* Generic initialization */
	ip->i_devvp = ntmp->ntm_devvp;
	ip->i_dev = ntmp->ntm_dev;
	ip->i_number = ino;
	ip->i_mp = ntmp;

	LIST_INIT(&ip->i_fnlist);
	vref(ip->i_devvp);

	/* init lock and lock the newborn ntnode */
	rw_init(&ip->i_lock, "ntnode");
	ntfs_ntget(ip, p);

	ntfs_nthashins(ip);

	rw_exit(&ntfs_hashlock);

	*ipp = ip;

	dprintf(("ntfs_ntlookup: ntnode %d: %p, usecount: %d\n",
		ino, ip, ip->i_usecount));

	return (0);
}

/*
 * Decrement usecount of ntnode and unlock it, if usecount reach zero,
 * deallocate ntnode.
 *
 * ntnode should be locked on entry, and unlocked on return.
 */
void
ntfs_ntput(
	struct ntnode *ip,
	struct proc *p
	)
{
	struct ntvattr *vap;

	dprintf(("ntfs_ntput: rele ntnode %d: %p, usecount: %d\n",
		ip->i_number, ip, ip->i_usecount));

	ip->i_usecount--;

#ifdef DIAGNOSTIC
	if (ip->i_usecount < 0) {
		panic("ntfs_ntput: ino: %d usecount: %d ",
		      ip->i_number,ip->i_usecount);
	}
#endif

	if (ip->i_usecount > 0) {
		rw_exit_write(&ip->i_lock);
		return;
	}

	dprintf(("ntfs_ntput: deallocating ntnode: %d\n", ip->i_number));

	if (LIST_FIRST(&ip->i_fnlist))
		panic("ntfs_ntput: ntnode has fnodes");

	ntfs_nthashrem(ip);

	while ((vap = LIST_FIRST(&ip->i_valist)) != NULL) {
		LIST_REMOVE(vap, va_list);
		ntfs_freentvattr(vap);
	}

	vrele(ip->i_devvp);
	free(ip, M_NTFSNTNODE);
}

/*
 * increment usecount of ntnode 
 */
void
ntfs_ntref(ip)
	struct ntnode *ip;
{
	ip->i_usecount++;

	dprintf(("ntfs_ntref: ino %d, usecount: %d\n",
		ip->i_number, ip->i_usecount));
			
}

/*
 * Decrement usecount of ntnode.
 */
void
ntfs_ntrele(ip)
	struct ntnode *ip;
{
	dprintf(("ntfs_ntrele: rele ntnode %d: %p, usecount: %d\n",
		ip->i_number, ip, ip->i_usecount));

	ip->i_usecount--;

	if (ip->i_usecount < 0)
		panic("ntfs_ntrele: ino: %d usecount: %d ",
		      ip->i_number,ip->i_usecount);
}

/*
 * Deallocate all memory allocated for ntvattr
 */
void
ntfs_freentvattr(vap)
	struct ntvattr * vap;
{
	if (vap->va_flag & NTFS_AF_INRUN) {
		if (vap->va_vruncn)
			free(vap->va_vruncn, M_NTFSRUN);
		if (vap->va_vruncl)
			free(vap->va_vruncl, M_NTFSRUN);
	} else {
		if (vap->va_datap)
			free(vap->va_datap, M_NTFSRDATA);
	}
	free(vap, M_NTFSNTVATTR);
}

/*
 * Convert disk image of attribute into ntvattr structure,
 * runs are expanded also.
 */
int
ntfs_attrtontvattr(
		   struct ntfsmount * ntmp,
		   struct ntvattr ** rvapp,
		   struct attr * rap)
{
	int             error, i;
	struct ntvattr *vap;

	error = 0;
	*rvapp = NULL;

	vap = malloc(sizeof(*vap), M_NTFSNTVATTR, M_WAITOK | M_ZERO);
	vap->va_ip = NULL;
	vap->va_flag = rap->a_hdr.a_flag;
	vap->va_type = rap->a_hdr.a_type;
	vap->va_compression = rap->a_hdr.a_compression;
	vap->va_index = rap->a_hdr.a_index;

	ddprintf(("type: 0x%x, index: %d", vap->va_type, vap->va_index));

	vap->va_namelen = rap->a_hdr.a_namelen;
	if (rap->a_hdr.a_namelen) {
		wchar *unp = (wchar *) ((caddr_t) rap + rap->a_hdr.a_nameoff);
		ddprintf((", name:["));
		for (i = 0; i < vap->va_namelen; i++) {
			vap->va_name[i] = unp[i];
			ddprintf(("%c", vap->va_name[i]));
		}
		ddprintf(("]"));
	}
	if (vap->va_flag & NTFS_AF_INRUN) {
		ddprintf((", nonres."));
		vap->va_datalen = rap->a_nr.a_datalen;
		vap->va_allocated = rap->a_nr.a_allocated;
		vap->va_vcnstart = rap->a_nr.a_vcnstart;
		vap->va_vcnend = rap->a_nr.a_vcnend;
		vap->va_compressalg = rap->a_nr.a_compressalg;
		error = ntfs_runtovrun(&(vap->va_vruncn), &(vap->va_vruncl),
				       &(vap->va_vruncnt),
				       (caddr_t) rap + rap->a_nr.a_dataoff);
	} else {
		vap->va_compressalg = 0;
		ddprintf((", res."));
		vap->va_datalen = rap->a_r.a_datalen;
		vap->va_allocated = rap->a_r.a_datalen;
		vap->va_vcnstart = 0;
		vap->va_vcnend = ntfs_btocn(vap->va_allocated);
		vap->va_datap = malloc(vap->va_datalen, M_NTFSRDATA, M_WAITOK);
		memcpy(vap->va_datap, (caddr_t) rap + rap->a_r.a_dataoff,
		       rap->a_r.a_datalen);
	}
	ddprintf((", len: %d", vap->va_datalen));

	if (error)
		free(vap, M_NTFSNTVATTR);
	else
		*rvapp = vap;

	ddprintf(("\n"));

	return (error);
}

/*
 * Expand run into more utilizable and more memory eating format.
 */
int
ntfs_runtovrun(
	       cn_t ** rcnp,
	       cn_t ** rclp,
	       u_long * rcntp,
	       u_int8_t * run)
{
	u_int32_t       off;
	u_int32_t       sz, i;
	cn_t           *cn;
	cn_t           *cl;
	u_long		cnt;
	cn_t		prev;
	cn_t		tmp;

	off = 0;
	cnt = 0;
	i = 0;
	while (run[off]) {
		off += (run[off] & 0xF) + ((run[off] >> 4) & 0xF) + 1;
		cnt++;
	}
	cn = malloc(cnt * sizeof(cn_t), M_NTFSRUN, M_WAITOK);
	cl = malloc(cnt * sizeof(cn_t), M_NTFSRUN, M_WAITOK);

	off = 0;
	cnt = 0;
	prev = 0;
	while (run[off]) {

		sz = run[off++];
		cl[cnt] = 0;

		for (i = 0; i < (sz & 0xF); i++)
			cl[cnt] += (u_int32_t) run[off++] << (i << 3);

		sz >>= 4;
		if (run[off + sz - 1] & 0x80) {
			tmp = ((u_int64_t) - 1) << (sz << 3);
			for (i = 0; i < sz; i++)
				tmp |= (u_int64_t) run[off++] << (i << 3);
		} else {
			tmp = 0;
			for (i = 0; i < sz; i++)
				tmp |= (u_int64_t) run[off++] << (i << 3);
		}
		if (tmp)
			prev = cn[cnt] = prev + tmp;
		else
			cn[cnt] = tmp;

		cnt++;
	}
	*rcnp = cn;
	*rclp = cl;
	*rcntp = cnt;
	return (0);
}

/*
 * Compare unicode and ascii string case insens.
 */
static int
ntfs_uastricmp(ntmp, ustr, ustrlen, astr, astrlen)
	struct ntfsmount *ntmp;
	const wchar *ustr;
	size_t ustrlen;
	const char *astr;
	size_t astrlen;
{
	size_t  i;
	int             res;
	const char *astrend = astr + astrlen;

	for (i = 0; i < ustrlen && astr < astrend; i++) {
		res = (*ntmp->ntm_wcmp)(NTFS_TOUPPER(ustr[i]),
				NTFS_TOUPPER((*ntmp->ntm_wget)(&astr)) );
		if (res)
			return res;
	}

	if (i == ustrlen && astr == astrend)
		return 0;
	else if (i == ustrlen)
		return -1;
	else
		return 1;
}

/*
 * Compare unicode and ascii string case sens.
 */
static int
ntfs_uastrcmp(ntmp, ustr, ustrlen, astr, astrlen)
	struct ntfsmount *ntmp;
	const wchar *ustr;
	size_t ustrlen;
	const char *astr;
	size_t astrlen;
{
	size_t             i;
	int             res;
	const char *astrend = astr + astrlen;

	for (i = 0; (i < ustrlen) && (astr < astrend); i++) {
		res = (*ntmp->ntm_wcmp)(ustr[i], (*ntmp->ntm_wget)(&astr));
		if (res)
			return res;
	}

	if (i == ustrlen && astr == astrend)
		return 0;
	else if (i == ustrlen)
		return -1;
	else
		return 1;
}

/* 
 * Search fnode in ntnode, if not found allocate and preinitialize.
 *
 * ntnode should be locked on entry.
 */
int
ntfs_fget(
	struct ntfsmount *ntmp,
	struct ntnode *ip,
	int attrtype,
	char *attrname,
	struct fnode **fpp)
{
	struct fnode *fp;

	dprintf(("ntfs_fget: ino: %d, attrtype: 0x%x, attrname: %s\n",
		ip->i_number,attrtype, attrname?attrname:""));
	*fpp = NULL;
	LIST_FOREACH(fp, &ip->i_fnlist, f_fnlist) {
		dprintf(("ntfs_fget: fnode: attrtype: %d, attrname: %s\n",
			fp->f_attrtype, fp->f_attrname?fp->f_attrname:""));

		if ((attrtype == fp->f_attrtype) && 
		    ((!attrname && !fp->f_attrname) ||
		     (attrname && fp->f_attrname &&
		      !strcmp(attrname,fp->f_attrname)))){
			dprintf(("ntfs_fget: found existed: %p\n",fp));
			*fpp = fp;
		}
	}

	if (*fpp)
		return (0);

	fp = malloc(sizeof(*fp), M_NTFSFNODE, M_WAITOK | M_ZERO);
	dprintf(("ntfs_fget: allocating fnode: %p\n",fp));

	fp->f_ip = ip;
	fp->f_attrname = attrname;
	if (fp->f_attrname) fp->f_flag |= FN_AATTRNAME;
	fp->f_attrtype = attrtype;

	ntfs_ntref(ip);

	LIST_INSERT_HEAD(&ip->i_fnlist, fp, f_fnlist);

	*fpp = fp;

	return (0);
}

/*
 * Deallocate fnode, remove it from ntnode's fnode list.
 *
 * ntnode should be locked.
 */
void
ntfs_frele(
	struct fnode *fp)
{
	struct ntnode *ip = FTONT(fp);

	dprintf(("ntfs_frele: fnode: %p for %d: %p\n", fp, ip->i_number, ip));

	dprintf(("ntfs_frele: deallocating fnode\n"));
	LIST_REMOVE(fp,f_fnlist);
	if (fp->f_flag & FN_AATTRNAME)
		free(fp->f_attrname, M_TEMP);
	if (fp->f_dirblbuf)
		free(fp->f_dirblbuf, M_NTFSDIR);
	free(fp, M_NTFSFNODE);
	ntfs_ntrele(ip);
}

/*
 * Lookup attribute name in format: [[:$ATTR_TYPE]:$ATTR_NAME], 
 * $ATTR_TYPE is searched in attrdefs read from $AttrDefs.
 * If $ATTR_TYPE not specified, ATTR_A_DATA assumed.
 */
static int
ntfs_ntlookupattr(
		struct ntfsmount * ntmp,
		const char * name,
		int namelen,
		int *attrtype,
		char **attrname)
{
	const char *sys;
	size_t syslen, i;
	struct ntvattrdef *adp;

	if (namelen == 0)
		return (0);

	if (name[0] == '$') {
		sys = name;
		for (syslen = 0; syslen < namelen; syslen++) {
			if(sys[syslen] == ':') {
				name++;
				namelen--;
				break;
			}
		}
		name += syslen;
		namelen -= syslen;

		adp = ntmp->ntm_ad;
		for (i = 0; i < ntmp->ntm_adnum; i++, adp++){
			if (syslen != adp->ad_namelen || 
			    strncmp(sys, adp->ad_name, syslen) != 0)
				continue;

			*attrtype = adp->ad_type;
			goto out;
		}
		return (ENOENT);
	}

    out:
	if (namelen) {
		*attrname = malloc(namelen + 1, M_TEMP, M_WAITOK);
		memcpy(*attrname, name, namelen);
		(*attrname)[namelen] = '\0';
		*attrtype = NTFS_A_DATA;
	}

	return (0);
}

/*
 * Lookup specified node for filename, matching cnp,
 * return fnode filled.
 */
int
ntfs_ntlookupfile(
	      struct ntfsmount * ntmp,
	      struct vnode * vp,
	      struct componentname * cnp,
	      struct vnode ** vpp,
	      struct proc *p)
{
	struct fnode   *fp = VTOF(vp);
	struct ntnode  *ip = FTONT(fp);
	struct ntvattr *vap;	/* Root attribute */
	cn_t            cn = 0;	/* VCN in current attribute */
	caddr_t         rdbuf;	/* Buffer to read directory's blocks  */
	u_int32_t       blsize;
	u_int32_t       rdsize;	/* Length of data to read from current block */
	struct attr_indexentry *iep;
	int             error, res, anamelen, fnamelen;
	const char     *fname,*aname;
	u_int32_t       aoff;
	int attrtype = NTFS_A_DATA;
	char *attrname = NULL;
	struct fnode   *nfp;
	struct vnode   *nvp;
	enum vtype	f_type;
	int fullscan = 0;
	struct ntfs_lookup_ctx *lookup_ctx = NULL, *tctx;

	error = ntfs_ntget(ip, p);

	if (error)
		return (error);

	error = ntfs_ntvattrget(ntmp, ip, NTFS_A_INDXROOT, "$I30", 0, &vap);
	if (error || (vap->va_flag & NTFS_AF_INRUN))
		return (ENOTDIR);

	/*
	 * Divide file name into: foofilefoofilefoofile[:attrspec]
	 * Store like this:       fname:fnamelen       [aname:anamelen]
	 */
	fname = cnp->cn_nameptr;
	aname = NULL;
	anamelen = 0;
	for (fnamelen = 0; fnamelen < cnp->cn_namelen; fnamelen++)
		if(fname[fnamelen] == ':') {
			aname = fname + fnamelen + 1;
			anamelen = cnp->cn_namelen - fnamelen - 1;
			dprintf(("ntfs_ntlookupfile: %s (%d), attr: %s (%d)\n",
				fname, fnamelen, aname, anamelen));
			break;
		}

	blsize = vap->va_a_iroot->ir_size;
	dprintf(("ntfs_ntlookupfile: blksz: %d\n", blsize));

	rdbuf = malloc(blsize, M_TEMP, M_WAITOK);

    loop:
	rdsize = vap->va_datalen;
	dprintf(("ntfs_ntlookupfile: rdsz: %d\n", rdsize));

	error = ntfs_readattr(ntmp, ip, NTFS_A_INDXROOT, "$I30",
			       0, rdsize, rdbuf, NULL);
	if (error)
		goto fail;

	aoff = sizeof(struct attr_indexroot);

	do {
		iep = (struct attr_indexentry *) (rdbuf + aoff);

		for (; !(iep->ie_flag & NTFS_IEFLAG_LAST) && (rdsize > aoff);
			aoff += iep->reclen,
			iep = (struct attr_indexentry *) (rdbuf + aoff))
		{
			ddprintf(("scan: %d, %d\n",
				  (u_int32_t) iep->ie_number,
				  (u_int32_t) iep->ie_fnametype));
 
			/* check the name - the case-insensitive check
			 * has to come first, to break from this for loop
			 * if needed, so we can dive correctly */
			res = ntfs_uastricmp(ntmp, iep->ie_fname,
				iep->ie_fnamelen, fname, fnamelen);
			if (!fullscan) {
				if (res > 0) break;
				if (res < 0) continue;
			}

			if (iep->ie_fnametype == 0 ||
			    !(ntmp->ntm_flag & NTFS_MFLAG_CASEINS))
			{
				res = ntfs_uastrcmp(ntmp, iep->ie_fname,
					iep->ie_fnamelen, fname, fnamelen);
				if (res != 0 && !fullscan) continue;
			}

			/* if we perform full scan, the file does not match
			 * and this is subnode, dive */
			if (fullscan && res != 0) {
			    if (iep->ie_flag & NTFS_IEFLAG_SUBNODE) {
				tctx = malloc(sizeof(struct ntfs_lookup_ctx),
					M_TEMP, M_WAITOK);
				tctx->aoff	= aoff + iep->reclen;
				tctx->rdsize	= rdsize;
				tctx->cn	= cn;
				tctx->prev	= lookup_ctx;
				lookup_ctx = tctx;
				break;
			    } else
				continue;
			}

			if (aname) {
				error = ntfs_ntlookupattr(ntmp,
					aname, anamelen,
					&attrtype, &attrname);
				if (error)
					goto fail;
			}

			/* Check if we've found ourselves */
			if ((iep->ie_number == ip->i_number) &&
			    (attrtype == fp->f_attrtype) &&
			    ((!attrname && !fp->f_attrname) ||
			     (attrname && fp->f_attrname &&
			      !strcmp(attrname, fp->f_attrname))))
			{
				vref(vp);
				*vpp = vp;
				error = 0;
				goto fail;
			}

			/* free the buffer returned by ntfs_ntlookupattr() */
			if (attrname) {
				free(attrname, M_TEMP);
				attrname = NULL;
			}

			/* vget node, but don't load it */
			error = ntfs_vgetex(ntmp->ntm_mountp,
				   iep->ie_number, attrtype, attrname,
				   LK_EXCLUSIVE, VG_DONTLOADIN | VG_DONTVALIDFN,
				   curproc, &nvp);
			if (error)
				goto fail;

			nfp = VTOF(nvp);

			if (nfp->f_flag & FN_VALID) {
				*vpp = nvp;
				goto fail;
			}

			nfp->f_fflag = iep->ie_fflag;
			nfp->f_pnumber = iep->ie_fpnumber;
			nfp->f_times = iep->ie_ftimes;

			if((nfp->f_fflag & NTFS_FFLAG_DIR) &&
			   (nfp->f_attrtype == NTFS_A_DATA) &&
			   (nfp->f_attrname == NULL))
				f_type = VDIR;	
			else
				f_type = VREG;	

			nvp->v_type = f_type;

			if ((nfp->f_attrtype == NTFS_A_DATA) &&
			    (nfp->f_attrname == NULL))
			{
				/* Opening default attribute */
				nfp->f_size = iep->ie_fsize;
				nfp->f_allocated = iep->ie_fallocated;
				nfp->f_flag |= FN_PRELOADED;
			} else {
				error = ntfs_filesize(ntmp, nfp,
					    &nfp->f_size, &nfp->f_allocated);
				if (error) {
					vput(nvp);
					goto fail;
				}
			}

			nfp->f_flag &= ~FN_VALID;
			*vpp = nvp;
			goto fail;
		}

		/* Dive if possible */
		if (iep->ie_flag & NTFS_IEFLAG_SUBNODE) {
			dprintf(("ntfs_ntlookupfile: diving\n"));

			cn = *(cn_t *) (rdbuf + aoff +
					iep->reclen - sizeof(cn_t));
			rdsize = blsize;

			error = ntfs_readattr(ntmp, ip, NTFS_A_INDX, "$I30",
					ntfs_cntob(cn), rdsize, rdbuf, NULL);
			if (error)
				goto fail;

			error = ntfs_procfixups(ntmp, NTFS_INDXMAGIC,
						rdbuf, rdsize);
			if (error)
				goto fail;

			aoff = (((struct attr_indexalloc *) rdbuf)->ia_hdrsize +
				0x18);
		} else if (fullscan && lookup_ctx) {
			cn = lookup_ctx->cn;
			aoff = lookup_ctx->aoff;
			rdsize = lookup_ctx->rdsize;

			error = ntfs_readattr(ntmp, ip,
				(cn == 0) ? NTFS_A_INDXROOT : NTFS_A_INDX,
				"$I30", ntfs_cntob(cn), rdsize, rdbuf, NULL);
			if (error)
				goto fail;
			
			if (cn != 0) {
				error = ntfs_procfixups(ntmp, NTFS_INDXMAGIC,
						rdbuf, rdsize);
				if (error)
					goto fail;
			}

			tctx = lookup_ctx;
			lookup_ctx = lookup_ctx->prev;
			free(tctx, M_TEMP);
		} else {
			dprintf(("ntfs_ntlookupfile: nowhere to dive :-(\n"));
			error = ENOENT;
			break;
		}
	} while (1);

	/* perform full scan if no entry was found */
	if (!fullscan && error == ENOENT) {
		fullscan = 1;
		cn = 0;		/* need zero, used by lookup_ctx */

		ddprintf(("ntfs_ntlookupfile: fullscan performed for: %.*s\n",
			(int) fnamelen, fname));
		goto loop;
	}

	dprintf(("finish\n"));

fail:
	if (attrname)
		free(attrname, M_TEMP);
	if (lookup_ctx) {
		while(lookup_ctx) {
			tctx = lookup_ctx;
			lookup_ctx = lookup_ctx->prev;
			free(tctx, M_TEMP);
		}
	}
	ntfs_ntvattrrele(vap);
	ntfs_ntput(ip, p);
	free(rdbuf, M_TEMP);
	return (error);
}

/*
 * Check if name type is permitted to show.
 */
int
ntfs_isnamepermitted(
		     struct ntfsmount * ntmp,
		     struct attr_indexentry * iep)
{
	if (ntmp->ntm_flag & NTFS_MFLAG_ALLNAMES)
		return 1;

	switch (iep->ie_fnametype) {
	case 2:
		ddprintf(("ntfs_isnamepermitted: skipped DOS name\n"));
		return 0;
	case 0: case 1: case 3:
		return 1;
	default:
		printf("ntfs_isnamepermitted: " \
		       "WARNING! Unknown file name type: %d\n",
		       iep->ie_fnametype);
		break;
	}
	return 0;
}

/*
 * Read ntfs dir like stream of attr_indexentry, not like btree of them.
 * This is done by scanning $BITMAP:$I30 for busy clusters and reading them.
 * Of course $INDEX_ROOT:$I30 is read before. Last read values are stored in
 * fnode, so we can skip toward record number num almost immediately.
 * Anyway this is rather slow routine. The problem is that we don't know
 * how many records are there in $INDEX_ALLOCATION:$I30 block.
 */
int
ntfs_ntreaddir(
	       struct ntfsmount * ntmp,
	       struct fnode * fp,
	       u_int32_t num,
	       struct attr_indexentry ** riepp,
	       struct proc *p)
{
	struct ntnode  *ip = FTONT(fp);
	struct ntvattr *vap = NULL;	/* IndexRoot attribute */
	struct ntvattr *bmvap = NULL;	/* BitMap attribute */
	struct ntvattr *iavap = NULL;	/* IndexAllocation attribute */
	caddr_t         rdbuf;		/* Buffer to read directory's blocks  */
	u_int8_t       *bmp = NULL;	/* Bitmap */
	u_int32_t       blsize;		/* Index allocation size (2048) */
	u_int32_t       rdsize;		/* Length of data to read */
	u_int32_t       attrnum;	/* Current attribute type */
	u_int32_t       cpbl = 1;	/* Clusters per directory block */
	u_int32_t       blnum;
	struct attr_indexentry *iep;
	int             error = ENOENT;
	u_int32_t       aoff, cnum;

	dprintf(("ntfs_ntreaddir: read ino: %d, num: %d\n", ip->i_number, num));
	error = ntfs_ntget(ip, p);
	if (error)
		return (error);

	error = ntfs_ntvattrget(ntmp, ip, NTFS_A_INDXROOT, "$I30", 0, &vap);
	if (error)
		return (ENOTDIR);

	if (fp->f_dirblbuf == NULL) {
		fp->f_dirblsz = vap->va_a_iroot->ir_size;
		fp->f_dirblbuf = malloc(MAX(vap->va_datalen,fp->f_dirblsz),
		    M_NTFSDIR, M_WAITOK);
	}

	blsize = fp->f_dirblsz;
	rdbuf = fp->f_dirblbuf;

	dprintf(("ntfs_ntreaddir: rdbuf: %p, blsize: %d\n", rdbuf, blsize));

	if (vap->va_a_iroot->ir_flag & NTFS_IRFLAG_INDXALLOC) {
		error = ntfs_ntvattrget(ntmp, ip, NTFS_A_INDXBITMAP, "$I30",
					0, &bmvap);
		if (error) {
			error = ENOTDIR;
			goto fail;
		}
		bmp = malloc(bmvap->va_datalen, M_TEMP, M_WAITOK);
		error = ntfs_readattr(ntmp, ip, NTFS_A_INDXBITMAP, "$I30", 0,
				       bmvap->va_datalen, bmp, NULL);
		if (error)
			goto fail;

		error = ntfs_ntvattrget(ntmp, ip, NTFS_A_INDX, "$I30",
					0, &iavap);
		if (error) {
			error = ENOTDIR;
			goto fail;
		}
		cpbl = ntfs_btocn(blsize + ntfs_cntob(1) - 1);
		dprintf(("ntfs_ntreaddir: indexalloc: %d, cpbl: %d\n",
			 iavap->va_datalen, cpbl));
	} else {
		dprintf(("ntfs_ntreadidir: w/o BitMap and IndexAllocation\n"));
		iavap = bmvap = NULL;
		bmp = NULL;
	}

	/* Try use previous values */
	if ((fp->f_lastdnum < num) && (fp->f_lastdnum != 0)) {
		attrnum = fp->f_lastdattr;
		aoff = fp->f_lastdoff;
		blnum = fp->f_lastdblnum;
		cnum = fp->f_lastdnum;
	} else {
		attrnum = NTFS_A_INDXROOT;
		aoff = sizeof(struct attr_indexroot);
		blnum = 0;
		cnum = 0;
	}

	do {
		dprintf(("ntfs_ntreaddir: scan: 0x%x, %d, %d, %d, %d\n",
			 attrnum, (u_int32_t) blnum, cnum, num, aoff));
		rdsize = (attrnum == NTFS_A_INDXROOT) ? vap->va_datalen : blsize;
		error = ntfs_readattr(ntmp, ip, attrnum, "$I30",
				ntfs_cntob(blnum * cpbl), rdsize, rdbuf, NULL);
		if (error)
			goto fail;

		if (attrnum == NTFS_A_INDX) {
			error = ntfs_procfixups(ntmp, NTFS_INDXMAGIC,
						rdbuf, rdsize);
			if (error)
				goto fail;
		}
		if (aoff == 0)
			aoff = (attrnum == NTFS_A_INDX) ?
				(0x18 + ((struct attr_indexalloc *) rdbuf)->ia_hdrsize) :
				sizeof(struct attr_indexroot);

		iep = (struct attr_indexentry *) (rdbuf + aoff);
		for (; !(iep->ie_flag & NTFS_IEFLAG_LAST) && (rdsize > aoff);
			aoff += iep->reclen,
			iep = (struct attr_indexentry *) (rdbuf + aoff))
		{
			if (!ntfs_isnamepermitted(ntmp, iep)) continue;

			if (cnum >= num) {
				fp->f_lastdnum = cnum;
				fp->f_lastdoff = aoff;
				fp->f_lastdblnum = blnum;
				fp->f_lastdattr = attrnum;

				*riepp = iep;

				error = 0;
				goto fail;
			}
			cnum++;
		}

		if (iavap) {
			if (attrnum == NTFS_A_INDXROOT)
				blnum = 0;
			else
				blnum++;

			while (ntfs_cntob(blnum * cpbl) < iavap->va_datalen) {
				if (bmp[blnum >> 3] & (1 << (blnum & 7)))
					break;
				blnum++;
			}

			attrnum = NTFS_A_INDX;
			aoff = 0;
			if (ntfs_cntob(blnum * cpbl) >= iavap->va_datalen)
				break;
			dprintf(("ntfs_ntreaddir: blnum: %d\n", (u_int32_t) blnum));
		}
	} while (iavap);

	*riepp = NULL;
	fp->f_lastdnum = 0;

fail:
	if (vap)
		ntfs_ntvattrrele(vap);
	if (bmvap)
		ntfs_ntvattrrele(bmvap);
	if (iavap)
		ntfs_ntvattrrele(iavap);
	if (bmp)
		free(bmp, M_TEMP);
	ntfs_ntput(ip, p);

	return (error);
}

/*
 * Convert NTFS times that are in 100 ns units and begins from
 * 1601 Jan 1 into unix times.
 */
struct timespec
ntfs_nttimetounix(
		  u_int64_t nt)
{
	struct timespec t;

	/* WindowNT times are in 100 ns and from 1601 Jan 1 */
	t.tv_nsec = (nt % (1000 * 1000 * 10)) * 100;
	t.tv_sec = nt / (1000 * 1000 * 10) -
		369LL * 365LL * 24LL * 60LL * 60LL -
		89LL * 1LL * 24LL * 60LL * 60LL;
	return (t);
}

/*
 * Get file sizes from corresponding attribute. 
 * 
 * ntnode under fnode should be locked.
 */
int
ntfs_filesize(
	      struct ntfsmount * ntmp,
	      struct fnode * fp,
	      u_int64_t * size,
	      u_int64_t * bytes)
{
	struct ntvattr *vap;
	struct ntnode *ip = FTONT(fp);
	u_int64_t       sz, bn;
	int             error;

	dprintf(("ntfs_filesize: ino: %d\n", ip->i_number));

	error = ntfs_ntvattrget(ntmp, ip,
		fp->f_attrtype, fp->f_attrname, 0, &vap);
	if (error)
		return (error);

	bn = vap->va_allocated;
	sz = vap->va_datalen;

	dprintf(("ntfs_filesize: %d bytes (%d bytes allocated)\n",
		(u_int32_t) sz, (u_int32_t) bn));

	if (size)
		*size = sz;
	if (bytes)
		*bytes = bn;

	ntfs_ntvattrrele(vap);

	return (0);
}

/*
 * This is one of the write routines.
 */
int
ntfs_writeattr_plain(
	struct ntfsmount * ntmp,
	struct ntnode * ip,
	u_int32_t attrnum,	
	char *attrname,
	off_t roff,
	size_t rsize,
	void *rdata,
	size_t * initp,
	struct uio *uio)
{
	size_t          init;
	int             error = 0;
	off_t           off = roff, left = rsize, towrite;
	caddr_t         data = rdata;
	struct ntvattr *vap;
	*initp = 0;

	while (left) {
		error = ntfs_ntvattrget(ntmp, ip, attrnum, attrname,
					ntfs_btocn(off), &vap);
		if (error)
			return (error);
		towrite = MIN(left, ntfs_cntob(vap->va_vcnend + 1) - off);
		ddprintf(("ntfs_writeattr_plain: o: %d, s: %d (%d - %d)\n",
			 (u_int32_t) off, (u_int32_t) towrite,
			 (u_int32_t) vap->va_vcnstart,
			 (u_int32_t) vap->va_vcnend));
		error = ntfs_writentvattr_plain(ntmp, ip, vap,
					 off - ntfs_cntob(vap->va_vcnstart),
					 towrite, data, &init, uio);
		if (error) {
			dprintf(("ntfs_writeattr_plain: " \
			       "ntfs_writentvattr_plain failed: o: %d, s: %d\n",
			       (u_int32_t) off, (u_int32_t) towrite));
			dprintf(("ntfs_writeattr_plain: attrib: %d - %d\n",
			       (u_int32_t) vap->va_vcnstart, 
			       (u_int32_t) vap->va_vcnend));
			ntfs_ntvattrrele(vap);
			break;
		}
		ntfs_ntvattrrele(vap);
		left -= towrite;
		off += towrite;
		data = data + towrite;
		*initp += init;
	}

	return (error);
}

/*
 * This is one of the write routines.
 *
 * ntnode should be locked.
 */
int
ntfs_writentvattr_plain(
	struct ntfsmount * ntmp,
	struct ntnode * ip,
	struct ntvattr * vap,
	off_t roff,
	size_t rsize,
	void *rdata,
	size_t * initp,
	struct uio *uio)
{
	int             error = 0;
	int             off;
	int             cnt;
	cn_t            ccn, ccl, cn, left, cl;
	caddr_t         data = rdata;
	struct buf     *bp;
	size_t          tocopy;

	*initp = 0;

	if ((vap->va_flag & NTFS_AF_INRUN) == 0) {
		dprintf(("ntfs_writevattr_plain: CAN'T WRITE RES. ATTRIBUTE\n"));
		return ENOTTY;
	}

	ddprintf(("ntfs_writentvattr_plain: data in run: %lu chains\n",
		 vap->va_vruncnt));

	off = roff;
	left = rsize;
	ccl = 0;
	ccn = 0;
	cnt = 0;
	for (; left && (cnt < vap->va_vruncnt); cnt++) {
		ccn = vap->va_vruncn[cnt];
		ccl = vap->va_vruncl[cnt];

		ddprintf(("ntfs_writentvattr_plain: " \
			 "left %d, cn: 0x%x, cl: %d, off: %d\n", \
			 (u_int32_t) left, (u_int32_t) ccn, \
			 (u_int32_t) ccl, (u_int32_t) off));

		if (ntfs_cntob(ccl) < off) {
			off -= ntfs_cntob(ccl);
			cnt++;
			continue;
		}
		if (!ccn && ip->i_number != NTFS_BOOTINO)
			continue; /* XXX */

		ccl -= ntfs_btocn(off);
		cn = ccn + ntfs_btocn(off);
		off = ntfs_btocnoff(off);

		while (left && ccl) {
			/*
			 * Always read and write single clusters at a time -
			 * we need to avoid requesting differently-sized
			 * blocks at the same disk offsets to avoid
			 * confusing the buffer cache.
			 */
			tocopy = MIN(left, ntfs_cntob(1) - off);
			cl = ntfs_btocl(tocopy + off);
			KASSERT(cl == 1 && tocopy <= ntfs_cntob(1));
			ddprintf(("ntfs_writentvattr_plain: write: " \
				"cn: 0x%x cl: %d, off: %d len: %d, left: %d\n",
				(u_int32_t) cn, (u_int32_t) cl, 
				(u_int32_t) off, (u_int32_t) tocopy, 
				(u_int32_t) left));
			if ((off == 0) && (tocopy == ntfs_cntob(cl)))
			{
				bp = getblk(ntmp->ntm_devvp, ntfs_cntobn(cn),
					    ntfs_cntob(cl), 0, 0);
				clrbuf(bp);
			} else {
				error = bread(ntmp->ntm_devvp, ntfs_cntobn(cn),
					      ntfs_cntob(cl), &bp);
				if (error) {
					brelse(bp);
					return (error);
				}
			}
			if (uio) {
				error = uiomove(bp->b_data + off, tocopy, uio);
				if (error != 0)
					break;
			} else
				memcpy(bp->b_data + off, data, tocopy);
			bawrite(bp);
			data = data + tocopy;
			*initp += tocopy;
			off = 0;
			left -= tocopy;
			cn += cl;
			ccl -= cl;
		}
	}

	if (left && error == 0) {
		printf("ntfs_writentvattr_plain: POSSIBLE RUN ERROR\n");
		error = EINVAL;
	}

	return (error);
}

/*
 * This is one of the read routines.
 *
 * ntnode should be locked.
 */
int
ntfs_readntvattr_plain(
	struct ntfsmount * ntmp,
	struct ntnode * ip,
	struct ntvattr * vap,
	off_t roff,
	size_t rsize,
	void *rdata,
	size_t * initp,
	struct uio *uio)
{
	int             error = 0;
	int             off;

	*initp = 0;
	if (vap->va_flag & NTFS_AF_INRUN) {
		int             cnt;
		cn_t            ccn, ccl, cn, left, cl;
		caddr_t         data = rdata;
		struct buf     *bp;
		size_t          tocopy;

		ddprintf(("ntfs_readntvattr_plain: data in run: %lu chains\n",
			 vap->va_vruncnt));

		off = roff;
		left = rsize;
		ccl = 0;
		ccn = 0;
		cnt = 0;
		while (left && (cnt < vap->va_vruncnt)) {
			ccn = vap->va_vruncn[cnt];
			ccl = vap->va_vruncl[cnt];

			ddprintf(("ntfs_readntvattr_plain: " \
				 "left %d, cn: 0x%x, cl: %d, off: %d\n", \
				 (u_int32_t) left, (u_int32_t) ccn, \
				 (u_int32_t) ccl, (u_int32_t) off));

			if (ntfs_cntob(ccl) < off) {
				off -= ntfs_cntob(ccl);
				cnt++;
				continue;
			}
			if (ccn || ip->i_number == NTFS_BOOTINO) {
				ccl -= ntfs_btocn(off);
				cn = ccn + ntfs_btocn(off);
				off = ntfs_btocnoff(off);

				while (left && ccl) {
					/*
					 * Always read single clusters at a
					 * time - we need to avoid reading
					 * differently-sized blocks at the
					 * same disk offsets to avoid
					 * confusing the buffer cache.
					 */
					tocopy = MIN(left,
					    ntfs_cntob(1) - off);
					cl = ntfs_btocl(tocopy + off);
					KASSERT(cl == 1 &&
					    tocopy <= ntfs_cntob(1));

					ddprintf(("ntfs_readntvattr_plain: " \
						"read: cn: 0x%x cl: %d, " \
						"off: %d len: %d, left: %d\n",
						(u_int32_t) cn, 
						(u_int32_t) cl, 
						(u_int32_t) off, 
						(u_int32_t) tocopy, 
						(u_int32_t) left));
					error = bread(ntmp->ntm_devvp,
						      ntfs_cntobn(cn),
						      ntfs_cntob(cl),
						      &bp);
					if (error) {
						brelse(bp);
						return (error);
					}
					if (uio) {
						error = uiomove(bp->b_data + off,
							tocopy, uio);
						if (error != 0)
							break;
					} else {
						memcpy(data, bp->b_data + off,
							tocopy);
					}
					brelse(bp);
					data = data + tocopy;
					*initp += tocopy;
					off = 0;
					left -= tocopy;
					cn += cl;
					ccl -= cl;
				}
			} else {
				tocopy = MIN(left, ntfs_cntob(ccl) - off);
				ddprintf(("ntfs_readntvattr_plain: "
					"hole: ccn: 0x%x ccl: %d, off: %d, " \
					" len: %d, left: %d\n", 
					(u_int32_t) ccn, (u_int32_t) ccl, 
					(u_int32_t) off, (u_int32_t) tocopy, 
					(u_int32_t) left));
				left -= tocopy;
				off = 0;
				if (uio) {
					size_t remains = tocopy;
					for(; remains; remains--) {
						error = uiomove("", 1, uio);
						if (error != 0)
							break;
					}
				} else 
					bzero(data, tocopy);
				data = data + tocopy;
			}
			cnt++;
			if (error != 0)
				break;
		}
		if (left && error == 0) {
			printf("ntfs_readntvattr_plain: POSSIBLE RUN ERROR\n");
			error = E2BIG;
		}
	} else {
		ddprintf(("ntfs_readnvattr_plain: data is in mft record\n"));
		if (uio) 
			error = uiomove(vap->va_datap + roff, rsize, uio);
		else
			memcpy(rdata, vap->va_datap + roff, rsize);
		*initp += rsize;
	}

	return (error);
}

/*
 * This is one of read routines.
 */
int
ntfs_readattr_plain(
	struct ntfsmount * ntmp,
	struct ntnode * ip,
	u_int32_t attrnum,	
	char *attrname,
	off_t roff,
	size_t rsize,
	void *rdata,
	size_t * initp,
	struct uio *uio)
{
	size_t          init;
	int             error = 0;
	off_t           off = roff, left = rsize, toread;
	caddr_t         data = rdata;
	struct ntvattr *vap;
	*initp = 0;

	while (left) {
		error = ntfs_ntvattrget(ntmp, ip, attrnum, attrname,
					ntfs_btocn(off), &vap);
		if (error)
			return (error);
		toread = MIN(left, ntfs_cntob(vap->va_vcnend + 1) - off);
		ddprintf(("ntfs_readattr_plain: o: %d, s: %d (%d - %d)\n",
			 (u_int32_t) off, (u_int32_t) toread,
			 (u_int32_t) vap->va_vcnstart,
			 (u_int32_t) vap->va_vcnend));
		error = ntfs_readntvattr_plain(ntmp, ip, vap,
					 off - ntfs_cntob(vap->va_vcnstart),
					 toread, data, &init, uio);
		if (error) {
			printf("ntfs_readattr_plain: " \
			       "ntfs_readntvattr_plain failed: o: %d, s: %d\n",
			       (u_int32_t) off, (u_int32_t) toread);
			printf("ntfs_readattr_plain: attrib: %d - %d\n",
			       (u_int32_t) vap->va_vcnstart, 
			       (u_int32_t) vap->va_vcnend);
			ntfs_ntvattrrele(vap);
			break;
		}
		ntfs_ntvattrrele(vap);
		left -= toread;
		off += toread;
		data = data + toread;
		*initp += init;
	}

	return (error);
}

/*
 * This is one of read routines.
 */
int
ntfs_readattr(
	struct ntfsmount * ntmp,
	struct ntnode * ip,
	u_int32_t attrnum,
	char *attrname,
	off_t roff,
	size_t rsize,
	void *rdata,
	struct uio *uio)
{
	int             error = 0;
	struct ntvattr *vap;
	size_t          init;

	ddprintf(("ntfs_readattr: reading %d: 0x%x, from %d size %d bytes\n",
	       ip->i_number, attrnum, (u_int32_t) roff, (u_int32_t) rsize));

	error = ntfs_ntvattrget(ntmp, ip, attrnum, attrname, 0, &vap);
	if (error)
		return (error);

	if ((roff > vap->va_datalen) ||
	    (roff + rsize > vap->va_datalen)) {
		printf("ntfs_readattr: offset too big: %ld (%ld) > %ld\n",
			(long int) roff, (long int) roff + rsize,
			(long int) vap->va_datalen);
		ntfs_ntvattrrele(vap);
		return (E2BIG);
	}
	if (vap->va_compression && vap->va_compressalg) {
		u_int8_t       *cup;
		u_int8_t       *uup;
		off_t           off = roff, left = rsize, tocopy;
		caddr_t         data = rdata;
		cn_t            cn;

		ddprintf(("ntfs_ntreadattr: compression: %d\n",
			 vap->va_compressalg));

		cup = malloc(ntfs_cntob(NTFS_COMPUNIT_CL), M_NTFSDECOMP,
		    M_WAITOK);
		uup = malloc(ntfs_cntob(NTFS_COMPUNIT_CL), M_NTFSDECOMP,
		    M_WAITOK);

		cn = (ntfs_btocn(roff)) & (~(NTFS_COMPUNIT_CL - 1));
		off = roff - ntfs_cntob(cn);

		while (left) {
			error = ntfs_readattr_plain(ntmp, ip, attrnum,
						  attrname, ntfs_cntob(cn),
					          ntfs_cntob(NTFS_COMPUNIT_CL),
						  cup, &init, NULL);
			if (error)
				break;

			tocopy = MIN(left, ntfs_cntob(NTFS_COMPUNIT_CL) - off);

			if (init == ntfs_cntob(NTFS_COMPUNIT_CL)) {
				if (uio)
					error = uiomove(cup + off, tocopy, uio);
				else
					memcpy(data, cup + off, tocopy);
			} else if (init == 0) {
				if (uio) {
					size_t remains = tocopy;
					for(; remains; remains--) {
						error = uiomove("", 1, uio);
						if (error != 0)
							break;
					}
				}
				else
					bzero(data, tocopy);
			} else {
				error = ntfs_uncompunit(ntmp, uup, cup);
				if (error)
					break;
				if (uio)
					error = uiomove(uup + off, tocopy, uio);
				else
					memcpy(data, uup + off, tocopy);
			}
			if (error)
				break;

			left -= tocopy;
			data = data + tocopy;
			off += tocopy - ntfs_cntob(NTFS_COMPUNIT_CL);
			cn += NTFS_COMPUNIT_CL;
		}

		free(uup, M_NTFSDECOMP);
		free(cup, M_NTFSDECOMP);
	} else
		error = ntfs_readattr_plain(ntmp, ip, attrnum, attrname,
					     roff, rsize, rdata, &init, uio);
	ntfs_ntvattrrele(vap);
	return (error);
}

#if UNUSED_CODE
int
ntfs_parserun(
	      cn_t * cn,
	      cn_t * cl,
	      u_int8_t * run,
	      u_long len,
	      u_long *off)
{
	u_int8_t        sz;
	int             i;

	if (NULL == run) {
		printf("ntfs_parsetun: run == NULL\n");
		return (EINVAL);
	}
	sz = run[(*off)++];
	if (0 == sz) {
		printf("ntfs_parserun: trying to go out of run\n");
		return (E2BIG);
	}
	*cl = 0;
	if ((sz & 0xF) > 8 || (*off) + (sz & 0xF) > len) {
		printf("ntfs_parserun: " \
		       "bad run: length too big: sz: 0x%02x (%ld < %ld + sz)\n",
		       sz, len, *off);
		return (EINVAL);
	}
	for (i = 0; i < (sz & 0xF); i++)
		*cl += (u_int32_t) run[(*off)++] << (i << 3);

	sz >>= 4;
	if ((sz & 0xF) > 8 || (*off) + (sz & 0xF) > len) {
		printf("ntfs_parserun: " \
		       "bad run: length too big: sz: 0x%02x (%ld < %ld + sz)\n",
		       sz, len, *off);
		return (EINVAL);
	}
	for (i = 0; i < (sz & 0xF); i++)
		*cn += (u_int32_t) run[(*off)++] << (i << 3);

	return (0);
}
#endif

/*
 * Process fixup routine on given buffer.
 */
int
ntfs_procfixups(
		struct ntfsmount * ntmp,
		u_int32_t magic,
		caddr_t buf,
		size_t len)
{
	struct fixuphdr *fhp = (struct fixuphdr *) buf;
	int             i;
	u_int16_t       fixup;
	u_int16_t      *fxp;
	u_int16_t      *cfxp;

	if (fhp->fh_magic != magic) {
		printf("ntfs_procfixups: magic doesn't match: %08x != %08x\n",
		       fhp->fh_magic, magic);
		return (EINVAL);
	}
	if ((fhp->fh_fnum - 1) * ntmp->ntm_bps != len) {
		printf("ntfs_procfixups: " \
		       "bad fixups number: %d for %ld bytes block\n", 
		       fhp->fh_fnum, (long)len);	/* XXX printf kludge */
		return (EINVAL);
	}
	if (fhp->fh_foff >= ntmp->ntm_spc * ntmp->ntm_mftrecsz * ntmp->ntm_bps) {
		printf("ntfs_procfixups: invalid offset: %x", fhp->fh_foff);
		return (EINVAL);
	}
	fxp = (u_int16_t *) (buf + fhp->fh_foff);
	cfxp = (u_int16_t *) (buf + ntmp->ntm_bps - 2);
	fixup = *fxp++;
	for (i = 1; i < fhp->fh_fnum; i++, fxp++) {
		if (*cfxp != fixup) {
			printf("ntfs_procfixups: fixup %d doesn't match\n", i);
			return (EINVAL);
		}
		*cfxp = *fxp;
		cfxp = (u_int16_t *)((caddr_t)cfxp + ntmp->ntm_bps);
	}
	return (0);
}

#if UNUSED_CODE
int
ntfs_runtocn(
	     cn_t * cn,	
	     struct ntfsmount * ntmp,
	     u_int8_t * run,
	     u_long len,
	     cn_t vcn)
{
	cn_t            ccn = 0;
	cn_t            ccl = 0;
	u_long          off = 0;
	int             error = 0;

#if NTFS_DEBUG
	int             i;
	printf("ntfs_runtocn: run: %p, %ld bytes, vcn:%ld\n",
		run, len, (u_long) vcn);
	printf("ntfs_runtocn: run: ");
	for (i = 0; i < len; i++)
		printf("0x%02x ", run[i]);
	printf("\n");
#endif

	if (NULL == run) {
		printf("ntfs_runtocn: run == NULL\n");
		return (EINVAL);
	}
	do {
		if (run[off] == 0) {
			printf("ntfs_runtocn: vcn too big\n");
			return (E2BIG);
		}
		vcn -= ccl;
		error = ntfs_parserun(&ccn, &ccl, run, len, &off);
		if (error) {
			printf("ntfs_runtocn: ntfs_parserun failed\n");
			return (error);
		}
	} while (ccl <= vcn);
	*cn = ccn + vcn;
	return (0);
}
#endif

/*
 * this initializes toupper table & dependant variables to be ready for
 * later work
 */
void
ntfs_toupper_init()
{
	ntfs_toupper_tab = (wchar *) NULL;
	ntfs_toupper_usecount = 0;
}

/*
 * if the ntfs_toupper_tab[] is filled already, just raise use count;
 * otherwise read the data from the filesystem we are currently mounting
 */
int
ntfs_toupper_use(mp, ntmp, p)
	struct mount *mp;
	struct ntfsmount *ntmp;
	struct proc *p;
{
	int error = 0;
	struct vnode *vp;

	/* get exclusive access */
	rw_enter_write(&ntfs_toupper_lock);

	/* only read the translation data from a file if it hasn't been
	 * read already */
	if (ntfs_toupper_tab)
		goto out;

	/*
	 * Read in Unicode lowercase -> uppercase translation file.
	 * XXX for now, just the first 256 entries are used anyway,
	 * so don't bother reading more
	 */
	ntfs_toupper_tab = malloc(256 * 256 * sizeof(wchar), M_NTFSRDATA,
	    M_WAITOK);

	if ((error = VFS_VGET(mp, NTFS_UPCASEINO, &vp)))
		goto out;
	error = ntfs_readattr(ntmp, VTONT(vp), NTFS_A_DATA, NULL,
			0, 256*256*sizeof(wchar), (char *) ntfs_toupper_tab,
			NULL);
	vput(vp);

    out:
	ntfs_toupper_usecount++;
	rw_exit_write(&ntfs_toupper_lock);
	return (error);
}

/*
 * lower the use count and if it reaches zero, free the memory
 * tied by toupper table
 */
void
ntfs_toupper_unuse(p)
	struct proc *p;
{
	/* get exclusive access */
	rw_enter_write(&ntfs_toupper_lock);

	ntfs_toupper_usecount--;
	if (ntfs_toupper_usecount == 0) {
		free(ntfs_toupper_tab, M_NTFSRDATA);
		ntfs_toupper_tab = NULL;
	}
#ifdef DIAGNOSTIC
	else if (ntfs_toupper_usecount < 0) {
		panic("ntfs_toupper_unuse(): use count negative: %d",
			ntfs_toupper_usecount);
	}
#endif
	
	/* release the lock */
	rw_exit_write(&ntfs_toupper_lock);
}
