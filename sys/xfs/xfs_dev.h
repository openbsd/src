/*	$OpenBSD: xfs_dev.h,v 1.2 1998/08/31 05:13:23 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 *
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $KTH: xfs_dev.h,v 1.4 1998/04/05 18:19:50 art Exp $ */

#ifndef _XFS_XFS_DEV_H_
#define _XFS_XFS_DEV_H_

int xfs_devopen __P((dev_t dev, int flags, int devtype, struct proc * p));

int xfs_install_device __P((void));
int xfs_uninstall_device __P((void));

int xfs_install_filesys __P((void));
int xfs_uninstall_filesys __P((void));

int xfs_stat_filesys __P((void));
int xfs_stat_device __P((void));

int xfs_message_send __P((int fd, struct xfs_message_header *message,
			  u_int size));
int xfs_message_rpc __P((int fd, struct xfs_message_header *message,
			 u_int size));
int xfs_message_receive __P((int fd,struct xfs_message_header *message,
			     u_int size,struct proc *p));
int xfs_message_wakeup __P((int fd, struct xfs_message_wakeup *message,
			    u_int size, struct proc *p));
int xfs_message_wakeup_data __P((int fd,
				 struct xfs_message_wakeup_data *message,
				 u_int size, struct proc *p));
#define USE_SELECT

#endif

