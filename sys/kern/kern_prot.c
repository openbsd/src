/*	$OpenBSD: kern_prot.c,v 1.22 2002/10/30 20:02:58 millert Exp $	*/
/*	$NetBSD: kern_prot.c,v 1.33 1996/02/09 18:59:42 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)kern_prot.c	8.6 (Berkeley) 1/21/94
 */

/*
 * System calls related to processes and protection
 */

#include <sys/param.h>
#include <sys/acct.h>
#include <sys/systm.h>
#include <sys/ucred.h>
#include <sys/proc.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/malloc.h>
#include <sys/filedesc.h>
#include <sys/pool.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

/* ARGSUSED */
int
sys_getpid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	*retval = p->p_pid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS) || defined(COMPAT_IBCS2) || \
    defined(COMPAT_FREEBSD) || defined(COMPAT_BSDOS)
	retval[1] = p->p_pptr->p_pid;
#endif
	return (0);
}

/* ARGSUSED */
int
sys_getppid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	*retval = p->p_pptr->p_pid;
	return (0);
}

/* Get process group ID; note that POSIX getpgrp takes no parameter */
int
sys_getpgrp(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	*retval = p->p_pgrp->pg_id;
	return (0);
}

/*
 * SysVR.4 compatible getpgid()
 */
pid_t
sys_getpgid(curp, v, retval)
	struct proc *curp;
	void *v;
	register_t *retval;
{
	struct sys_getpgid_args /* {
		syscallarg(pid_t) pid;
	} */ *uap = v;
	struct proc *targp = curp;

	if (SCARG(uap, pid) == 0 || SCARG(uap, pid) == curp->p_pid)
		goto found;
	if ((targp = pfind(SCARG(uap, pid))) == NULL)
		return (ESRCH);
	if (targp->p_session != curp->p_session)
		return (EPERM);
found:
	*retval = targp->p_pgid;
	return (0);
}

pid_t
sys_getsid(curp, v, retval)
	struct proc *curp;
	void *v;
	register_t *retval;
{
	struct sys_getsid_args /* {
		syscallarg(pid_t) pid;
	} */ *uap = v;
	struct proc *targp = curp;

	if (SCARG(uap, pid) == 0 || SCARG(uap, pid) == curp->p_pid)
		goto found;
	if ((targp = pfind(SCARG(uap, pid))) == NULL)
		return (ESRCH);
	if (targp->p_session != curp->p_session)
		return (EPERM);
found:
	/* Skip exiting processes */
	if (targp->p_pgrp->pg_session->s_leader == NULL)
		return (ESRCH);
	*retval = targp->p_pgrp->pg_session->s_leader->p_pid;
	return (0);
}

/* ARGSUSED */
int
sys_getuid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	*retval = p->p_cred->p_ruid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS) || defined(COMPAT_IBCS2) || \
    defined(COMPAT_FREEBSD) || defined(COMPAT_BSDOS)
	retval[1] = p->p_ucred->cr_uid;
#endif
	return (0);
}

/* ARGSUSED */
int
sys_geteuid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	*retval = p->p_ucred->cr_uid;
	return (0);
}

/* ARGSUSED */
int
sys_issetugid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	if (p->p_flag & P_SUGIDEXEC)
		*retval = 1;
	else
		*retval = 0;
	return (0);
}

/* ARGSUSED */
int
sys_getgid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	*retval = p->p_cred->p_rgid;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS) || defined(COMPAT_FREEBSD) || defined(COMPAT_BSDOS)
	retval[1] = p->p_ucred->cr_gid;
#endif
	return (0);
}

/*
 * Get effective group ID.  The "egid" is groups[0], and could be obtained
 * via getgroups.  This syscall exists because it is somewhat painful to do
 * correctly in a library function.
 */
/* ARGSUSED */
int
sys_getegid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	*retval = p->p_ucred->cr_gid;
	return (0);
}

int
sys_getgroups(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_getgroups_args /* {
		syscallarg(int) gidsetsize;
		syscallarg(gid_t *) gidset;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	u_int ngrp;
	int error;

	if ((ngrp = SCARG(uap, gidsetsize)) == 0) {
		*retval = pc->pc_ucred->cr_ngroups;
		return (0);
	}
	if (ngrp < pc->pc_ucred->cr_ngroups)
		return (EINVAL);
	ngrp = pc->pc_ucred->cr_ngroups;
	error = copyout((caddr_t)pc->pc_ucred->cr_groups,
	    (caddr_t)SCARG(uap, gidset), ngrp * sizeof(gid_t));
	if (error)
		return (error);
	*retval = ngrp;
	return (0);
}

/* ARGSUSED */
int
sys_setsid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	if (p->p_pgid == p->p_pid || pgfind(p->p_pid)) {
		return (EPERM);
	} else {
		(void)enterpgrp(p, p->p_pid, 1);
		*retval = p->p_pid;
		return (0);
	}
}

/*
 * set process group (setpgid/old setpgrp)
 *
 * caller does setpgid(targpid, targpgid)
 *
 * pid must be caller or child of caller (ESRCH)
 * if a child
 *	pid must be in same session (EPERM)
 *	pid can't have done an exec (EACCES)
 * if pgid != pid
 * 	there must exist some pid in same session having pgid (EPERM)
 * pid must not be session leader (EPERM)
 */
/* ARGSUSED */
int
sys_setpgid(curp, v, retval)
	struct proc *curp;
	void *v;
	register_t *retval;
{
	struct sys_setpgid_args /* {
		syscallarg(pid_t) pid;
		syscallarg(int) pgid;
	} */ *uap = v;
	struct proc *targp;		/* target process */
	struct pgrp *pgrp;		/* target pgrp */
	pid_t pid;
	int pgid;

	pid = SCARG(uap, pid);
	pgid = SCARG(uap, pgid);

	if (pgid < 0)
		return (EINVAL);

	if (pid != 0 && pid != curp->p_pid) {
		if ((targp = pfind(pid)) == 0 || !inferior(targp))
			return (ESRCH);
		if (targp->p_session != curp->p_session)
			return (EPERM);
		if (targp->p_flag & P_EXEC)
			return (EACCES);
	} else
		targp = curp;
	if (SESS_LEADER(targp))
		return (EPERM);
	if (pgid == 0)
		pgid = targp->p_pid;
	else if (pgid != targp->p_pid)
		if ((pgrp = pgfind(pgid)) == 0 ||
		    pgrp->pg_session != curp->p_session)
			return (EPERM);
	return (enterpgrp(targp, pgid, 0));
}

/* ARGSUSED */
int
sys_getresuid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_getresuid_args /* {
		syscallarg(uid_t *) ruid;
		syscallarg(uid_t *) euid;
		syscallarg(uid_t *) suid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	uid_t *ruid, *euid, *suid;
	int error1 = 0, error2 = 0, error3 = 0;

	ruid = SCARG(uap, ruid);
	euid = SCARG(uap, euid);
	suid = SCARG(uap, suid);

	if (ruid != NULL)
		error1 = copyout(&pc->p_ruid, ruid, sizeof(*ruid));
	if (euid != NULL)
		error2 = copyout(&pc->pc_ucred->cr_uid, euid, sizeof(*euid));
	if (suid != NULL)
		error3 = copyout(&pc->p_svuid, suid, sizeof(*suid));

	return (error1 ? error1 : error2 ? error2 : error3);
}

/* ARGSUSED */
int
sys_setresuid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_setresuid_args /* {
		syscallarg(uid_t) ruid;
		syscallarg(uid_t) euid;
		syscallarg(uid_t) suid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	uid_t ruid, euid, suid;
	int error;

	ruid = SCARG(uap, ruid);
	euid = SCARG(uap, euid);
	suid = SCARG(uap, suid);

	if ((ruid == -1 || ruid == pc->p_ruid) &&
	    (euid == -1 || euid == pc->pc_ucred->cr_uid) &&
	    (suid == -1 || suid == pc->p_svuid))
		return (0);			/* no change */

	/*
	 * Any of the real, effective, and saved uids may be changed
	 * to the current value of one of the three (root is not limited).
	 */
	if (ruid != (uid_t)-1 &&
	    ruid != pc->p_ruid &&
	    ruid != pc->pc_ucred->cr_uid &&
	    ruid != pc->p_svuid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	if (euid != (uid_t)-1 &&
	    euid != pc->p_ruid &&
	    euid != pc->pc_ucred->cr_uid &&
	    euid != pc->p_svuid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	if (suid != (uid_t)-1 &&
	    suid != pc->p_ruid &&
	    suid != pc->pc_ucred->cr_uid &&
	    suid != pc->p_svuid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	/*
	 * Note that unlike the other set*uid() calls, each
	 * uid type is set independently of the others.
	 */
	if (ruid != (uid_t)-1 && ruid != pc->p_ruid) {
		/*
		 * Transfer proc count to new user.
		 */
		(void)chgproccnt(pc->p_ruid, -1);
		(void)chgproccnt(ruid, 1);
		pc->p_ruid = ruid;
	}
	if (euid != (uid_t)-1 && euid != pc->pc_ucred->cr_uid) {
		/*
		 * Copy credentials so other references do not see our changes.
		 */
		pc->pc_ucred = crcopy(pc->pc_ucred);
		pc->pc_ucred->cr_uid = euid;
	}
	if (suid != (uid_t)-1 && suid != pc->p_svuid)
		pc->p_svuid = suid;

	p->p_flag |= P_SUGID;
	return (0);
}

/* ARGSUSED */
int
sys_getresgid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_getresgid_args /* {
		syscallarg(gid_t *) rgid;
		syscallarg(gid_t *) egid;
		syscallarg(gid_t *) sgid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	gid_t *rgid, *egid, *sgid;
	int error1 = 0, error2 = 0, error3 = 0;

	rgid = SCARG(uap, rgid);
	egid = SCARG(uap, egid);
	sgid = SCARG(uap, sgid);

	if (rgid != NULL)
		error1 = copyout(&pc->p_rgid, rgid, sizeof(*rgid));
	if (egid != NULL)
		error2 = copyout(&pc->pc_ucred->cr_gid, egid, sizeof(*egid));
	if (sgid != NULL)
		error3 = copyout(&pc->p_svgid, sgid, sizeof(*sgid));

	return (error1 ? error1 : error2 ? error2 : error3);
}

/* ARGSUSED */
int
sys_setresgid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_setresgid_args /* {
		syscallarg(gid_t) rgid;
		syscallarg(gid_t) egid;
		syscallarg(gid_t) sgid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	gid_t rgid, egid, sgid;
	int error;

	rgid = SCARG(uap, rgid);
	egid = SCARG(uap, egid);
	sgid = SCARG(uap, sgid);

	if ((rgid == -1 || rgid == pc->p_rgid) &&
	    (egid == -1 || egid == pc->pc_ucred->cr_gid) &&
	    (sgid == -1 || sgid == pc->p_svgid))
		return (0);			/* no change */

	/*
	 * Any of the real, effective, and saved gids may be changed
	 * to the current value of one of the three (root is not limited).
	 */
	if (rgid != (gid_t)-1 &&
	    rgid != pc->p_rgid &&
	    rgid != pc->pc_ucred->cr_gid &&
	    rgid != pc->p_svgid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	if (egid != (gid_t)-1 &&
	    egid != pc->p_rgid &&
	    egid != pc->pc_ucred->cr_gid &&
	    egid != pc->p_svgid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	if (sgid != (gid_t)-1 &&
	    sgid != pc->p_rgid &&
	    sgid != pc->pc_ucred->cr_gid &&
	    sgid != pc->p_svgid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	/*
	 * Note that unlike the other set*gid() calls, each
	 * gid type is set independently of the others.
	 */
	if (rgid != (gid_t)-1)
		pc->p_rgid = rgid;
	if (egid != (gid_t)-1) {
		/*
		 * Copy credentials so other references do not see our changes.
		 */
		pc->pc_ucred = crcopy(pc->pc_ucred);
		pc->pc_ucred->cr_gid = egid;
	}
	if (sgid != (gid_t)-1)
		pc->p_svgid = sgid;

	p->p_flag |= P_SUGID;
	return (0);
}

/* ARGSUSED */
int
sys_setuid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_setuid_args /* {
		syscallarg(uid_t) uid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	uid_t uid;
	int error;

	uid = SCARG(uap, uid);

	if (pc->pc_ucred->cr_uid == uid &&
	    pc->p_ruid == uid &&
	    pc->p_svuid == uid)
		return (0);

	if (uid != pc->p_ruid &&
	    uid != pc->p_svuid &&
	    uid != pc->pc_ucred->cr_uid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	/*
	 * Everything's okay, do it.
	 */
	if (uid == pc->pc_ucred->cr_uid ||
	    suser(pc->pc_ucred, &p->p_acflag) == 0) {
		/*
		 * Transfer proc count to new user.
		 */
		if (uid != pc->p_ruid) {
			(void)chgproccnt(pc->p_ruid, -1);
			(void)chgproccnt(uid, 1);
		}
		pc->p_ruid = uid;
		pc->p_svuid = uid;
	}

	/*
	 * Copy credentials so other references do not see our changes.
	 */
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_uid = uid;
	p->p_flag |= P_SUGID;
	return (0);
}

/* ARGSUSED */
int
sys_seteuid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_seteuid_args /* {
		syscallarg(uid_t) euid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	uid_t euid;
	int error;

	euid = SCARG(uap, euid);

	if (pc->pc_ucred->cr_uid == euid)
		return (0);

	if (euid != pc->p_ruid && euid != pc->p_svuid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	/*
	 * Copy credentials so other references do not see our changes.
	 */
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_uid = euid;
	p->p_flag |= P_SUGID;
	return (0);
}

/* ARGSUSED */
int
sys_setgid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_setgid_args /* {
		syscallarg(gid_t) gid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	gid_t gid;
	int error;

	gid = SCARG(uap, gid);

	if (pc->pc_ucred->cr_gid == gid &&
	    pc->p_rgid == gid &&
	    pc->p_svgid == gid)
		return (0);

	if (gid != pc->p_rgid &&
	    gid != pc->p_svgid &&
	    gid != pc->pc_ucred->cr_gid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	if (gid == pc->pc_ucred->cr_gid ||
	    suser(pc->pc_ucred, &p->p_acflag) == 0) {
		pc->p_rgid = gid;
		pc->p_svgid = gid;
	}

	/*
	 * Copy credentials so other references do not see our changes.
	 */
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_gid = gid;
	p->p_flag |= P_SUGID;
	return (0);
}

/* ARGSUSED */
int
sys_setegid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_setegid_args /* {
		syscallarg(gid_t) egid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	gid_t egid;
	int error;

	egid = SCARG(uap, egid);

	if (pc->pc_ucred->cr_gid == egid)
		return (0);

	if (egid != pc->p_rgid && egid != pc->p_svgid &&
	    (error = suser(pc->pc_ucred, &p->p_acflag)))
		return (error);

	/*
	 * Copy credentials so other references do not see our changes.
	 */
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_gid = egid;
	p->p_flag |= P_SUGID;
	return (0);
}

/* ARGSUSED */
int
sys_setgroups(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_setgroups_args /* {
		syscallarg(int) gidsetsize;
		syscallarg(gid_t *) gidset;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	u_int ngrp;
	int error;

	if ((error = suser(pc->pc_ucred, &p->p_acflag)) != 0)
		return (error);
	ngrp = SCARG(uap, gidsetsize);
	if (ngrp > NGROUPS)
		return (EINVAL);
	pc->pc_ucred = crcopy(pc->pc_ucred);
	error = copyin((caddr_t)SCARG(uap, gidset),
	    (caddr_t)pc->pc_ucred->cr_groups, ngrp * sizeof(gid_t));
	if (error)
		return (error);
	pc->pc_ucred->cr_ngroups = ngrp;
	p->p_flag |= P_SUGID;
	return (0);
}

/*
 * Check if gid is a member of the group set.
 */
int
groupmember(gid, cred)
	gid_t gid;
	struct ucred *cred;
{
	gid_t *gp;
	gid_t *egp;

	egp = &(cred->cr_groups[cred->cr_ngroups]);
	for (gp = cred->cr_groups; gp < egp; gp++)
		if (*gp == gid)
			return (1);
	return (0);
}

/*
 * Test whether the specified credentials imply "super-user"
 * privilege; if so, and we have accounting info, set the flag
 * indicating use of super-powers.
 * Returns 0 or error.
 */
int
suser(cred, acflag)
	struct ucred *cred;
	u_short *acflag;
{
	if (cred->cr_uid == 0) {
		if (acflag)
			*acflag |= ASU;
		return (0);
	}
	return (EPERM);
}

/*
 * Allocate a zeroed cred structure.
 */
struct ucred *
crget()
{
	struct ucred *cr;

	cr = pool_get(&ucred_pool, PR_WAITOK);
	bzero((caddr_t)cr, sizeof(*cr));
	cr->cr_ref = 1;
	return (cr);
}

/*
 * Free a cred structure.
 * Throws away space when ref count gets to 0.
 */
void
crfree(cr)
	struct ucred *cr;
{

	if (--cr->cr_ref == 0)
		pool_put(&ucred_pool, cr);
}

/*
 * Copy cred structure to a new one and free the old one.
 */
struct ucred *
crcopy(cr)
	struct ucred *cr;
{
	struct ucred *newcr;

	if (cr->cr_ref == 1)
		return (cr);
	newcr = crget();
	*newcr = *cr;
	crfree(cr);
	newcr->cr_ref = 1;
	return (newcr);
}

/*
 * Dup cred struct to a new held one.
 */
struct ucred *
crdup(cr)
	struct ucred *cr;
{
	struct ucred *newcr;

	newcr = crget();
	*newcr = *cr;
	newcr->cr_ref = 1;
	return (newcr);
}

/*
 * Get login name, if available.
 */
/* ARGSUSED */
int
sys_getlogin(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_getlogin_args /* {
		syscallarg(char *) namebuf;
		syscallarg(u_int) namelen;
	} */ *uap = v;

	if (SCARG(uap, namelen) > sizeof (p->p_pgrp->pg_session->s_login))
		SCARG(uap, namelen) = sizeof (p->p_pgrp->pg_session->s_login);
	return (copyout((caddr_t) p->p_pgrp->pg_session->s_login,
	    (caddr_t) SCARG(uap, namebuf), SCARG(uap, namelen)));
}

/*
 * Set login name.
 */
/* ARGSUSED */
int
sys_setlogin(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_setlogin_args /* {
		syscallarg(char *) namebuf;
	} */ *uap = v;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
	error = copyinstr((caddr_t) SCARG(uap, namebuf),
	    (caddr_t) p->p_pgrp->pg_session->s_login,
	    sizeof (p->p_pgrp->pg_session->s_login), (size_t *)0);
	if (error == ENAMETOOLONG)
		error = EINVAL;
	return (error);
}

/*
 * Check if a process is allowed to raise its privileges.
 */
int
proc_cansugid(struct proc *p)
{
	/* ptrace(2)d processes shouldn't. */
	if ((p->p_flag & P_TRACED) != 0)
		return (0);

	/* proceses with shared filedescriptors shouldn't. */
	if (p->p_fd->fd_refcnt > 1)
		return (0);

	/* Allow. */
	return (1);
}
