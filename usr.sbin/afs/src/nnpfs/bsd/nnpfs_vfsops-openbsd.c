/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors
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

#include <nnpfs/nnpfs_locl.h>

RCSID("$arla: nnpfs_vfsops-openbsd.c,v 1.16 2003/06/02 18:26:50 lha Exp $");

#include <nnpfs/nnpfs_common.h>
#include <nnpfs/nnpfs_message.h>
#include <nnpfs/nnpfs_fs.h>
#include <nnpfs/nnpfs_dev.h>
#include <nnpfs/nnpfs_deb.h>
#include <nnpfs/nnpfs_vfsops.h>
#include <nnpfs/nnpfs_vfsops-bsd.h>
#include <nnpfs/nnpfs_vnodeops.h>

static vop_t **nnpfs_dead_vnodeop_p;

int
nnpfs_make_dead_vnode(struct mount *mp, struct vnode **vpp)
{
    NNPFSDEB(XDEBNODE, ("make_dead_vnode mp = %lx\n",
		      (unsigned long)mp));

    return getnewvnode(VT_NON, mp, nnpfs_dead_vnodeop_p, vpp);
}

static struct vnodeopv_entry_desc nnpfs_dead_vnodeop_entries[] = {
    {&vop_default_desc, (vop_t *) nnpfs_eopnotsupp},
    {&vop_lookup_desc,	(vop_t *) nnpfs_dead_lookup},
    {&vop_reclaim_desc, (vop_t *) nnpfs_returnzero},
    {&vop_lock_desc,	(vop_t *) vop_generic_lock},
    {&vop_unlock_desc,	(vop_t *) vop_generic_unlock},
    {&vop_islocked_desc,(vop_t *) vop_generic_islocked},
    {NULL, NULL}};

static struct vnodeopv_desc nnpfs_dead_vnodeop_opv_desc =
{&nnpfs_dead_vnodeop_p, nnpfs_dead_vnodeop_entries};

extern struct vnodeopv_desc nnpfs_vnodeop_opv_desc;

static int
nnpfs_init(struct vfsconf *vfs)
{
    NNPFSDEB(XDEBVFOPS, ("nnpfs_init\n"));
    vfs_opv_init_explicit(&nnpfs_vnodeop_opv_desc);
    vfs_opv_init_default(&nnpfs_vnodeop_opv_desc);
    vfs_opv_init_explicit(&nnpfs_dead_vnodeop_opv_desc);
    vfs_opv_init_default(&nnpfs_dead_vnodeop_opv_desc);
    return 0;
}

struct vfsops nnpfs_vfsops = {
#ifdef HAVE_STRUCT_VFSOPS_VFS_MOUNT
    nnpfs_mount_common,
#else
    nnpfs_mount_caddr,
#endif
    nnpfs_start,
    nnpfs_unmount,
    nnpfs_root,
    nnpfs_quotactl,
    nnpfs_statfs,
    nnpfs_sync,
    nnpfs_vget,
    nnpfs_fhtovp,
    nnpfs_vptofh,
    nnpfs_init,
    NULL,
#ifdef HAVE_STRUCT_VFSOPS_VFS_CHECKEXP
    nnpfs_checkexp,               /* checkexp */
#endif
};

static struct vfsconf nnpfs_vfc = {
    &nnpfs_vfsops,
    "nnpfs",
    0,
    0,
    0,
    NULL,
    NULL
};

#ifndef HAVE_KERNEL_VFS_REGISTER

static int
vfs_register (struct vfsconf *vfs)
{
    struct vfsconf *vfsp;
    struct vfsconf **vfspp;

    /* Check if filesystem already known */
    for (vfspp = &vfsconf, vfsp = vfsconf;
	 vfsp;
	 vfspp = &vfsp->vfc_next, vfsp = vfsp->vfc_next)
	if (strcmp(vfsp->vfc_name, vfs->vfc_name) == 0)
	    return (EEXIST);

    maxvfsconf++;

    /* Add to the end of the list */
    *vfspp = vfs;

    vfs->vfc_next = NULL;

    /* Call vfs_init() */
    NNPFSDEB(XDEBVFOPS, ("calling vfs_init\n"));
    (*(vfs->vfc_vfsops->vfs_init)) (vfs);

    /* done! */

    return 0;
}

static int
vfs_unregister (struct vfsconf *vfs)
{
    struct vfsconf *vfsp;
    struct vfsconf **vfspp;

    /* Find our vfsconf struct */
    for (vfspp = &vfsconf, vfsp = vfsconf;
	 vfsp;
	 vfspp = &vfsp->vfc_next, vfsp = vfsp->vfc_next)
	if (strcmp(vfsp->vfc_name, vfs->vfc_name) == 0)
	    break;

    if (!vfsp)			       /* Not found */
	return (ENOENT);

    if (vfsp->vfc_refcount)	       /* In use */
	return (EBUSY);

    /* Remove from list and free  */
    *vfspp = vfsp->vfc_next;

    maxvfsconf--;

    return 0;
}

#endif

int
nnpfs_install_filesys(void)
{
    return vfs_register (&nnpfs_vfc);
}

int
nnpfs_uninstall_filesys(void)
{
    return vfs_unregister (&nnpfs_vfc);
}

int
nnpfs_stat_filesys (void)
{
    return 0;
}
