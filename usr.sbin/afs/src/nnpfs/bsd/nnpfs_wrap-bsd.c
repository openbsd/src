/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
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
#include <nnpfs/nnpfs_message.h>
#include <nnpfs/nnpfs_fs.h>
#include <nnpfs/nnpfs_dev.h>
#include <nnpfs/nnpfs_syscalls.h>
#include <nnpfs/nnpfs_deb.h>
#include <nnpfs/nnpfs_wrap.h>

RCSID("$arla: nnpfs_wrap-bsd.c,v 1.46 2003/02/04 10:28:16 lha Exp $");

#include "version.h"

int nnpfs_dev_major;

/*
 * Iff `dev' represents a valid nnpfs device.
 */

int
nnpfs_is_nnpfs_dev (dev_t dev)
{
    return major (dev) == nnpfs_dev_major
	&& minor(dev) >= 0 && minor(dev) < NNNPFS;
}

static int
nnpfs_uninstall(void)
{
    int err, ret = 0;

    if ((err = nnpfs_uninstall_filesys()) != 0)
	ret = err;
    if ((err = nnpfs_uninstall_device()) != 0)
	ret = err;
    if ((err = nnpfs_uninstall_syscalls()) != 0)
	ret = err;

#if defined(__NetBSD__) && __NetBSD_Version__ >= 106140000
    malloc_type_detach(M_NNPFS);
    malloc_type_detach(M_NNPFS_LINK);
    malloc_type_detach(M_NNPFS_MSG);
    malloc_type_detach(M_NNPFS_NODE);
#endif

    return ret;
}

static int
nnpfs_install(void)
{
    int err = 0;

#if defined(__NetBSD__) && __NetBSD_Version__ >= 106140000
    malloc_type_attach(M_NNPFS);
    malloc_type_attach(M_NNPFS_LINK);
    malloc_type_attach(M_NNPFS_MSG);
    malloc_type_attach(M_NNPFS_NODE);
#endif

    if ((err = nnpfs_install_device()) ||
	(err = nnpfs_install_syscalls()) ||
	(err = nnpfs_install_filesys()))
	nnpfs_uninstall();
    return err;
}

static int
nnpfs_stat(void)
{
    int err = 0;

    if ((err = nnpfs_stat_device()) != 0)
	return err;
    else if ((err = nnpfs_stat_syscalls()) != 0)
	return err;
    else if ((err = nnpfs_stat_filesys()) != 0)
	return err;

    return err;
}

extern struct cdevsw nnpfs_dev;

/*
 * This is to build a kld module (FreeBSD3.0 and later, but we only
 * support FreeBSD 4.1 and later)
 */

#if KLD_MODULE

static void
make_devices (struct cdevsw *devsw)
{
    int i;

    for (i = 0; i < NNNPFS; ++i)
	make_dev (devsw, i, UID_ROOT, GID_WHEEL, 0600, "nnpfs%d", i);
}

static void
destroy_devices (struct cdevsw *devsw)
{
    int i;

    for (i = 0; i < NNNPFS; ++i)
	destroy_dev (makedev (devsw->d_maj, i));
}

/*
 *
 */

static int
nnpfs_load(struct module *mod, int cmd, void *arg)
{
    int ret;

    NNPFSDEB(XDEBLKM, ("nnpfs_load\n"));

    switch (cmd) {
    case MOD_LOAD :
	ret = nnpfs_install ();
	if (ret == 0) {
	    make_devices (&nnpfs_dev);
	    nnpfs_dev_major = nnpfs_dev.d_maj;
	    printf ("nnpfs: cdev: %d, syscall: %d\n",
		    nnpfs_dev_major, nnpfs_syscall_num);
	}
	break;
    case MOD_UNLOAD :
	ret = nnpfs_uninstall ();
	if (ret == 0) {
	    destroy_devices (&nnpfs_dev);
	}
	break;
    default :
	ret = EINVAL;
	break;
    }
    NNPFSDEB(XDEBLKM, ("nnpfs_load = %d\n", ret));
    return ret;
}

extern struct vfsops nnpfs_vfsops;

extern struct sysent nnpfs_syscallent;

VFS_SET(nnpfs_vfsops, nnpfs, 0);

DEV_MODULE(arlannpfsdev, nnpfs_load, NULL);

#ifdef MODULE_VERSION
MODULE_VERSION(arlannpfsdev,1);
#endif /* MODULE_VERSION */

#else /* KLD_MODULE */

/*
 * An ordinary lkm-module
 */

#if __NetBSD_Version__ >= 106080000
MOD_DEV("nnpfs_mod","nnpfs_mod", NULL, -1, &nnpfs_dev, -1)
#elif !defined(__APPLE__)
MOD_DEV("nnpfs_mod",LM_DT_CHAR,-1,&nnpfs_dev)
#endif

/*
 *
 */

#if defined(__APPLE__)
__private_extern__ kern_return_t
nnpfs_modload(kmod_info_t *ki, void *data)
#else
static int
nnpfs_modload(struct lkm_table *lkmtp, int cmd)
#endif
{
    int error = 0;

    NNPFSDEB(XDEBLKM, ("nnpfs_modload\n"));

    error = nnpfs_install();
    if (error == 0)
	nnpfs_stat();
    return error;
}


/*
 *
 */

#if defined(__APPLE__)
__private_extern__ kern_return_t
nnpfs_modunload(kmod_info_t *ki, void *data)
#else
static int
nnpfs_modunload(struct lkm_table * lkmtp, int cmd)
#endif
{
    int error = 0;

    NNPFSDEB(XDEBLKM, ("nnpfs_modunload\n"));

    error = nnpfs_uninstall();
    if (!error)
	NNPFSDEB(XDEBLKM, ("nnpfs_modunload: successful\n"));
    else
	NNPFSDEB(XDEBLKM, ("nnpfs_modunload: unsuccessful, system unstable\n"));
    return error;
}

/*
 *
 */

#if !defined(__APPLE__)
static int
nnpfs_modstat(struct lkm_table * lkmtp, int cmd)
{
    int error = 0;

    NNPFSDEB(XDEBLKM, ("nnpfs_modstat\n"));

    error = nnpfs_stat();
    return error;
}

int
nnpfs_mod(struct lkm_table * lkmtp, int cmd, int ver);

int
nnpfs_mod(struct lkm_table * lkmtp,
	int cmd,
	int ver)
{
    int ret;
    int cdevmaj;

    if (ver != LKM_VERSION)						
	return EINVAL;	/* version mismatch */			
    switch (cmd) {							
	int	error;							
    case LKM_E_LOAD:
	lkmtp->private.lkm_any = (struct lkm_any *)&_module;
	if ((error = nnpfs_modload(lkmtp, cmd)) != 0)
	    return error;
	break;
    case LKM_E_UNLOAD:
	if ((error = nnpfs_modunload(lkmtp, cmd)) != 0)
	    return error;
	break;
    case LKM_E_STAT:
	if ((error = nnpfs_modstat(lkmtp, cmd)) != 0)
	    return error;
	break;
    }
    ret = lkmdispatch(lkmtp, cmd);
    if(cmd == LKM_E_LOAD) {
#if __NetBSD_Version__ >= 106080000
	nnpfs_dev_major = _module.lkm_cdevmaj;
#else
	nnpfs_dev_major = _module.lkm_offset;
#endif
	printf ("nnpfs (%s): "
		"protocol version %d, using cdev: %d, syscall: %d\n",
		arla_version, NNPFS_VERSION,
		nnpfs_dev_major, nnpfs_syscall_num);
    }
    return ret;
}
#endif
    
#endif /* KLD_MODULE */

