/*	$OpenBSD: linux_sem.h,v 1.2 1996/04/17 05:23:57 mickey Exp $	*/
/*	$NetBSD: linux_sem.h,v 1.1 1995/08/15 21:14:35 fvdl Exp $	*/

/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
 * 4. The name of the author may not be used to endorse or promote products
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
 */

#ifndef _LINUX_SEM_H
#define _LINUX_SEM_H

/*
 * Operations for semctl(2), in addition to IPC_STAT and IPC_SET
 */
#define LINUX_GETPID  11
#define LINUX_GETVAL  12
#define LINUX_GETALL  13
#define LINUX_GETNCNT 14
#define LINUX_GETZCNT 15
#define LINUX_SETVAL  16
#define LINUX_SETALL  17

/*
 * Linux semid_ds structure. Internally used pointer fields are not
 * important to us and have been changed to void *
 */

struct linux_semid_ds {
	struct linux_ipc_perm	 l_sem_perm;
	linux_time_t		 l_sem_otime;
	linux_time_t		 l_sem_ctime;
	void 			*l_sem_base;
	void			*l_eventn;
	void			*l_eventz;
	void			*l_undo;
	ushort			 l_sem_nsems;
};

union linux_semun {
	int			 l_val;
	struct linux_semid_ds	*l_buf;
	ushort			*l_array;
	void			*l___buf;	/* For unsupported IPC_INFO */
	void			*l___pad;
};

#endif /* _LINUX_SEM_H */
