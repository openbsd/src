/*	$OpenBSD: tcfs_fileinfo.c,v 1.3 2000/06/17 20:25:54 provos Exp $	*/
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
#include <sys/stat.h>

#include <miscfs/tcfs/tcfs.h>
#include <miscfs/tcfs/tcfs_rw.h>


tcfs_fileinfo
tcfs_xgetflags(struct vnode *v, struct proc *p, struct ucred *c)
{
        tcfs_fileinfo r;
        struct vop_getattr_args x;
        struct vattr att;
        int retval;

        att = va_null;
        x.a_desc = VDESC(vop_getattr);
        x.a_vp = v;
        x.a_vap = &att;
        x.a_cred = c;
        x.a_p = p;

        retval = tcfs_bypass((void*)&x);
        r.flag = (unsigned long)(x.a_vap->va_flags);
        r.end_of_file = x.a_vap->va_size;

        return r;
}

int 
tcfs_xsetflags(struct vnode *v, struct proc *p, struct ucred *c,
	       tcfs_fileinfo *i)
{
        struct vop_setattr_args x;
        struct vattr att;
        int retval;

        att = va_null;

	att.va_flags = i->flag;

        x.a_desc = VDESC(vop_setattr);
        x.a_vp = v;
        x.a_vap = &att;
        x.a_cred = c;
        x.a_p = p;

        retval = tcfs_bypass((void*)&x);
        return retval;
}



tcfs_fileinfo
tcfs_get_fileinfo(void *a)
{
	struct vop_read_args *arg;

	arg = (struct vop_read_args*)a;
	return tcfs_xgetflags(arg->a_vp, arg->a_uio->uio_procp, arg->a_cred);
}


int
tcfs_set_fileinfo(void *a, tcfs_fileinfo *i)
{
	struct vop_read_args *arg;

	arg = (struct vop_read_args*)a;
	return tcfs_xsetflags(arg->a_vp, arg->a_uio->uio_procp,
			      arg->a_cred, i);
}
