/*	$OpenBSD: xfs_vfsops-openbsd.c,v 1.3 2000/03/03 00:54:59 todd Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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

#include <xfs/xfs_locl.h>

RCSID("$OpenBSD: xfs_vfsops-openbsd.c,v 1.3 2000/03/03 00:54:59 todd Exp $");

#include <xfs/xfs_common.h>
#include <xfs/xfs_message.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_dev.h>
#include <xfs/xfs_deb.h>
#include <xfs/xfs_vfsops.h>
#include <xfs/xfs_vfsops-bsd.h>
#include <xfs/xfs_vnodeops.h>

static vop_t **xfs_dead_vnodeop_p;

int
make_dead_vnode(struct mount *mp, struct vnode **vpp)
{
    XFSDEB(XDEBNODE, ("make_dead_vnode mp = %p\n", mp));

    return getnewvnode(VT_NON, mp, xfs_dead_vnodeop_p, vpp);
}

static struct vnodeopv_entry_desc xfs_dead_vnodeop_entries[] = {
    {&vop_default_desc, (vop_t *) xfs_eopnotsupp},
    {&vop_lookup_desc,	(vop_t *) xfs_dead_lookup},
    {&vop_reclaim_desc, (vop_t *) xfs_returnzero},
    {NULL, NULL}};

static struct vnodeopv_desc xfs_dead_vnodeop_opv_desc =
{&xfs_dead_vnodeop_p, xfs_dead_vnodeop_entries};

extern struct vnodeopv_desc xfs_vnodeop_opv_desc;

static int
xfs_init(struct vfsconf *vfs)
{
    XFSDEB(XDEBVFOPS, ("xfs_init\n"));
    vfs_opv_init_explicit(&xfs_vnodeop_opv_desc);
    vfs_opv_init_default(&xfs_vnodeop_opv_desc);
    vfs_opv_init_explicit(&xfs_dead_vnodeop_opv_desc);
    vfs_opv_init_default(&xfs_dead_vnodeop_opv_desc);
    return 0;
}

static int
xfs_sysctl (int *name, u_int namelen, void *oldp, size_t *oldlenp,
	    void *newp, size_t newlen, struct proc *p)
{
    XFSDEB(XDEBVFOPS, ("xfs_sysctl\n"));
    return EOPNOTSUPP;
}

static int
xfs_checkexp(struct mount *mp, struct mbuf *nam, int *exflagsp,
	     struct ucred **credanonp)
{
    XFSDEB(XDEBVFOPS, ("xfs_checkexp\n"));
    return (EOPNOTSUPP);
}

struct vfsops xfs_vfsops = {
    xfs_mount,
    xfs_start,
    xfs_unmount,
    xfs_root,
    xfs_quotactl,
    xfs_statfs,
    xfs_sync,
    xfs_vget,
    xfs_fhtovp,
    xfs_vptofh,
    xfs_init,
    xfs_sysctl,
    xfs_checkexp
};

static struct vfsconf xfs_vfc = {
    &xfs_vfsops,
    "xfs",
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
    XFSDEB(XDEBVFOPS, ("calling vfs_init\n"));
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
xfs_install_filesys(void)
{
    return vfs_register (&xfs_vfc);
}

int
xfs_uninstall_filesys(void)
{
    return vfs_unregister (&xfs_vfc);
}

int
xfs_stat_filesys (void)
{
    return 0;
}
