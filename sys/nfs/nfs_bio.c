/*	$OpenBSD: nfs_bio.c,v 1.31 2001/12/10 02:19:34 art Exp $	*/
/*	$NetBSD: nfs_bio.c,v 1.25.4.2 1996/07/08 20:47:04 jtc Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)nfs_bio.c	8.9 (Berkeley) 3/30/95
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/namei.h>
#include <sys/pool.h>

#include <uvm/uvm.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsnode.h>
#include <nfs/nfs_var.h>

extern struct proc *nfs_iodwant[NFS_MAXASYNCDAEMON];
extern int nfs_numasync;
struct nfsstats nfsstats;

/*
 * Vnode op for read using bio
 * Any similarity to readip() is purely coincidental
 */
int
nfs_bioread(vp, uio, ioflag, cred)
	struct vnode *vp;
	struct uio *uio;
	int ioflag;
	struct ucred *cred;
{
	struct nfsnode *np = VTONFS(vp);
	int biosize;
	struct buf *bp = NULL;
	struct vattr vattr;
	struct proc *p;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	caddr_t baddr;
	int got_buf = 0, error = 0, n = 0, on = 0;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_READ)
		panic("nfs_read mode");
#endif
	if (uio->uio_resid == 0)
		return (0);
	if (uio->uio_offset < 0)
		return (EINVAL);
	p = uio->uio_procp;
	if ((nmp->nm_flag & (NFSMNT_NFSV3 | NFSMNT_GOTFSINFO)) == NFSMNT_NFSV3)
		(void)nfs_fsinfo(nmp, vp, cred, p);
	biosize = nmp->nm_rsize;
	/*
	 * For nfs, cache consistency can only be maintained approximately.
	 * Although RFC1094 does not specify the criteria, the following is
	 * believed to be compatible with the reference port.
	 * For nfs:
	 * If the file's modify time on the server has changed since the
	 * last read rpc or you have written to the file,
	 * you may have lost data cache consistency with the
	 * server, so flush all of the file's data out of the cache.
	 * Then force a getattr rpc to ensure that you have up to date
	 * attributes.
	 * NB: This implies that cache data can be read when up to
	 * NFS_ATTRTIMEO seconds out of date. If you find that you need current
	 * attributes this could be forced by setting n_attrstamp to 0 before
	 * the VOP_GETATTR() call.
	 */
	/* 
	 * There is no way to modify a symbolic link via NFS or via
	 * VFS, so we don't check if the link was modified 
	 */
	if (vp->v_type != VLNK) {
		if (np->n_flag & NMODIFIED) {
			np->n_attrstamp = 0;
			error = VOP_GETATTR(vp, &vattr, cred, p);
			if (error)
				return (error);
			np->n_mtime = vattr.va_mtime.tv_sec;
		} else {
			error = VOP_GETATTR(vp, &vattr, cred, p);
			if (error)
				return (error);
			if (np->n_mtime != vattr.va_mtime.tv_sec) {
				error = nfs_vinvalbuf(vp, V_SAVE, cred, p, 1);
				if (error)
					return (error);
				np->n_mtime = vattr.va_mtime.tv_sec;
			}
		}
	}

	/*
	 * update the cache read creds for this vnode
	 */
	if (np->n_rcred)
		crfree(np->n_rcred);
	np->n_rcred = cred;
	crhold(cred);

	do {
	    if ((vp->v_flag & VROOT) && vp->v_type == VLNK) {
		    return (nfs_readlinkrpc(vp, uio, cred));
	    }
	    baddr = (caddr_t)0;
	    switch (vp->v_type) {
	    case VREG:
		nfsstats.biocache_reads++;
		error = 0;
		while (uio->uio_resid > 0) {
			void *win;
			vsize_t bytelen = MIN(np->n_size - uio->uio_offset,
					      uio->uio_resid);

			if (bytelen == 0)
				break;
			win = ubc_alloc(&vp->v_uobj, uio->uio_offset,
					&bytelen, UBC_READ);
			error = uiomove(win, bytelen, uio);
			ubc_release(win, 0);
			if (error) {
				break;
			}
		}
		n = 0;
		break;

	    case VLNK:
		nfsstats.biocache_readlinks++;
		bp = nfs_getcacheblk(vp, (daddr_t)0, NFS_MAXPATHLEN, p);
		if (!bp)
			return (EINTR);
		if ((bp->b_flags & B_DONE) == 0) {
			bp->b_flags |= B_READ;
			error = nfs_doio(bp, p);
			if (error) {
				brelse(bp);
				return (error);
			}
		}
		n = MIN(uio->uio_resid, NFS_MAXPATHLEN - bp->b_resid);
		got_buf = 1;
		on = 0;
		break;
	    default:
		printf(" nfsbioread: type %x unexpected\n",vp->v_type);
		break;
	    }

	    if (n > 0) {
		if (!baddr)
			baddr = bp->b_data;
		error = uiomove(baddr + on, (int)n, uio);
	    }
	    switch (vp->v_type) {
	    case VREG:
		break;
	    case VLNK:
		n = 0;
		break;
	    default:
		printf(" nfsbioread: type %x unexpected\n",vp->v_type);
	    }
	    if (got_buf)
		brelse(bp);
	} while (error == 0 && uio->uio_resid > 0 && n > 0);
	return (error);
}

/*
 * Vnode op for write using bio
 */
int
nfs_write(v)
	void *v;
{
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap = v;
	int biosize;
	struct uio *uio = ap->a_uio;
	struct proc *p = uio->uio_procp;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct ucred *cred = ap->a_cred;
	int ioflag = ap->a_ioflag;
	struct vattr vattr;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0;
	int rv;

#ifdef DIAGNOSTIC
	if (uio->uio_rw != UIO_WRITE)
		panic("nfs_write mode");
	if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
		panic("nfs_write proc");
#endif
	if (vp->v_type != VREG)
		return (EIO);
	if (np->n_flag & NWRITEERR) {
		np->n_flag &= ~NWRITEERR;
		return (np->n_error);
	}
	if ((nmp->nm_flag & (NFSMNT_NFSV3 | NFSMNT_GOTFSINFO)) == NFSMNT_NFSV3)
		(void)nfs_fsinfo(nmp, vp, cred, p);
	if (ioflag & (IO_APPEND | IO_SYNC)) {
		if (np->n_flag & NMODIFIED) {
			np->n_attrstamp = 0;
			error = nfs_vinvalbuf(vp, V_SAVE, cred, p, 1);
			if (error)
				return (error);
		}
		if (ioflag & IO_APPEND) {
			np->n_attrstamp = 0;
			error = VOP_GETATTR(vp, &vattr, cred, p);
			if (error)
				return (error);
			uio->uio_offset = np->n_size;
		}
	}
	if (uio->uio_offset < 0)
		return (EINVAL);
	if (uio->uio_resid == 0)
		return (0);
	/*
	 * Maybe this should be above the vnode op call, but so long as
	 * file servers have no limits, i don't think it matters
	 */
	if (p && uio->uio_offset + uio->uio_resid >
	      p->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
		psignal(p, SIGXFSZ);
		return (EFBIG);
	}

	/*
	 * update the cache write creds for this node.
	 */
	if (np->n_wcred)
		crfree(np->n_wcred);
	np->n_wcred = cred;
	crhold(cred);

	/*
	 * I use nm_rsize, not nm_wsize so that all buffer cache blocks
	 * will be the same size within a filesystem. nfs_writerpc will
	 * still use nm_wsize when sizing the rpc's.
	 */
	biosize = nmp->nm_rsize;
	do {
		void *win;
		voff_t oldoff = uio->uio_offset;
		vsize_t bytelen = uio->uio_resid;

		nfsstats.biocache_writes++;
		np->n_flag |= NMODIFIED;
		if (np->n_size < uio->uio_offset + bytelen) {
			np->n_size = uio->uio_offset + bytelen;
			uvm_vnp_setsize(vp, np->n_size);
		}
		win = ubc_alloc(&vp->v_uobj, uio->uio_offset, &bytelen,
				UBC_WRITE);
		error = uiomove(win, bytelen, uio);
		ubc_release(win, 0);
		rv = 1;
		if ((ioflag & IO_SYNC)) {
			simple_lock(&vp->v_uobj.vmobjlock);
			rv = vp->v_uobj.pgops->pgo_flush(
			    &vp->v_uobj,
			    oldoff & ~(nmp->nm_wsize - 1),
			    uio->uio_offset & ~(nmp->nm_wsize - 1),
			    PGO_CLEANIT|PGO_SYNCIO);
			simple_unlock(&vp->v_uobj.vmobjlock);
		} else if ((oldoff & ~(nmp->nm_wsize - 1)) !=
		    (uio->uio_offset & ~(nmp->nm_wsize - 1))) {
			simple_lock(&vp->v_uobj.vmobjlock);
			rv = vp->v_uobj.pgops->pgo_flush(
			    &vp->v_uobj,
			    oldoff & ~(nmp->nm_wsize - 1),
			    uio->uio_offset & ~(nmp->nm_wsize - 1),
			    PGO_CLEANIT|PGO_WEAK);
			simple_unlock(&vp->v_uobj.vmobjlock);
		}
		if (!rv) {
			error = EIO;
		}
		if (error) {
			break;
		}
	} while (uio->uio_resid > 0);
	return (error);
}

/*
 * Get an nfs cache block.
 * Allocate a new one if the block isn't currently in the cache
 * and return the block marked busy. If the calling process is
 * interrupted by a signal for an interruptible mount point, return
 * NULL.
 */
struct buf *
nfs_getcacheblk(vp, bn, size, p)
	struct vnode *vp;
	daddr_t bn;
	int size;
	struct proc *p;
{
	register struct buf *bp;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);

	if (nmp->nm_flag & NFSMNT_INT) {
		bp = getblk(vp, bn, size, PCATCH, 0);
		while (bp == NULL) {
			if (nfs_sigintr(nmp, NULL, p))
				return (NULL);
			bp = getblk(vp, bn, size, 0, 2 * hz);
		}
	} else
		bp = getblk(vp, bn, size, 0, 0);
	return (bp);
}

/*
 * Flush and invalidate all dirty buffers. If another process is already
 * doing the flush, just wait for completion.
 */
int
nfs_vinvalbuf(vp, flags, cred, p, intrflg)
	struct vnode *vp;
	int flags;
	struct ucred *cred;
	struct proc *p;
	int intrflg;
{
	register struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	int error = 0, slpflag, slptimeo;

	if ((nmp->nm_flag & NFSMNT_INT) == 0)
		intrflg = 0;
	if (intrflg) {
		slpflag = PCATCH;
		slptimeo = 2 * hz;
	} else {
		slpflag = 0;
		slptimeo = 0;
	}
	/*
	 * First wait for any other process doing a flush to complete.
	 */
	while (np->n_flag & NFLUSHINPROG) {
		np->n_flag |= NFLUSHWANT;
		error = tsleep((caddr_t)&np->n_flag, PRIBIO + 2, "nfsvinval",
			slptimeo);
		if (error && intrflg && nfs_sigintr(nmp, NULL, p))
			return (EINTR);
	}

	/*
	 * Now, flush as required.
	 */
	np->n_flag |= NFLUSHINPROG;
	error = vinvalbuf(vp, flags, cred, p, slpflag, 0);
	while (error) {
		if (intrflg && nfs_sigintr(nmp, NULL, p)) {
			np->n_flag &= ~NFLUSHINPROG;
			if (np->n_flag & NFLUSHWANT) {
				np->n_flag &= ~NFLUSHWANT;
				wakeup((caddr_t)&np->n_flag);
			}
			return (EINTR);
		}
		error = vinvalbuf(vp, flags, cred, p, 0, slptimeo);
	}
	np->n_flag &= ~(NMODIFIED | NFLUSHINPROG);
	if (np->n_flag & NFLUSHWANT) {
		np->n_flag &= ~NFLUSHWANT;
		wakeup((caddr_t)&np->n_flag);
	}
	return (0);
}

/*
 * Initiate asynchronous I/O. Return an error if no nfsiods are available.
 * This is mainly to avoid queueing async I/O requests when the nfsiods
 * are all hung on a dead server.
 */
int
nfs_asyncio(bp)
	struct buf *bp;
{
	int i;

	if (nfs_numasync == 0)
		return (EIO);
	for (i = 0; i < NFS_MAXASYNCDAEMON; i++) {
	    if (nfs_iodwant[i]) {
		TAILQ_INSERT_TAIL(&nfs_bufq, bp, b_freelist);
		nfs_iodwant[i] = NULL;
		wakeup((caddr_t)&nfs_iodwant[i]);
		return (0);
	    }
	}

	return (EIO);
}

/*
 * Do an I/O operation to/from a cache block. This may be called
 * synchronously or from an nfsiod.
 */
int
nfs_doio(bp, p)
	struct buf *bp;
	struct proc *p;
{
	register struct uio *uiop;
	register struct vnode *vp;
	struct nfsnode *np;
	struct nfsmount *nmp;
	int error = 0, diff, len, iomode, must_commit = 0;
	struct uio uio;
	struct iovec io;

	vp = bp->b_vp;
	np = VTONFS(vp);
	nmp = VFSTONFS(vp->v_mount);
	uiop = &uio;
	uiop->uio_iov = &io;
	uiop->uio_iovcnt = 1;
	uiop->uio_segflg = UIO_SYSSPACE;
	uiop->uio_procp = p;

	/*
	 * Historically, paging was done with physio, but no more...
	 */
	if (bp->b_flags & B_PHYS) {
	    /*
	     * ...though reading /dev/drum still gets us here.
	     */
	    io.iov_len = uiop->uio_resid = bp->b_bcount;
	    /* mapping was done by vmapbuf() */
	    io.iov_base = bp->b_data;
	    uiop->uio_offset = ((off_t)bp->b_blkno) << DEV_BSHIFT;
	    if (bp->b_flags & B_READ) {
		uiop->uio_rw = UIO_READ;
		nfsstats.read_physios++;
		error = nfs_readrpc(vp, uiop);
	    } else {
		iomode = NFSV3WRITE_DATASYNC;
		uiop->uio_rw = UIO_WRITE;
		nfsstats.write_physios++;
		error = nfs_writerpc(vp, uiop, &iomode, &must_commit);
	    }
	    if (error) {
		bp->b_flags |= B_ERROR;
		bp->b_error = error;
	    }
	} else if (bp->b_flags & B_READ) {
	    io.iov_len = uiop->uio_resid = bp->b_bcount;
	    io.iov_base = bp->b_data;
	    uiop->uio_rw = UIO_READ;
	    switch (vp->v_type) {
	    case VREG:
		uiop->uio_offset = ((off_t)bp->b_blkno) << DEV_BSHIFT;
		nfsstats.read_bios++;
		error = nfs_readrpc(vp, uiop);
		if (!error && uiop->uio_resid) {
			/*
			 * If len > 0, there is a hole in the file and
			 * no writes after the hole have been pushed to
			 * the server yet.
			 * Just zero fill the rest of the valid area.
			 */
			diff = bp->b_bcount - uiop->uio_resid;
			len = np->n_size - ((((off_t)bp->b_blkno) << DEV_BSHIFT)
				+ diff);
			if (len > 0) {
				len = MIN(len, uiop->uio_resid);
				memset((char *)bp->b_data + diff, 0, len);
			}
		}
		if (p && (vp->v_flag & VTEXT) &&
		    (np->n_mtime != np->n_vattr.va_mtime.tv_sec)) {
			uprintf("Process killed due to text file modification\n");
			psignal(p, SIGKILL);
			p->p_holdcnt++;
		}
		break;
	    case VLNK:
		uiop->uio_offset = (off_t)0;
		nfsstats.readlink_bios++;
		error = nfs_readlinkrpc(vp, uiop, curproc->p_ucred);
		break;
	    default:
		printf("nfs_doio:  type %x unexpected\n",vp->v_type);
		break;
	    }
	    if (error) {
		bp->b_flags |= B_ERROR;
		bp->b_error = error;
	    }
	} else {
	    io.iov_base = bp->b_data;
	    io.iov_len = uiop->uio_resid = bp->b_bcount;
	    uiop->uio_offset = ((off_t)bp->b_blkno) << DEV_BSHIFT;
	    uiop->uio_rw = UIO_WRITE;
	    nfsstats.write_bios++;
	    iomode = NFSV3WRITE_UNSTABLE;
	    error = nfs_writerpc(vp, uiop, &iomode, &must_commit);
	}
	bp->b_resid = uiop->uio_resid;
	if (must_commit)
		nfs_clearcommit(vp->v_mount);
	biodone(bp);
	return (error);
}

/*
 * Vnode op for VM getpages.
 */
int
nfs_getpages(v)
	void *v;
{
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		voff_t a_offset;
		vm_page_t *a_m;
		int *a_count;
		int a_centeridx;
		vm_prot_t a_access_type;
		int a_advice;
		int a_flags;
	} */ *ap = v;

	off_t eof, offset, origoffset, startoffset, endoffset;
	int s, i, error, npages, orignpages, npgs, ridx, pidx, pcount;
	vaddr_t kva;
	struct buf *bp, *mbp;
	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct uvm_object *uobj = &vp->v_uobj;
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	size_t bytes, iobytes, tailbytes, totalbytes, skipbytes;
	int flags = ap->a_flags;
	int bsize;
	struct vm_page *pgs[16];			/* XXXUBC 16 */
	boolean_t v3 = NFS_ISV3(vp);
	boolean_t async = (flags & PGO_SYNCIO) == 0;
	boolean_t write = (ap->a_access_type & VM_PROT_WRITE) != 0;
	struct proc *p = curproc;

	UVMHIST_FUNC("nfs_getpages"); UVMHIST_CALLED(ubchist);
	UVMHIST_LOG(ubchist, "vp %p off 0x%x count %d", vp, (int)ap->a_offset,
		    *ap->a_count,0);

#ifdef DIAGNOSTIC
	if (ap->a_centeridx < 0 || ap->a_centeridx >= *ap->a_count) {
		panic("nfs_getpages: centeridx %d out of range",
		      ap->a_centeridx);
	}
#endif

	error = 0;
	origoffset = ap->a_offset;
	eof = vp->v_size;
	if (origoffset >= eof) {
		if ((flags & PGO_LOCKED) == 0) {
			simple_unlock(&uobj->vmobjlock);
		}
		UVMHIST_LOG(ubchist, "off 0x%x past EOF 0x%x",
			    (int)origoffset, (int)eof,0,0);
		return EINVAL;
	}

	if (flags & PGO_LOCKED) {
		uvn_findpages(uobj, origoffset, ap->a_count, ap->a_m,
			      UFP_NOWAIT|UFP_NOALLOC);
		return 0;
	}

	/* vnode is VOP_LOCKed, uobj is locked */
	if (write && (vp->v_bioflag & VBIOONSYNCLIST) == 0) {
		vn_syncer_add_to_worklist(vp, syncdelay);
	}
	bsize = nmp->nm_rsize;
	orignpages = MIN(*ap->a_count,
			 round_page(eof - origoffset) >> PAGE_SHIFT);
	npages = orignpages;
	startoffset = origoffset & ~(bsize - 1);
	endoffset = round_page((origoffset + (npages << PAGE_SHIFT)
				+ bsize - 1) & ~(bsize - 1));
	endoffset = MIN(endoffset, round_page(eof));
	ridx = (origoffset - startoffset) >> PAGE_SHIFT;

	if (!async && !write) {
		int rapages = MAX(PAGE_SIZE, nmp->nm_rsize) >> PAGE_SHIFT;

		(void) VOP_GETPAGES(vp, endoffset, NULL, &rapages, 0,
				    VM_PROT_READ, 0, 0);
		simple_lock(&uobj->vmobjlock);
	}

	UVMHIST_LOG(ubchist, "npages %d offset 0x%x", npages,
		    (int)origoffset, 0,0);
	memset(pgs, 0, sizeof(pgs));
	uvn_findpages(uobj, origoffset, &npages, &pgs[ridx], UFP_ALL);

	if (flags & PGO_OVERWRITE) {
		UVMHIST_LOG(ubchist, "PGO_OVERWRITE",0,0,0,0);

		/* XXXUBC for now, zero the page if we allocated it */
		for (i = 0; i < npages; i++) {
			struct vm_page *pg = pgs[ridx + i];

			if (pg->flags & PG_FAKE) {
				uvm_pagezero(pg);
				pg->flags &= ~(PG_FAKE);
			}
		}
		npages += ridx;
		if (v3) {
			simple_unlock(&uobj->vmobjlock);
			goto uncommit;
		}
		goto out;
	}

	/*
	 * if the pages are already resident, just return them.
	 */

	for (i = 0; i < npages; i++) {
		struct vm_page *pg = pgs[ridx + i];

		if ((pg->flags & PG_FAKE) != 0 ||
		    ((ap->a_access_type & VM_PROT_WRITE) &&
		      (pg->flags & PG_RDONLY))) {
			break;
		}
	}
	if (i == npages) {
		UVMHIST_LOG(ubchist, "returning cached pages", 0,0,0,0);
		npages += ridx;
		goto out;
	}

	/*
	 * the page wasn't resident and we're not overwriting,
	 * so we're going to have to do some i/o.
	 * find any additional pages needed to cover the expanded range.
	 */

	if (startoffset != origoffset ||
	    startoffset + (npages << PAGE_SHIFT) != endoffset) {

		/*
		 * XXXUBC we need to avoid deadlocks caused by locking
		 * additional pages at lower offsets than pages we
		 * already have locked.  for now, unlock them all and
		 * start over.
		 */

		for (i = 0; i < npages; i++) {
			struct vm_page *pg = pgs[ridx + i];

			if (pg->flags & PG_FAKE) {
				pg->flags |= PG_RELEASED;
			}
		}
		uvm_page_unbusy(&pgs[ridx], npages);
		memset(pgs, 0, sizeof(pgs));

		UVMHIST_LOG(ubchist, "reset npages start 0x%x end 0x%x",
			    startoffset, endoffset, 0,0);
		npages = (endoffset - startoffset) >> PAGE_SHIFT;
		npgs = npages;
		uvn_findpages(uobj, startoffset, &npgs, pgs, UFP_ALL);
	}
	simple_unlock(&uobj->vmobjlock);

	/*
	 * update the cached read creds for this node.
	 */

	if (np->n_rcred) {
		crfree(np->n_rcred);
	}
	np->n_rcred = curproc->p_ucred;
	crhold(np->n_rcred);

	/*
	 * read the desired page(s).
	 */

	totalbytes = npages << PAGE_SHIFT;
	bytes = MIN(totalbytes, vp->v_size - startoffset);
	tailbytes = totalbytes - bytes;
	skipbytes = 0;

	kva = uvm_pagermapin(pgs, npages, UVMPAGER_MAPIN_WAITOK |
			     UVMPAGER_MAPIN_READ);

	s = splbio();
	mbp = pool_get(&bufpool, PR_WAITOK);
	splx(s);
	mbp->b_bufsize = totalbytes;
	mbp->b_data = (void *)kva;
	mbp->b_resid = mbp->b_bcount = bytes;
	mbp->b_flags = B_BUSY|B_READ| (async ? B_CALL|B_ASYNC : 0);
	mbp->b_iodone = uvm_aio_biodone;
	mbp->b_vp = NULL;
	mbp->b_proc = NULL;		/* XXXUBC */
	LIST_INIT(&mbp->b_dep);
	bgetvp(vp, mbp);

	/*
	 * if EOF is in the middle of the last page, zero the part past EOF.
	 */

	if (tailbytes > 0 && (pgs[bytes >> PAGE_SHIFT]->flags & PG_FAKE)) {
		memset((char *)kva + bytes, 0, tailbytes);
	}

	/*
	 * now loop over the pages, reading as needed.
	 */

	bp = NULL;
	for (offset = startoffset;
	     bytes > 0;
	     offset += iobytes, bytes -= iobytes) {

		/*
		 * skip pages which don't need to be read.
		 */

		pidx = (offset - startoffset) >> PAGE_SHIFT;
		UVMHIST_LOG(ubchist, "pidx %d offset 0x%x startoffset 0x%x",
			    pidx, (int)offset, (int)startoffset,0);
		while ((pgs[pidx]->flags & PG_FAKE) == 0) {
			size_t b;

			KASSERT((offset & (PAGE_SIZE - 1)) == 0);
			b = MIN(PAGE_SIZE, bytes);
			offset += b;
			bytes -= b;
			skipbytes += b;
			pidx++;
			UVMHIST_LOG(ubchist, "skipping, new offset 0x%x",
				    (int)offset, 0,0,0);
			if (bytes == 0) {
				goto loopdone;
			}
		}

		/*
		 * see how many pages can be read with this i/o.
		 * reduce the i/o size if necessary.
		 */

		iobytes = bytes;
		if (offset + iobytes > round_page(offset)) {
			pcount = 1;
			while (pidx + pcount < npages &&
			       pgs[pidx + pcount]->flags & PG_FAKE) {
				pcount++;
			}
			iobytes = MIN(iobytes, (pcount << PAGE_SHIFT) -
				      (offset - trunc_page(offset)));
		}
		iobytes = MIN(iobytes, nmp->nm_rsize);

		/*
		 * allocate a sub-buf for this piece of the i/o
		 * (or just use mbp if there's only 1 piece),
		 * and start it going.
		 */

		if (offset == startoffset && iobytes == bytes) {
			bp = mbp;
		} else {
			s = splbio();
			bp = pool_get(&bufpool, PR_WAITOK);
			splx(s);
			bp->b_data = (char *)kva + offset - startoffset;
			bp->b_resid = bp->b_bcount = iobytes;
			bp->b_flags = B_BUSY|B_READ|B_CALL|B_ASYNC;
			bp->b_iodone = uvm_aio_biodone1;
			bp->b_vp = vp;
			bp->b_proc = NULL;	/* XXXUBC */
			LIST_INIT(&bp->b_dep);
		}
		bp->b_private = mbp;
		bp->b_lblkno = bp->b_blkno = offset >> DEV_BSHIFT;

		UVMHIST_LOG(ubchist, "bp %p offset 0x%x bcount 0x%x blkno 0x%x",
			    bp, offset, iobytes, bp->b_blkno);

		VOP_STRATEGY(bp);
	}

loopdone:
	if (skipbytes) {
		s = splbio();
		mbp->b_resid -= skipbytes;
		if (mbp->b_resid == 0) {
			biodone(mbp);
		}
		splx(s);
	}
	if (async) {
		UVMHIST_LOG(ubchist, "returning 0 (async)",0,0,0,0);
		return 0;
	}
	if (bp != NULL) {
		error = biowait(mbp);
	}
	s = splbio();
	(void) buf_cleanout(mbp);
	pool_put(&bufpool, mbp);
	splx(s);
	uvm_pagermapout(kva, npages);
 
	if (write && v3) {
uncommit:
 		lockmgr(&np->n_commitlock, LK_EXCLUSIVE, NULL, p);
		nfs_del_committed_range(vp, origoffset, npages);
		nfs_del_tobecommitted_range(vp, origoffset, npages);
		simple_lock(&uobj->vmobjlock);
		for (i = 0; i < npages; i++) {
			if (pgs[i] == NULL) {
				continue;
			}
			pgs[i]->flags &= ~(PG_NEEDCOMMIT|PG_RDONLY);
		}
		simple_unlock(&uobj->vmobjlock);
 		lockmgr(&np->n_commitlock, LK_RELEASE, NULL, p);
	}

	simple_lock(&uobj->vmobjlock);

out:
	if (error) {
		uvm_lock_pageq();
		for (i = 0; i < npages; i++) {
			if (pgs[i] == NULL) {
				continue;
			}
			UVMHIST_LOG(ubchist, "examining pg %p flags 0x%x",
				    pgs[i], pgs[i]->flags, 0,0);
			if (pgs[i]->flags & PG_WANTED) {
				wakeup(pgs[i]);
			}
			if (pgs[i]->flags & PG_RELEASED) {
				uvm_unlock_pageq();
				(uobj->pgops->pgo_releasepg)(pgs[i], NULL);
				uvm_lock_pageq();
				continue;
			}
			if (pgs[i]->flags & PG_FAKE) {
				uvm_pagefree(pgs[i]);
				continue;
			}
			uvm_pageactivate(pgs[i]);
			pgs[i]->flags &= ~(PG_WANTED|PG_BUSY);
			UVM_PAGE_OWN(pgs[i], NULL);
		}
		uvm_unlock_pageq();
		simple_unlock(&uobj->vmobjlock);
		UVMHIST_LOG(ubchist, "returning error %d", error,0,0,0);
		return error;
	}

	UVMHIST_LOG(ubchist, "ridx %d count %d", ridx, npages, 0,0);
	uvm_lock_pageq();
	for (i = 0; i < npages; i++) {
		if (pgs[i] == NULL) {
			continue;
		}
		UVMHIST_LOG(ubchist, "examining pg %p flags 0x%x",
			    pgs[i], pgs[i]->flags, 0,0);
		if (pgs[i]->flags & PG_FAKE) {
			UVMHIST_LOG(ubchist, "unfaking pg %p offset 0x%x",
				    pgs[i], (int)pgs[i]->offset,0,0);
			pgs[i]->flags &= ~(PG_FAKE);
			pmap_clear_modify(pgs[i]);
			pmap_clear_reference(pgs[i]);
		}
		if (i < ridx || i >= ridx + orignpages || async) {
			UVMHIST_LOG(ubchist, "unbusy pg %p offset 0x%x",
				    pgs[i], (int)pgs[i]->offset,0,0);
			if (pgs[i]->flags & PG_WANTED) {
				wakeup(pgs[i]);
			}
			if (pgs[i]->flags & PG_RELEASED) {
				uvm_unlock_pageq();
				(uobj->pgops->pgo_releasepg)(pgs[i], NULL);
				uvm_lock_pageq();
				continue;
			}
			uvm_pageactivate(pgs[i]);
			pgs[i]->flags &= ~(PG_WANTED|PG_BUSY);
			UVM_PAGE_OWN(pgs[i], NULL);
		}
	}
	uvm_unlock_pageq();
	simple_unlock(&uobj->vmobjlock);
	if (ap->a_m != NULL) {
		memcpy(ap->a_m, &pgs[ridx],
		       *ap->a_count * sizeof(struct vm_page *));
	}
	return 0;
}

/*
 * Vnode op for VM putpages.
 */
int
nfs_putpages(v)
	void *v;
{
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		struct vm_page **a_m;
		int a_count;
		int a_flags;
		int *a_rtvals;
	} */ *ap = v;

	struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct buf *bp, *mbp;
	struct vm_page **pgs = ap->a_m;
	int flags = ap->a_flags;
	int npages = ap->a_count;
	int s, error, i;
	size_t bytes, iobytes, skipbytes;
	vaddr_t kva;
	off_t offset, origoffset, commitoff;
	uint32_t commitbytes;
	boolean_t v3 = NFS_ISV3(vp);
	boolean_t async = (flags & PGO_SYNCIO) == 0;
	boolean_t weak = (flags & PGO_WEAK) && v3;
	struct proc *p = curproc;
	UVMHIST_FUNC("nfs_putpages"); UVMHIST_CALLED(ubchist);

	UVMHIST_LOG(ubchist, "vp %p pgp %p count %d",
		    vp, ap->a_m, ap->a_count,0);

	simple_unlock(&vp->v_uobj.vmobjlock);

	error = 0;
	origoffset = pgs[0]->offset;
	bytes = MIN(ap->a_count << PAGE_SHIFT, vp->v_size - origoffset);
	skipbytes = 0;

	/*
	 * if the range has been committed already, mark the pages thus.
	 * if the range just needs to be committed, we're done
	 * if it's a weak putpage, otherwise commit the range.
	 */

	if (v3) {
 		lockmgr(&np->n_commitlock, LK_EXCLUSIVE, NULL, p);
		if (nfs_in_committed_range(vp, origoffset, bytes)) {
			goto committed;
		}
		if (nfs_in_tobecommitted_range(vp, origoffset, bytes)) {
			if (weak) {
				lockmgr(&np->n_commitlock, LK_RELEASE, NULL, p);
				return 0;
			} else {
				commitoff = np->n_pushlo;
				commitbytes = (uint32_t)(np->n_pushhi -
							 np->n_pushlo);
				goto commit;
			}
		}
		lockmgr(&np->n_commitlock, LK_RELEASE, NULL, p);
	}

	/*
	 * otherwise write or commit all the pages.
	 */

	kva = uvm_pagermapin(pgs, ap->a_count, UVMPAGER_MAPIN_WAITOK|
			     UVMPAGER_MAPIN_WRITE);

	s = splbio();
	vp->v_numoutput += 2;
	mbp = pool_get(&bufpool, PR_WAITOK);
	UVMHIST_LOG(ubchist, "vp %p mbp %p num now %d bytes 0x%x",
		    vp, mbp, vp->v_numoutput, bytes);
	splx(s);
	mbp->b_bufsize = npages << PAGE_SHIFT;
	mbp->b_data = (void *)kva;
	mbp->b_resid = mbp->b_bcount = bytes;
	mbp->b_flags = B_BUSY|B_WRITE|B_AGE |
		(async ? B_CALL|B_ASYNC : 0) |
		(curproc == uvm.pagedaemon_proc ? B_PDAEMON : 0);
	mbp->b_iodone = uvm_aio_biodone;
	mbp->b_vp = NULL;
	mbp->b_proc = NULL;		/* XXXUBC */
	LIST_INIT(&mbp->b_dep);
	bgetvp(vp, mbp);

	for (offset = origoffset;
	     bytes > 0;
	     offset += iobytes, bytes -= iobytes) {
		iobytes = MIN(nmp->nm_wsize, bytes);

 		/*
		 * skip writing any pages which only need a commit.
		 */

		if ((pgs[(offset - origoffset) >> PAGE_SHIFT]->flags &
		     PG_NEEDCOMMIT) != 0) {
			KASSERT((offset & (PAGE_SIZE - 1)) == 0);
			iobytes = MIN(PAGE_SIZE, bytes);
			skipbytes += iobytes;
			continue;
		}

		/* if it's really one i/o, don't make a second buf */
		if (offset == origoffset && iobytes == bytes) {
			bp = mbp;
		} else {
			s = splbio();
			vp->v_numoutput++;
			bp = pool_get(&bufpool, PR_WAITOK);
			UVMHIST_LOG(ubchist, "vp %p bp %p num now %d",
				    vp, bp, vp->v_numoutput, 0);
			splx(s);
			bp->b_data = (char *)kva + (offset - origoffset);
			bp->b_resid = bp->b_bcount = iobytes;
			bp->b_flags = B_BUSY|B_WRITE|B_CALL|B_ASYNC;
			bp->b_iodone = uvm_aio_biodone1;
			bp->b_vp = vp;
			bp->b_proc = NULL;	/* XXXUBC */
			LIST_INIT(&bp->b_dep);
		}
		bp->b_private = mbp;
		bp->b_lblkno = bp->b_blkno = (daddr_t)(offset >> DEV_BSHIFT);
		UVMHIST_LOG(ubchist, "bp %p numout %d",
			    bp, vp->v_numoutput,0,0);
		VOP_STRATEGY(bp);
	}
	if (skipbytes) {
		UVMHIST_LOG(ubchist, "skipbytes %d", bytes, 0,0,0);
		s = splbio();
		mbp->b_resid -= skipbytes;
		if (mbp->b_resid == 0) {
			biodone(mbp);
		}
		splx(s);
	}
	if (async) {
		return 0;
	}
	if (bp != NULL) {
		error = biowait(mbp);
	}

	s = splbio();
	if (mbp->b_vp) {
		vwakeup(mbp->b_vp);
	}
	(void) buf_cleanout(mbp);
	pool_put(&bufpool, mbp);
	splx(s);

	uvm_pagermapout(kva, ap->a_count);
	if (error || !v3) {
		UVMHIST_LOG(ubchist, "returning error %d", error, 0,0,0);
		return error;
	}

	/*
	 * for a weak put, mark the range as "to be committed"
	 * and mark the pages read-only so that we will be notified
	 * to remove the pages from the "to be committed" range
	 * if they are made dirty again.
	 * for a strong put, commit the pages and remove them from the
	 * "to be committed" range.  also, mark them as writable
	 * and not cleanable with just a commit.
	 */

	lockmgr(&np->n_commitlock, LK_EXCLUSIVE, NULL, p);
	if (weak) {
		nfs_add_tobecommitted_range(vp, origoffset,
					    npages << PAGE_SHIFT);
		for (i = 0; i < npages; i++) {
			pgs[i]->flags |= PG_NEEDCOMMIT|PG_RDONLY;
		}
	} else {
		commitoff = origoffset;
		commitbytes = npages << PAGE_SHIFT;
commit:
		error = nfs_commit(vp, commitoff, commitbytes, curproc);
		nfs_del_tobecommitted_range(vp, commitoff, commitbytes);
committed:
		for (i = 0; i < npages; i++) {
			pgs[i]->flags &= ~(PG_NEEDCOMMIT|PG_RDONLY);
		}
	}
	lockmgr(&np->n_commitlock, LK_RELEASE, NULL, p);
	return error;
}
