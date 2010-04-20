/* 	$OpenBSD: compat_aout.c,v 1.4 2010/04/20 22:05:41 tedu Exp $ */

/*
 * Copyright (c) 2003 Marc Espie
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
 *
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/signalvar.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/fcntl.h>
#include <sys/core.h>
#include <compat/common/compat_util.h>

void aout_compat_setup(struct exec_package *epp);

extern char sigcode[], esigcode[];

struct sysent aout_sysent[SYS_MAXSYSCALL];

struct emul emul_aout = {
	"aout",
	NULL,
	sendsig,
	SYS_syscall,
	SYS_MAXSYSCALL,
	NULL,
#ifdef SYSCALL_DEBUG
	syscallnames,
#else
	NULL,
#endif
	0,
	copyargs,
	setregs,
	NULL,
	coredump_trad,
	sigcode,
	esigcode,
};

#ifdef	syscallarg
#undef	syscallarg
#endif

#define	syscallarg(x)							\
	union {								\
		register_t pad;						\
		struct { x datum; } le;					\
		struct {						\
			int8_t pad[ (sizeof (register_t) < sizeof (x))	\
				? 0					\
				: sizeof (register_t) - sizeof (x)];	\
			x datum;					\
		} be;							\
	}


struct aout_sys_open_args {
	syscallarg(char *) path;
	syscallarg(int) flags;
	syscallarg(int) mode;
};

struct aout_sys_link_args {
	syscallarg(char *) path;
	syscallarg(char *) link;
};

struct aout_sys_unlink_args {
	syscallarg(char *) path;
};

struct aout_sys_rename_args {
	syscallarg(char *) from;
	syscallarg(char *) to;
};

int aout_sys_open(struct proc *, void *, register_t *);
int aout_sys_link(struct proc *, void *, register_t *);
int aout_sys_unlink(struct proc *, void *, register_t *);
int aout_sys_rename(struct proc *, void *, register_t *);

const char aout_path[] = "/emul/a.out";

#define AOUT_CHECK_ALT_EXIST(p, sgp, path) \
    CHECK_ALT_EXIST(p, sgp, aout_path, path)

#define  AOUT_CHECK_ALT_CREAT(p, sgp, path) \
    CHECK_ALT_CREAT(p, sgp, aout_path, path)

/* XXX We just translate enough calls to allow ldconfig and ld.so to work. */

void
aout_compat_setup(struct exec_package *epp)
{
	if (emul_aout.e_sysent == NULL) {
		memcpy(aout_sysent, sysent, sizeof aout_sysent); 
		aout_sysent[SYS_open].sy_call = aout_sys_open;
		aout_sysent[SYS_link].sy_call = aout_sys_link;
		aout_sysent[SYS_unlink].sy_call = aout_sys_unlink;
		aout_sysent[SYS_rename].sy_call = aout_sys_rename;
		emul_aout.e_sysent = aout_sysent;
	}
	epp->ep_emul = &emul_aout;
}

int
aout_sys_open(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct aout_sys_open_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	if (SCARG(uap, flags) & O_CREAT)
		AOUT_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));
	else
		AOUT_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_open(p, uap, retval);
}

int
aout_sys_link(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct aout_sys_link_args /* {
		syscallarg(char *) path;
		syscallarg(char *) link;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	AOUT_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	AOUT_CHECK_ALT_CREAT(p, &sg, SCARG(uap, link));
	return sys_link(p, uap, retval);
}

int
aout_sys_unlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct aout_sys_unlink_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	AOUT_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_unlink(p, uap, retval);
}


int
aout_sys_rename(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct aout_sys_rename_args /* {
		syscallarg(char *) from;
		syscallarg(char *) to;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	AOUT_CHECK_ALT_EXIST(p, &sg, SCARG(uap, from));
	AOUT_CHECK_ALT_CREAT(p, &sg, SCARG(uap, to));
	return sys_rename(p, uap, retval);
}
