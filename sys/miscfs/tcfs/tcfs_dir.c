/*	$OpenBSD: tcfs_dir.c,v 1.4 2002/03/14 01:27:08 millert Exp $	*/
/*
 * Copyright 2000 The TCFS Project at http://tcfs.dia.unisa.it/
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/dirent.h>

#include <miscfs/tcfs/tcfs.h>
#include <miscfs/tcfs/tcfs_rw.h>

int tcfs_new_direntry(void *, tcfs_fileinfo *);

int
tcfs_new_direntry(void *v, tcfs_fileinfo *i)
{
	struct vop_create_args *x;
	struct ucred *cr;
	struct proc *pr;
	int err;

	x = (struct vop_create_args *)v;
	cr = x->a_cnp->cn_cred;
	pr = x->a_cnp->cn_proc;

	if (!TCFS_CHECK_AKEY(cr, pr, x->a_dvp))
		return tcfs_bypass(v);

	/* Per i file speciali e per i link, niente flags (per ora)*/
	if((x->a_desc == VDESC(vop_mknod)) ||
	   (x->a_desc == VDESC(vop_symlink)) ||
	   (x->a_desc == VDESC(vop_link))) {
		err=tcfs_bypass(v);
		return err;
	}
	   
	if (!(err=tcfs_bypass(v)))
		err = tcfs_xsetflags(*(x->a_vpp),pr,cr,i);

	return err;
}

int
tcfs_create(v)
	void *v;
{
	struct vop_create_args *x;
	struct ucred *cr;
	struct proc *pr;
	tcfs_fileinfo i;

	x = (struct vop_create_args *)v;
	cr = x->a_cnp->cn_cred;
	pr = x->a_cnp->cn_proc;
	i = tcfs_xgetflags(x->a_dvp, pr, cr);

	if (FI_CFLAG(&i)||FI_GSHAR(&i))
		return tcfs_new_direntry(v, &i);
	else
		return tcfs_bypass(v);
}

int 
tcfs_mknod(v)
	void *v;
{
	struct vop_mknod_args *x;
	struct ucred *cr;
	struct proc *pr;
	tcfs_fileinfo i;

	x = (struct vop_mknod_args *)v;
	cr = x->a_cnp->cn_cred;
	pr = x->a_cnp->cn_proc;
	i = tcfs_xgetflags(x->a_dvp, pr, cr);

	if (FI_CFLAG(&i) || FI_GSHAR(&i))
		return tcfs_new_direntry(v, &i);
	else
		return tcfs_bypass(v);
}

int
tcfs_mkdir(v)
	void *v;
{
	struct vop_mkdir_args *x;
	struct ucred *cr;
	struct proc *pr;
	tcfs_fileinfo i;

	x = (struct vop_mkdir_args *)v;
	cr = x->a_cnp->cn_cred;
	pr = x->a_cnp->cn_proc;
	i = tcfs_xgetflags(x->a_dvp,pr,cr);

	if (FI_CFLAG(&i) || FI_GSHAR(&i))
		return tcfs_new_direntry(v, &i);
	else
		return tcfs_bypass(v);
}

int tcfs_link(v)
	void *v;
{
	struct vop_link_args *x;
	struct ucred *cr;
	struct proc *pr;
	tcfs_fileinfo i;

	x = (struct vop_link_args *)v;
	cr = x->a_cnp->cn_cred;
	pr = x->a_cnp->cn_proc;
	i = tcfs_xgetflags(x->a_dvp, pr, cr);

	if (FI_CFLAG(&i) || FI_GSHAR(&i))
		return tcfs_new_direntry(v, &i);
	else
		return tcfs_bypass(v);
}

int
tcfs_symlink(v)
	void *v;
{
	struct vop_symlink_args *x;
	struct ucred *cr;
	struct proc *pr;
	tcfs_fileinfo i;

	x = (struct vop_symlink_args *)v;
	cr = x->a_cnp->cn_cred;
	pr = x->a_cnp->cn_proc;
	i = tcfs_xgetflags(x->a_dvp,pr,cr);

	if (FI_CFLAG(&i) || FI_GSHAR(&i))
		return tcfs_new_direntry(v, &i);
	else
		return tcfs_bypass(v);
}

int
tcfs_readdir(v)
	void *v;
{
/*
	tcfs_fileinfo i;
	struct vop_readdir_args *a=(struct vop_readdir_args*)v;
	struct dirent *s,*d,*e;
	char *p;
	int err;
	off_t offset;
	int req,resid;
	
	
	i=tcfs_xgetflags(a->a_vp,a->a_uio->uio_procp,a->a_cred);
	offset=a->a_uio->uio_offset;
	req=a->a_uio->uio_resid;
	p=a->a_uio->uio_iov->iov_base;
	
	err=tcfs_bypass(v);
	resid=a->a_uio->uio_resid;

	s=(struct dirent*)p;
	e=(struct dirent*)a->a_uio->uio_iov->iov_base;
	
	if( (!FI_CFLAG(&i)) && (!FI_GSHAR(&i)) )
		return err;

	for(d=s; d<e && d->d_reclen;d=(struct dirent*)((char*)d+d->d_reclen))
		{
		 if(!d->d_fileno) 
			continue;
		 if((d->d_namlen==1 && d->d_name[0]=='.') ||
		    (d->d_namlen==2 && d->d_name[0]=='.' && d->d_name[1]=='.'))
			continue;
	
		if(d->d_namlen)
			d->d_name[0]--;
		}
	return err;
*/
	return tcfs_bypass(v);
}

int
tcfs_rename(v)
	void *v;
{
	return tcfs_bypass(v);
}
