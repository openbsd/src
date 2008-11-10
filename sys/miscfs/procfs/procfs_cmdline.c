/*	$OpenBSD: procfs_cmdline.c,v 1.9 2008/11/10 03:38:53 deraadt Exp $	*/
/*	$NetBSD: procfs_cmdline.c,v 1.3 1999/03/13 22:26:48 thorpej Exp $	*/

/*
 * Copyright (c) 1999 Jaromir Dolecek <dolecek@ics.muni.cz>
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/syslimits.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <miscfs/procfs/procfs.h>
#include <uvm/uvm_extern.h>

/*
 * code for returning process's command line arguments
 */
int
procfs_docmdline(struct proc *curp, struct proc *p, struct pfsnode *pfs, struct uio *uio)
{
	struct ps_strings pss;
	int count, error, i;
	size_t len, xlen, upper_bound;
	struct uio auio;
	struct iovec aiov;
	struct vmspace *vm;
	vaddr_t argv;
	char *arg;

	/* Don't allow writing. */
	if (uio->uio_rw != UIO_READ)
		return (EOPNOTSUPP);

	/*
	 * Allocate a temporary buffer to hold the arguments.
	 */
	arg = malloc(PAGE_SIZE, M_TEMP, M_WAITOK);

	/*
	 * Zombies don't have a stack, so we can't read their psstrings.
	 * System processes also don't have a user stack.  This is what
	 * ps(1) would display.
	 */
	if (P_ZOMBIE(p) || (p->p_flag & P_SYSTEM) != 0) {
                len = snprintf(arg, PAGE_SIZE, "(%s)", p->p_comm);
                if (uio->uio_offset >= (off_t)len)
                        error = 0;
                else
                        error = uiomove(arg, len - uio->uio_offset, uio);
		
                free(arg, M_TEMP);
                return (error);	
	}

	/*
	 * NOTE: Don't bother doing a process_checkioperm() here
	 * because the psstrings info is available by using ps(1),
	 * so it's not like there's anything to protect here.
	 */

	/*
	 * Lock the process down in memory.
	 */
	/* XXXCDC: how should locking work here? */
	if ((p->p_flag & P_WEXIT) || (p->p_vmspace->vm_refcnt < 1)) {
		free(arg, M_TEMP);
		return (EFAULT);
	}
	vm = p->p_vmspace;
	vm->vm_refcnt++;	/* XXX */

	/*
	 * Read in the ps_strings structure.
	 */
	aiov.iov_base = &pss;
	aiov.iov_len = sizeof(pss);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = (vaddr_t)PS_STRINGS;
	auio.uio_resid = sizeof(pss);
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_procp = NULL;
	error = uvm_io(&vm->vm_map, &auio, 0);
	if (error)
		goto bad;

	/*
	 * Now read the address of the argument vector.
	 */
	aiov.iov_base = &argv;
	aiov.iov_len = sizeof(argv);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = (vaddr_t)pss.ps_argvstr;
	auio.uio_resid = sizeof(argv);
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ; 
	auio.uio_procp = NULL;
	error = uvm_io(&vm->vm_map, &auio, 0);
	if (error)
		goto bad;

	/*
	 * Now copy in the actual argument vector, one byte at a time,
	 * since we don't know how long the vector is (though, we do
	 * know how many NUL-terminated strings are in the vector).
	 */
	len = 0;
	count = pss.ps_nargvstr;
	upper_bound = round_page(uio->uio_offset + uio->uio_resid);
	for (; count && len < upper_bound; len += xlen) {
		aiov.iov_base = arg;
		aiov.iov_len = PAGE_SIZE;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = argv + len;
		xlen = PAGE_SIZE - ((argv + len) & PAGE_MASK);
		auio.uio_resid = xlen;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_rw = UIO_READ;
		auio.uio_procp = NULL;
		error = uvm_io(&vm->vm_map, &auio, 0);
		if (error)
			goto bad;

		for (i = 0; i < xlen && count != 0; i++) {
			if (arg[i] == '\0')
                                count--;        /* one full string */
                }

		if (count == 0)
                        i--;                /* exclude the final NUL */

                if (len + i > uio->uio_offset) {
                        /* Have data in this page, copy it out */
                        error = uiomove(arg + uio->uio_offset - len,
                            i + len - uio->uio_offset, uio);
                        if (error || uio->uio_resid <= 0)
                                break;
                }
	}


 bad:
	uvmspace_free(vm);
	free(arg, M_TEMP);
	return (error);
}
