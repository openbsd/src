/*	$OpenBSD: linux_shm.h,v 1.3 2011/04/05 22:54:31 pirofti Exp $	*/
/*	$NetBSD: linux_shm.h,v 1.1 1995/02/28 23:25:57 fvdl Exp $	*/

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

#ifndef _LINUX_SHM_H_
#define _LINUX_SHM_H_

/*
 * shm segment control structure
 */
struct linux_shmid_ds {
	struct linux_ipc_perm	l_shm_perm;
	int			l_shm_segsz;
	linux_time_t		l_shm_atime;
	linux_time_t		l_shm_dtime;
	linux_time_t		l_shm_ctime;
	ushort			l_shm_cpid;
	ushort			l_shm_lpid;
	short			l_shm_nattch;
	ushort			l_private1;
	void			*l_private2;
	void			*l_private3;
};

#define LINUX_SHM_RDONLY	0x1000
#define LINUX_SHM_RND		0x2000
#define LINUX_SHM_REMAP		0x4000

#define LINUX_SHM_LOCK		11
#define LINUX_SHM_UNLOCK	12
#define LINUX_SHM_STAT		13
#define LINUX_SHM_INFO		14

#endif /* _LINUX_SHM_H_ */
