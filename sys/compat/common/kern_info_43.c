/*	$OpenBSD: kern_info_43.c,v 1.16 2011/03/12 04:54:28 guenther Exp $	*/
/*	$NetBSD: kern_info_43.c,v 1.5 1996/02/04 02:02:22 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)subr_xxx.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/unistd.h>
#include <sys/resourcevar.h>
#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

int
compat_43_sys_getdtablesize(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	*retval = min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfiles);
	return (0);
}


/* ARGSUSED */
int
compat_43_sys_gethostid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	*(int32_t *)retval = hostid;
	return (0);
}


/*ARGSUSED*/
int
compat_43_sys_gethostname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_sys_gethostname_args /* {
		syscallarg(char *) hostname;
		syscallarg(u_int) len;
	} */ *uap = v;
	int name;
	size_t sz;

	name = KERN_HOSTNAME;
	sz = SCARG(uap, len);
	return (kern_sysctl(&name, 1, SCARG(uap, hostname), &sz, 0, 0, p));
}

#define	KINFO_PROC		(0<<8)
#define	KINFO_RT		(1<<8)
#define	KINFO_VNODE		(2<<8)
#define	KINFO_FILE		(3<<8)
#define	KINFO_METER		(4<<8)
#define	KINFO_LOADAVG		(5<<8)
#define	KINFO_CLOCKRATE		(6<<8)
#define	KINFO_BSDI_SYSINFO	(101<<8)


/*
 * The string data is appended to the end of the bsdi_si structure during
 * copyout. The "char *" offsets in the bsdi_si struct are relative to the
 * base of the bsdi_si struct. 
 */
struct bsdi_si {
        char    *machine;
        char    *cpu_model;
        long    ncpu;
        long    cpuspeed;
        long    hwflags;
        u_long  physmem;
        u_long  usermem;
        u_long  pagesize;

        char    *ostype;
        char    *osrelease;
        long    os_revision;
        long    posix1_version;
        char    *version;

        long    hz;
        long    profhz;
        int     ngroups_max;
        long    arg_max;
        long    open_max;
        long    child_max;

        struct  timeval boottime;
        char    *hostname;
};

/* Non-standard BSDI extension - only present on their 4.3 net-2 releases */
#define       KINFO_BSDI_SYSINFO      (101<<8)

/*
 * XXX this is bloat, but I hope it's better here than on the potentially
 * limited kernel stack...  -Peter
 */

struct {      
	char    *bsdi_machine;          /* "i386" on BSD/386 */
	char    *pad0;
	long    pad1;
	long    pad2;
	long    pad3;
	u_long  pad4;
	u_long  pad5;
	u_long  pad6;

	char    *bsdi_ostype;           /* "BSD/386" on BSD/386 */
	char    *bsdi_osrelease;        /* "1.1" on BSD/386 */
	long    pad7;   
	long    pad8;
	char    *pad9;

	long    pad10;
	long    pad11;  
	int     pad12;
	long    pad13; 
	quad_t  pad14; 
	long    pad15;

	struct  timeval pad16;
	/* we dont set this, because BSDI's uname used gethostname() instead */
	char    *bsdi_hostname;         /* hostname on BSD/386 */

	/* the actual string data is appended here */

} bsdi_si;
/*
 * this data is appended to the end of the bsdi_si structure during copyout.
 * The "char *" offsets are relative to the base of the bsdi_si struct.
 * This contains "OpenBSD\01.2-BUILT-nnnnnn\0i386\0", and these strings
 * should not exceed the length of the buffer here... (or else!! :-)
 */
char bsdi_strings[80];        /* It had better be less than this! */

int
compat_43_sys_getkerninfo(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_43_sys_getkerninfo_args /* {
		syscallarg(int) op;
		syscallarg(char *) where;
		syscallarg(int *) size;
		syscallarg(int) arg;
	} */ *uap = v;
	int error, name[5];
	size_t size;

	extern char machine[];

	if (SCARG(uap, size) && (error = copyin((caddr_t)SCARG(uap, size),
	    (caddr_t)&size, sizeof(size))))
		return (error);

	switch (SCARG(uap, op) & 0xff00) {

	case KINFO_RT:
		name[0] = PF_ROUTE;
		name[1] = 0;
		name[2] = (SCARG(uap, op) & 0xff0000) >> 16;
		name[3] = SCARG(uap, op) & 0xff;
		name[4] = SCARG(uap, arg);
		error =
		    net_sysctl(name, 5, SCARG(uap, where), &size, NULL, 0, p);
		break;

	case KINFO_VNODE:
		name[0] = KERN_VNODE;
		error =
		    kern_sysctl(name, 1, SCARG(uap, where), &size, NULL, 0, p);
		break;

	case KINFO_FILE:
		name[0] = KERN_FILE;
		error =
		    kern_sysctl(name, 1, SCARG(uap, where), &size, NULL, 0, p);
		break;

	case KINFO_METER:
		name[0] = VM_METER;
		error =
		    uvm_sysctl(name, 1, SCARG(uap, where), &size, NULL, 0, p);
		break;

	case KINFO_LOADAVG:
		name[0] = VM_LOADAVG;
		error =
		    uvm_sysctl(name, 1, SCARG(uap, where), &size, NULL, 0, p);
		break;

	case KINFO_CLOCKRATE:
		name[0] = KERN_CLOCKRATE;
		error =
		    kern_sysctl(name, 1, SCARG(uap, where), &size, NULL, 0, p);
		break;

	case KINFO_BSDI_SYSINFO: { 
		/*
		 * this is pretty crude, but it's just enough for uname()
		 * from BSDI's 1.x libc to work.
		 */

		u_int needed;
		u_int left;
		char *s;

		bzero((char *)&bsdi_si, sizeof(bsdi_si));
		bzero(bsdi_strings, sizeof(bsdi_strings));

		s = bsdi_strings;

		bsdi_si.bsdi_ostype = ((char *)(s - bsdi_strings)) +
				       sizeof(bsdi_si);
		strlcpy(s, ostype, bsdi_strings + sizeof bsdi_strings - s);
		s += strlen(s) + 1;

		bsdi_si.bsdi_osrelease = ((char *)(s - bsdi_strings)) +
					  sizeof(bsdi_si);
		strlcpy(s, osrelease, bsdi_strings + sizeof bsdi_strings - s);
		s += strlen(s) + 1;

		bsdi_si.bsdi_machine = ((char *)(s - bsdi_strings)) +
					sizeof(bsdi_si);
		strlcpy(s, machine, bsdi_strings + sizeof bsdi_strings - s);
		s += strlen(s) + 1;

		needed = sizeof(bsdi_si) + (s - bsdi_strings);

		if (SCARG(uap, where) == NULL) {
			/* process is asking how much buffer to supply.. */
			size = needed;
			error = 0;
			break;
		}

		/* if too much buffer supplied, trim it down */
		if (size > needed)
			size = needed;

		/* how much of the buffer is remaining */
		left = size;

		if ((error = copyout((char *)&bsdi_si, SCARG(uap, where), 
		    left)) != 0)
			break;

		/* is there any point in continuing? */
		if (left > sizeof(bsdi_si))
			left -= sizeof(bsdi_si);
		else
			break;

		error = copyout(&bsdi_strings, SCARG(uap, where) +
				sizeof(bsdi_si), left);

		break;
	}

	default:
		return (EOPNOTSUPP);
	}
	if (error)
		return (error);
	*retval = size;
	if (SCARG(uap, size))
		error = copyout((caddr_t)&size, (caddr_t)SCARG(uap, size),
		    sizeof(size));
	return (error);
}


/* ARGSUSED */
int
compat_43_sys_sethostid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_sys_sethostid_args /* {
		syscallarg(int32_t) hostid;
	} */ *uap = v;
	int error;

	if ((error = suser(p, 0)) != 0)
		return (error);
	hostid = SCARG(uap, hostid);
	return (0);
}


/* ARGSUSED */
int
compat_43_sys_sethostname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_sys_sethostname_args *uap = v;
	int name;
	int error;

	if ((error = suser(p, 0)) != 0)
		return (error);
	name = KERN_HOSTNAME;
	return (kern_sysctl(&name, 1, 0, 0, SCARG(uap, hostname),
			    SCARG(uap, len), p));
}
