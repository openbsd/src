/*	$OpenBSD: linux_ipccall.h,v 1.3 2011/04/05 22:54:30 pirofti Exp $	*/
/*	$NetBSD: linux_ipccall.h,v 1.2 1995/08/15 21:14:33 fvdl Exp $	*/

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

#ifndef _LINUX_IPC_CALL_H_
#define _LINUX_IPC_CALL_H_

/*
 * Defines for the numbers passes as the first argument to the
 * linux_ipc() call, and based on which the actual system calls
 * are made.
 */
#define LINUX_SYS_semop		1
#define LINUX_SYS_semget	2
#define LINUX_SYS_semctl	3
#define LINUX_SYS_msgsnd	11
#define LINUX_SYS_msgrcv	12
#define LINUX_SYS_msgget	13
#define LINUX_SYS_msgctl	14
#define LINUX_SYS_shmat		21
#define LINUX_SYS_shmdt		22
#define LINUX_SYS_shmget	23
#define LINUX_SYS_shmctl	24

#endif /* _LINUX_IPC_CALL_H_ */
