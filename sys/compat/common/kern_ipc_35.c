/*	$OpenBSD: kern_ipc_35.c,v 1.1 2004/05/03 17:38:48 millert Exp $	*/

/*
 * Copyright (c) 2004 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#ifdef SYSVMSG
/*
 * Old-style shmget(2) used int for the size parameter, we now use size_t.
 */
int
compat_35_sys_shmget(struct proc *p, void *v, register_t *retval)
{
	struct compat_35_sys_shmget_args /* {
		syscallarg(key_t) key;
		syscallarg(int) size;
		syscallarg(int) shmflg;
	} */ *uap = v;
	struct sys_shmget_args /* {
		syscallarg(key_t) key;
		syscallarg(size_t) size;
		syscallarg(int) shmflg;
	} */ shmget_args;

	SCARG(&shmget_args, key) = SCARG(uap, key);
	SCARG(&shmget_args, size) = (size_t)SCARG(uap, size);
	SCARG(&shmget_args, shmflg) = SCARG(uap, shmflg);

	return (sys_shmget(p, &shmget_args, retval));
}
#endif

#ifdef SYSVSEM
/*
 * Old-style shmget(2) used u_int for the nsops parameter, we now use size_t.
 */
int
compat_35_sys_semop(struct proc *p, void *v, register_t *retval)
{
	struct compat_35_sys_semop_args /* {
		syscallarg(int) semid;
		syscallarg(struct sembuf *) sops;
		syscallarg(u_int) nsops;
	} */ *uap = v;
	struct sys_semop_args /* {
		syscallarg(int) semid;
		syscallarg(struct sembuf *) sops;
		syscallarg(size_t) nsops;
	} */ semop_args;

	SCARG(&semop_args, semid) = SCARG(uap, semid);
	SCARG(&semop_args, sops) = SCARG(uap, sops);
	SCARG(&semop_args, nsops) = (size_t)SCARG(uap, nsops);

	return (sys_semop(p, &semop_args, retval));
}
#endif
