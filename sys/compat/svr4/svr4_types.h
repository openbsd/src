/*	$NetBSD: svr4_types.h,v 1.7 1995/10/14 20:25:04 christos Exp $	 */

/*
 * Copyright (c) 1994 Christos Zoulas
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
 */

#ifndef	_SVR4_TYPES_H_
#define	_SVR4_TYPES_H_

typedef long    svr4_off_t;
typedef u_long  svr4_dev_t;
typedef u_long  svr4_ino_t;
typedef u_long  svr4_mode_t;
typedef u_long  svr4_nlink_t;
typedef long    svr4_uid_t;
typedef long    svr4_gid_t;
typedef long    svr4_daddr_t;
typedef long    svr4_pid_t;
typedef long    svr4_time_t;
typedef char   *svr4_caddr_t;
typedef u_int   svr4_size_t;

typedef short   svr4_o_dev_t;
typedef short   svr4_o_pid_t;
typedef u_short svr4_o_ino_t;
typedef u_short svr4_o_mode_t;
typedef short   svr4_o_nlink_t;
typedef u_short svr4_o_uid_t;
typedef u_short svr4_o_gid_t;
typedef long	svr4_clock_t;
typedef int	svr4_key_t;

typedef struct timespec svr4_timestruc_t;

#endif /* !_SVR4_TYPES_H_ */
