/* 	$OpenBSD: compat_util.c,v 1.14 2014/12/16 18:30:03 tedu Exp $	*/
/* 	$NetBSD: compat_util.c,v 1.4 1996/03/14 19:31:45 christos Exp $	*/

/*
 * Copyright (c) 1994 Christos Zoulas
 * Copyright (c) 1995 Frank van der Linden
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/vnode.h>

#include <uvm/uvm_extern.h>

#include <compat/common/compat_util.h>

/*
 * Search an alternate path before passing pathname arguments on
 * to system calls. Useful for keeping a separate 'emulation tree'.
 *
 * If cflag is set, we check if an attempt can be made to create
 * the named file, i.e. we check if the directory it should
 * be in exists.
 */
int
emul_find(struct proc *p, caddr_t *sgp, const char *prefix,
    char *path, char **pbuf, int cflag)
{
	struct nameidata	 nd;
	struct nameidata	 ndroot;
	struct vattr		 vat;
	struct vattr		 vatroot;
	int			 error;
	char			*ptr, *buf, *cp;
	const char		*pr;
	size_t			 sz, len;

	buf = (char *) malloc(MAXPATHLEN, M_TEMP, M_WAITOK);
	*pbuf = path;

	for (ptr = buf, pr = prefix; (*ptr = *pr) != '\0'; ptr++, pr++)
		continue;

	sz = MAXPATHLEN - (ptr - buf);

	/* 
	 * If sgp is not given then the path is already in kernel space
	 */
	if (sgp == NULL)
		error = copystr(path, ptr, sz, &len);
	else
		error = copyinstr(path, ptr, sz, &len);

	if (error)
		goto bad;

	if (*ptr != '/') {
		error = EINVAL;
		goto bad;
	}

	/*
	 * We know that there is a / somewhere in this pathname.
	 * Search backwards for it, to find the file's parent dir
	 * to see if it exists in the alternate tree. If it does,
	 * and we want to create a file (cflag is set). We don't
	 * need to worry about the root comparison in this case.
	 */

	if (cflag) {
		for (cp = &ptr[len] - 1; *cp != '/'; cp--)
			;
		*cp = '\0';

		NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, buf, p);

		if ((error = namei(&nd)) != 0)
			goto bad;

		*cp = '/';
	} else {
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, buf, p);

		if ((error = namei(&nd)) != 0)
			goto bad;

		/*
		 * We now compare the vnode of the emulation root to the one
		 * vnode asked. If they resolve to be the same, then we
		 * ignore the match so that the real root gets used.
		 * This avoids the problem of traversing "../.." to find the
		 * root directory and never finding it, because "/" resolves
		 * to the emulation root directory. This is expensive :-(
		 */
		/* XXX: prototype should have const here for NDINIT */
		NDINIT(&ndroot, LOOKUP, FOLLOW, UIO_SYSSPACE, prefix, p);

		if ((error = namei(&ndroot)) != 0)
			goto bad2;

		if ((error = VOP_GETATTR(nd.ni_vp, &vat, p->p_ucred, p)) != 0)
			goto bad3;

		if ((error = VOP_GETATTR(ndroot.ni_vp, &vatroot, p->p_ucred, p))
		    != 0)
			goto bad3;

		if (vat.va_fsid == vatroot.va_fsid &&
		    vat.va_fileid == vatroot.va_fileid) {
			error = ENOENT;
			goto bad3;
		}
	}
	if (sgp == NULL)
		*pbuf = buf;
	else {
		sz = &ptr[len] - buf;
		*pbuf = stackgap_alloc(sgp, sz + 1);
		if (*pbuf == NULL) {
			error = ENAMETOOLONG;
			goto bad;
		}
		if ((error = copyout(buf, *pbuf, sz)) != 0) {
			*pbuf = path;
			goto bad;
		}
		free(buf, M_TEMP, 0);
	}

	vrele(nd.ni_vp);
	if (!cflag)
		vrele(ndroot.ni_vp);
	return error;

bad3:
	vrele(ndroot.ni_vp);
bad2:
	vrele(nd.ni_vp);
bad:
	free(buf, M_TEMP, 0);
	return error;
}

caddr_t  
stackgap_init(struct proc *p)
{
        return STACKGAPBASE;
}
 
void *          
stackgap_alloc(caddr_t *sgp, size_t sz)
{
	void *n = (void *) *sgp;
	caddr_t nsgp;
	
	sz = ALIGN(sz);
	nsgp = *sgp + sz;
#ifdef MACHINE_STACK_GROWS_UP
	if (nsgp > ((caddr_t)PS_STRINGS) + STACKGAPLEN)
		return NULL;
#else
	if (nsgp > ((caddr_t)PS_STRINGS))
		return NULL;
#endif
	*sgp = nsgp;
	return n;
}
