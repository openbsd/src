/*	$OpenBSD: sysv_ipc.c,v 1.3 1998/06/11 18:32:13 deraadt Exp $	*/
/*	$NetBSD: sysv_ipc.c,v 1.10 1995/06/03 05:53:28 mycroft Exp $	*/

/*
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
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
 *      This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/ipc.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/vnode.h>

/*
 * Check for ipc permission
 */

int
ipcperm(cred, perm, mode)
	struct ucred *cred;
	struct ipc_perm *perm;
	int mode;
{

	if (mode == IPC_M) {
		if (cred->cr_uid == 0 ||
		    cred->cr_uid == perm->uid ||
		    cred->cr_uid == perm->cuid)
			return (0);
		return (EPERM);
	}

	if (vaccess(perm->mode, perm->uid, perm->gid, mode, cred) == 0 ||
	    vaccess(perm->mode, perm->cuid, perm->cgid, mode, cred) == 0)
		return (0);
	return (EACCES);
}

void
ipc_n2o(n, o)
	struct ipc_perm *n;
	struct oipc_perm *o;
{
	o->cuid = n->cuid;		/* XXX */
	o->cgid = n->cgid;		/* XXX */
	o->uid = n->uid;		/* XXX */
	o->gid = n->gid;		/* XXX */
	o->mode = n->mode;		/* XXX */
	o->seq = n->seq;
	o->key = n->key;
}

void
ipc_o2n(o, n)
	struct oipc_perm *o;
	struct ipc_perm *n;
{
	n->cuid = o->cuid;		/* XXX */
	n->cgid = o->cgid;		/* XXX */
	n->uid = o->uid;		/* XXX */
	n->gid = o->gid;		/* XXX */
	n->mode = o->mode;		/* XXX */
	n->seq = o->seq;
	n->key = o->key;
}

