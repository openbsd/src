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

RCSID("$arla: nnpfs_syscalls-wrap-bsd.c,v 1.18 2003/01/19 20:53:50 lha Exp $");

/*
 * NNPFS system calls.
 */

#include <nnpfs/nnpfs_syscalls.h>
#include <nnpfs/nnpfs_message.h>
#include <nnpfs/nnpfs_fs.h>
#include <nnpfs/nnpfs_dev.h>
#include <nnpfs/nnpfs_node.h>
#include <nnpfs/nnpfs_deb.h>

#include <kafs.h>

int nnpfs_syscall_num;

static struct sysent old_syscallent;

static struct sysent nnpfs_syscallent = {
    5,				       /* number of args */
    sizeof(struct sys_pioctl_args),    /* size of args */
#if HAVE_STRUCT_SYSENT_SY_FLAGS
    0,
#endif
    nnpfspioctl			       /* function pointer */
};

static struct sysent old_setgroups;

/* XXX really defined in kern/kern_lkm.c */
extern int
sys_lkmnosys(syscall_d_thread_t *p, void *v, register_t *retval);

/*
 *
 */

#ifdef HAVE_KERNEL_AOUT_SYSENT
/* XXX if we are running with a.out compatibility, we need to add all
   syscalls to both the ELF, and the a.out syscall tables */
/* XXX this should be made generic, so we can add support for, say,
   linux afs binaries */
extern struct sysent aout_sysent[];
#endif

static int
try_install_syscall (int offset,
		     struct sysent new_sysent,
		     struct sysent *old_sysent)
{
    if(sysent[offset].sy_call != sys_lkmnosys)
#if defined(__OpenBSD__) && defined(AFS_SYSCALL)
	/* XXX
	 * OpenBSD puts a dummy pointer at AFS_SYSCALL,
	 * ignore it - we're handling AFS now.
	 */
	if (offset != AFS_SYSCALL)
	    return EBUSY;
#else
	    return EBUSY;
#endif

#ifdef HAVE_KERNEL_AOUT_SYSENT
    if(aout_sysent[offset].sy_call != sys_lkmnosys)
	return EBUSY;
#endif
    
    *old_sysent = sysent[offset];
    sysent[offset] = new_sysent;
#ifdef HAVE_KERNEL_AOUT_SYSENT
    aout_sysent[offset] = new_sysent;
#endif
    return 0;
}

/*
 *
 */

static int
install_first_free_syscall (int *offset,
			    struct sysent sysent,
			    struct sysent *old_sysent)
{
    int i;

    for (i = 1; i < SYS_MAXSYSCALL; ++i)
	if (try_install_syscall (i, sysent, old_sysent) == 0) {
	    *offset = i;
	    return 0;
	}
    return ENFILE;
}

/*
 * Try AFS_SYSCALL first, if that fails, any free slot
 */

int
nnpfs_install_syscalls(void)
{
    int ret = ENOENT;

#ifdef HAVE_KERNEL_AOUT_SYSENT
    /* XXX make sure that the ELF, and the a.out syscall are the same;
       if they're not we don't know what to do (infact you could just
       add two different setgroups calls, but this is probably not
       worth the trouble */

    if(sysent[SYS_setgroups].sy_call != 
       aout_sysent[SYS_setgroups].sy_call){
	printf("%s: ELF and a.out setgroups syscalls differ!\n", 
	       __FUNCTION__);
	return ret;
    }
#endif
#ifdef AFS_SYSCALL
    if (ret != 0) {
	ret = try_install_syscall(AFS_SYSCALL,
				  nnpfs_syscallent,
				  &old_syscallent);
	if (ret == 0)
	    nnpfs_syscall_num = AFS_SYSCALL;
    }
#endif
    if (ret != 0)
	ret = install_first_free_syscall (&nnpfs_syscall_num,
					  nnpfs_syscallent,
					  &old_syscallent);
    if (ret != 0)
	NNPFSDEB(XDEBSYS, ("failed installing nnpfs_syscall\n"));
    if (ret == 0) {
	old_setgroups = sysent[SYS_setgroups];
	old_setgroups_func = old_setgroups.sy_call;
	sysent[SYS_setgroups].sy_call = nnpfs_setgroups;
#ifdef HAVE_KERNEL_AOUT_SYSENT
	aout_sysent[SYS_setgroups].sy_call = nnpfs_setgroups;
#endif
    }
    return ret;
}

int
nnpfs_uninstall_syscalls(void)
{
    if (nnpfs_syscall_num) {
	sysent[nnpfs_syscall_num] = old_syscallent;
	sysent[SYS_setgroups] = old_setgroups;
#ifdef HAVE_KERNEL_AOUT_SYSENT
	aout_sysent[nnpfs_syscall_num] = old_syscallent;
	aout_sysent[SYS_setgroups] = old_setgroups;
#endif
    }
    return 0;
}

int
nnpfs_stat_syscalls(void)
{
    return 0;
}
