/*	$NetBSD: ramdisk.h,v 1.2 1995/10/26 15:46:24 gwr Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross
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
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon W. Ross
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

/*
 * RAM-disk ioctl functions:
 */

#include <sys/ioccom.h>

struct rd_conf {
	caddr_t rd_addr;
	size_t  rd_size;
	int     rd_type;
};

#define RD_GETCONF	_IOR('r', 0, struct rd_conf)	/* get unit config */
#define RD_SETCONF	_IOW('r', 1, struct rd_conf)	/* set unit config */

/*
 * There are three configurations supported for each unit,
 * reflected in the value of the rd_type field:
 */
#define RD_UNCONFIGURED 0
/*
 *     Not yet configured.  Open returns ENXIO.
 */
#define RD_KMEM_FIXED	1
/*
 *     Disk image resident in kernel (patched in or loaded).
 *     Requires that the function: rd_set_kmem() is called to
 *     attach the (initialized) kernel memory to be used by the
 *     device.  It can be initialized by an "open hook" if this
 *     driver is compiled with the RD_OPEN_HOOK option.
 *     No attempt will ever be made to free this memory.
 */
#define RD_KMEM_ALLOCATED 2
/*
 *     Small, wired-down chunk of kernel memory obtained from
 *     kmem_alloc().  The allocation is performed by an ioctl
 *     call on the "control" unit (regular unit + 16)
 */
#define RD_UMEM_SERVER 3
/*
 *     Indirect access to user-space of a user-level server.
 *     (Like the MFS hack, but better! 8^)  Device operates
 *     only while the server has the control device open and
 *     continues to service I/O requests.  The process that
 *     does this setconf will become the I/O server.  This
 *     configuration type can be disabled using:
 *         option  RAMDISK_SERVER=0
 */

#ifdef	_KERNEL
/*
 * If the option RAMDISK_HOOKS is on, then these functions are
 * called by the ramdisk driver to allow machine-dependent to
 * configure and/or load each ramdisk unit.
 */
extern void rd_attach_hook __P((int unit, struct rd_conf *));
extern void rd_open_hook   __P((int unit, struct rd_conf *));
#endif
