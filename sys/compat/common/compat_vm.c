/*	$OpenBSD: compat_vm.c,v 1.5 2007/11/28 13:10:28 deraadt Exp $	*/
/*	$NetBSD: vm_12.c,v 1.8 1997/10/20 22:05:22 thorpej Exp $	*/

/*
 * Copyright (c) 1997 Matthew R. Green
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
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>		/* needed for next include! */
#include <sys/syscallargs.h>

#include <sys/swap.h>
#include <sys/mman.h>

int
compat_25_sys_swapon(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_swapctl_args ua;
	struct compat_25_sys_swapon_args /* {
		syscallarg(const char *) name;
	} */ *uap = v;

	SCARG(&ua, cmd) = SWAP_ON;
	SCARG(&ua, arg) = (void *)SCARG(uap, name);
	SCARG(&ua, misc) = 0;	/* priority */
	return (sys_swapctl(p, &ua, retval));
}
