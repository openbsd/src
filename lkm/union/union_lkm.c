/*	$OpenBSD: union_lkm.c,v 1.3 2002/02/17 19:42:26 millert Exp $	*/

/*
 * union_lkm.c (was: kernfsmod.c)
 *
 * 05 Jun 93	Terry Lambert		Original
 * 20 Jan 97	Michael Shalayeff	NetBSD PR
 *					by Paul Goyette <paul@pgoyette.bdt.com>
 *
 * Copyright (c) 1993 Terrence R. Lambert.
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
 *      This product includes software developed by Terrence R. Lambert.
 * 4. The name Terrence R. Lambert may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TERRENCE R. LAMBERT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE TERRENCE R. LAMBERT BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from Id: kernfsmod.c,v 1.1.1.1 1995/10/18 08:44:22 deraadt Exp
 */
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/lkm.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/namei.h>

/*
 * This is the vfsops table from /sys/miscfs/union/union_vfsops.c
 */
extern struct vfsops union_vfsops;
extern struct vnodeopv_desc union_vnodeop_opv_desc;

extern int (*union_check_p)(off_t, struct proc *, struct vnode *, 
	    struct file *, struct uio, int *);
extern int union_check(off_t, struct proc *, struct vnode *, struct file *,
	    struct uio, int *);
static int union_load(struct lkm_table *, int);

/*
 * declare the filesystem
 */
MOD_VFS("union", -1, &union_vfsops)

/*
 * Routine to execute when module is loaded or unloaded.  We need to set
 * a couple of critical pointers for the vfs_syscalls routines.
 */
static int
union_load(lkmtp, cmd)
	struct lkm_table *lkmtp;
	int cmd;
{
	switch (cmd) {
	case LKM_E_LOAD:
		union_check_p = union_check;
		break;

	case LKM_E_UNLOAD:
		union_check_p = NULL;
		break;
	};
	return (0);
}

/*
 * LKM entry point
 * (named this way to avoid keyword collision)
 */
int
unionfs(lkmtp, cmd, ver)
	struct lkm_table *lkmtp;	
	int cmd;
	int ver;
{

	/*
	 * This is normally done automatically at boot time if the
	 * opv_desc is listed in vfs_opv_descs[] in vfs_conf.c.  For
	 * loaded modules, we have to do it manually.
	 */
	vfs_opv_init_explicit(&union_vnodeop_opv_desc);
	vfs_opv_init_default(&union_vnodeop_opv_desc);

	DISPATCH(lkmtp, cmd, ver, union_load, union_load, lkm_nofunc)
}


