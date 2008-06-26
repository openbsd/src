/*	$OpenBSD: svr4_types.h,v 1.7 2008/06/26 05:42:14 ray Exp $	*/
/*	$NetBSD: svr4_types.h,v 1.11 1998/09/11 12:34:46 mycroft Exp $	*/

/*-
 * Copyright (c) 1994 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef	_SVR4_TYPES_H_
#define	_SVR4_TYPES_H_

typedef u_quad_t	 svr4_ino64_t;
typedef quad_t		 svr4_off64_t;
typedef quad_t		 svr4_blkcnt64_t;
typedef u_quad_t	 svr4_fsblkcnt64_t;

typedef long  		 svr4_off_t;
typedef u_long		 svr4_dev_t;
typedef u_long		 svr4_ino_t;
typedef u_long		 svr4_mode_t;
typedef u_long		 svr4_nlink_t;
typedef long		 svr4_uid_t;
typedef long		 svr4_gid_t;
typedef int32_t		 svr4_daddr_t;
typedef long		 svr4_pid_t;
typedef long		 svr4_time_t;
typedef long		 svr4_blkcnt_t;
typedef u_long		 svr4_fsblkcnt_t;
typedef char		*svr4_caddr_t;
typedef u_int		 svr4_size_t;

typedef short		 svr4_o_dev_t;
typedef short		 svr4_o_pid_t;
typedef u_short		 svr4_o_ino_t;
typedef u_short		 svr4_o_mode_t;
typedef short		 svr4_o_nlink_t;
typedef u_short		 svr4_o_uid_t;
typedef u_short		 svr4_o_gid_t;
typedef long		 svr4_clock_t;
typedef int		 svr4_key_t;

typedef struct timespec  svr4_timestruc_t;

#define	svr4_omajor(x)		((int32_t)((((x) & 0x7f00) >> 8)))
#define	svr4_ominor(x)		((int32_t)((((x) & 0x00ff) >> 0)))
#define	svr4_omakedev(x,y)	((svr4_o_dev_t)((((x) << 8) & 0x7f00) | \
						(((y) << 0) & 0x00ff)))
#define svr4_to_bsd_odev_t(d)	makedev(svr4_omajor(d), svr4_ominor(d))
#define bsd_to_svr4_odev_t(d)	svr4_omakedev(major(d), minor(d))

#define	svr4_major(x)		((int32_t)((((x) & 0xfffc0000) >> 18)))
#define	svr4_minor(x)		((int32_t)((((x) & 0x0003ffff) >>  0)))
#define	svr4_makedev(x,y)	((svr4_dev_t)((((x) << 18) & 0xfffc0000) | \
					      (((y) <<  0) & 0x0003ffff)))
#define svr4_to_bsd_dev_t(d)	makedev(svr4_major(d), svr4_minor(d))
#define bsd_to_svr4_dev_t(d)	svr4_makedev(major(d), minor(d))

#endif /* !_SVR4_TYPES_H_ */
