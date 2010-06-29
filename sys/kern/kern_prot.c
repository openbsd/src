/*	$OpenBSD: kern_prot.c,v 1.44 2010/06/29 19:09:11 tedu Exp $	*/
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
#include <sys/times.h>
#include <sys/malloc.h>
#include <sys/filedesc.h>
#include <sys/pool.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

/* ARGSUSED */
int
sys_getpid(struct proc *p, void *v, register_t *retval)
{

	retval[0] = p->p_p->ps_mainproc->p_pid;
	retval[1] = p->p_p->ps_mainproc->p_pptr->p_pid;
	return (0);
}

/* ARGSUSED */
int
sys_getthrid(struct proc *p, void *v, register_t *retval)
{

	if (!rthreads_enabled)
		return (ENOTSUP);
	*retval = p->p_pid + (p->p_flag & P_THREAD ? 0 : THREAD_PID_OFFSET);
	return (0);
}

/* ARGSUSED */
int
sys_getppid(struct proc *p, void *v, register_t *retval)
{

	*retval = p->p_p->ps_mainproc->p_pptr->p_pid;
	return (0);
}

/* Get process group ID; note that POSIX getpgrp takes no parameter */
int
sys_getpgrp(struct proc *p, void *v, register_t *retval)
{

	*retval = p->p_pgrp->pg_id;
	return (0);
}

/*
 * SysVR.4 compatible getpgid()
 */
pid_t
sys_getpgid(struct proc *curp, void *v, register_t *retval)
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
sys_getsid(struct proc *curp, void *v, register_t *retval)
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
sys_getuid(struct proc *p, void *v, register_t *retval)
{

	retval[0] = p->p_cred->p_ruid;
	retval[1] = p->p_ucred->cr_uid;
	return (0);
}

/* ARGSUSED */
int
sys_geteuid(struct proc *p, void *v, register_t *retval)
{

	*retval = p->p_ucred->cr_uid;
	return (0);
}

/* ARGSUSED */
int
sys_issetugid(struct proc *p, void *v, register_t *retval)
{
	if (p->p_flag & P_SUGIDEXEC)
		*retval = 1;
	else
		*retval = 0;
	return (0);
}

/* ARGSUSED */
int
sys_getgid(struct proc *p, void *v, register_t *retval)
{

	retval[0] = p->p_cred->p_rgid;
	retval[1] = p->p_ucred->cr_gid;
	return (0);
}

/*
 * Get effective group ID.  The "egid" is groups[0], and could be obtained
 * via getgroups.  This syscall exists because it is somewhat painful to do
 * correctly in a library function.
 */
/* ARGSUSED */
int
sys_getegid(struct proc *p, void *v, register_t *retval)
{

	*retval = p->p_ucred->cr_gid;
	return (0);
}

int
sys_getgroups(struct proc *p, void *v, register_t *retval)
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
sys_setsid(struct proc *p, void *v, register_t *retval)
{
	struct session *newsess;
	struct pgrp *newpgrp;

	newsess = pool_get(&session_pool, PR_WAITOK);
	newpgrp = pool_get(&pgrp_pool, PR_WAITOK);

	if (p->p_pgid == p->p_pid || pgfind(p->p_pid)) {
		pool_put(&pgrp_pool, newpgrp);
		pool_put(&session_pool, newsess);
		return (EPERM);
	} else {
		(void) enterpgrp(p, p->p_pid, newpgrp, newsess);
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
sys_setpgid(struct proc *curp, void *v, register_t *retval)
{
	struct sys_setpgid_args /* {
		syscallarg(pid_t) pid;
		syscallarg(int) pgid;
	} */ *uap = v;
	struct proc *targp;		/* target process */
	struct pgrp *pgrp, *newpgrp;	/* target pgrp */
	pid_t pid;
	int pgid, error;

	pid = SCARG(uap, pid);
	pgid = SCARG(uap, pgid);

	if (pgid < 0)
		return (EINVAL);

	newpgrp = pool_get(&pgrp_pool, PR_WAITOK);

	if (pid != 0 && pid != curp->p_pid) {
		if ((targp = pfind(pid)) == 0 || !inferior(targp, curp)) {
			error = ESRCH;
			goto out;
		}
		if (targp->p_session != curp->p_session) {
			error = EPERM;
			goto out;
		}
		if (targp->p_flag & P_EXEC) {
			error = EACCES;
			goto out;
		}
	} else
		targp = curp;
	if (SESS_LEADER(targp)) {
		error = EPERM;
		goto out;
	}
	if (pgid == 0)
		pgid = targp->p_pid;
	else if (pgid != targp->p_pid)
		if ((pgrp = pgfind(pgid)) == 0 ||
		    pgrp->pg_session != curp->p_session) {
			error = EPERM;
			goto out;
		}
	return (enterpgrp(targp, pgid, newpgrp, NULL));
out:
	pool_put(&pgrp_pool, newpgrp);
	return (error);
}

/* ARGSUSED */
int
sys_getresuid(struct proc *p, void *v, register_t *retval)
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
sys_setresuid(struct proc *p, void *v, register_t *retval)
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
	    (error = suser(p, 0)))
		return (error);

	if (euid != (uid_t)-1 &&
	    euid != pc->p_ruid &&
	    euid != pc->pc_ucred->cr_uid &&
	    euid != pc->p_svuid &&
	    (error = suser(p, 0)))
		return (error);

	if (suid != (uid_t)-1 &&
	    suid != pc->p_ruid &&
	    suid != pc->pc_ucred->cr_uid &&
	    suid != pc->p_svuid &&
	    (error = suser(p, 0)))
		return (error);

	/*
	 * Note that unlike the other set*uid() calls, each
	 * uid type is set independently of the others.
	 */
	if (ruid != (uid_t)-1 && ruid != pc->p_ruid) {
		/*
		 * Transfer proc count to new user.
		 */
		(void)chgproccnt(pc->p_ruid, -p->p_p->ps_refcnt);
		(void)chgproccnt(ruid, p->p_p->ps_refcnt);
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

	atomic_setbits_int(&p->p_flag, P_SUGID);
	return (0);
}

/* ARGSUSED */
int
sys_getresgid(struct proc *p, void *v, register_t *retval)
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
sys_setresgid(struct proc *p, void *v, register_t *retval)
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
	    (error = suser(p, 0)))
		return (error);

	if (egid != (gid_t)-1 &&
	    egid != pc->p_rgid &&
	    egid != pc->pc_ucred->cr_gid &&
	    egid != pc->p_svgid &&
	    (error = suser(p, 0)))
		return (error);

	if (sgid != (gid_t)-1 &&
	    sgid != pc->p_rgid &&
	    sgid != pc->pc_ucred->cr_gid &&
	    sgid != pc->p_svgid &&
	    (error = suser(p, 0)))
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

	atomic_setbits_int(&p->p_flag, P_SUGID);
	return (0);
}

/* ARGSUSED */
int
sys_setregid(struct proc *p, void *v, register_t *retval)
{
	struct sys_setregid_args /* {
		syscallarg(gid_t) rgid;
		syscallarg(gid_t) egid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	struct sys_setresgid_args sresgidargs;
	gid_t rgid, egid;

	rgid = SCARG(&sresgidargs, rgid) = SCARG(uap, rgid);
	egid = SCARG(&sresgidargs, egid) = SCARG(uap, egid);

	/*
	 * The saved gid presents a bit of a dilemma, as it did not
	 * exist when setregid(2) was conceived.  We only set the saved
	 * gid when the real gid is specified and either its value would
	 * change, or where the saved and effective gids are different.
	 */
	if (rgid != (gid_t)-1 && (rgid != pc->p_rgid ||
	    pc->p_svgid != (egid != (gid_t)-1 ? egid : pc->pc_ucred->cr_gid)))
		SCARG(&sresgidargs, sgid) = rgid;
	else
		SCARG(&sresgidargs, sgid) = (gid_t)-1;

	return (sys_setresgid(p, &sresgidargs, retval));
}

/* ARGSUSED */
int
sys_setreuid(struct proc *p, void *v, register_t *retval)
{
	struct sys_setreuid_args /* {
		syscallarg(uid_t) ruid;
		syscallarg(uid_t) euid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	struct sys_setresuid_args sresuidargs;
	uid_t ruid, euid;

	ruid = SCARG(&sresuidargs, ruid) = SCARG(uap, ruid);
	euid = SCARG(&sresuidargs, euid) = SCARG(uap, euid);

	/*
	 * The saved uid presents a bit of a dilemma, as it did not
	 * exist when setreuid(2) was conceived.  We only set the saved
	 * uid when the real uid is specified and either its value would
	 * change, or where the saved and effective uids are different.
	 */
	if (ruid != (uid_t)-1 && (ruid != pc->p_ruid ||
	    pc->p_svuid != (euid != (uid_t)-1 ? euid : pc->pc_ucred->cr_uid)))
		SCARG(&sresuidargs, suid) = ruid;
	else
		SCARG(&sresuidargs, suid) = (uid_t)-1;

	return (sys_setresuid(p, &sresuidargs, retval));
}

/* ARGSUSED */
int
sys_setuid(struct proc *p, void *v, register_t *retval)
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
	    (error = suser(p, 0)))
		return (error);

	/*
	 * Everything's okay, do it.
	 */
	if (uid == pc->pc_ucred->cr_uid ||
	    suser(p, 0) == 0) {
		/*
		 * Transfer proc count to new user.
		 */
		if (uid != pc->p_ruid) {
			(void)chgproccnt(pc->p_ruid, -p->p_p->ps_refcnt);
			(void)chgproccnt(uid, p->p_p->ps_refcnt);
		}
		pc->p_ruid = uid;
		pc->p_svuid = uid;
	}

	/*
	 * Copy credentials so other references do not see our changes.
	 */
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_uid = uid;
	atomic_setbits_int(&p->p_flag, P_SUGID);
	return (0);
}

/* ARGSUSED */
int
sys_seteuid(struct proc *p, void *v, register_t *retval)
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
	    (error = suser(p, 0)))
		return (error);

	/*
	 * Copy credentials so other references do not see our changes.
	 */
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_uid = euid;
	atomic_setbits_int(&p->p_flag, P_SUGID);
	return (0);
}

/* ARGSUSED */
int
sys_setgid(struct proc *p, void *v, register_t *retval)
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
	    (error = suser(p, 0)))
		return (error);

	if (gid == pc->pc_ucred->cr_gid ||
	    suser(p, 0) == 0) {
		pc->p_rgid = gid;
		pc->p_svgid = gid;
	}

	/*
	 * Copy credentials so other references do not see our changes.
	 */
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_gid = gid;
	atomic_setbits_int(&p->p_flag, P_SUGID);
	return (0);
}

/* ARGSUSED */
int
sys_setegid(struct proc *p, void *v, register_t *retval)
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
	    (error = suser(p, 0)))
		return (error);

	/*
	 * Copy credentials so other references do not see our changes.
	 */
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_gid = egid;
	atomic_setbits_int(&p->p_flag, P_SUGID);
	return (0);
}

/* ARGSUSED */
int
sys_setgroups(struct proc *p, void *v, register_t *retval)
{
	struct sys_setgroups_args /* {
		syscallarg(int) gidsetsize;
		syscallarg(const gid_t *) gidset;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	u_int ngrp;
	int error;

	if ((error = suser(p, 0)) != 0)
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
	atomic_setbits_int(&p->p_flag, P_SUGID);
	return (0);
}

/*
 * Check if gid is a member of the group set.
 */
int
groupmember(gid_t gid, struct ucred *cred)
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
 * Test whether this process has special user powers.
 * Returns 0 or error.
 */
int
suser(struct proc *p, u_int flags)
{
	struct ucred *cred = p->p_ucred;

	if (cred->cr_uid == 0) {
		if (!(flags & SUSER_NOACCT))
			p->p_acflag |= ASU;
		return (0);
	}
	return (EPERM);
}

/*
 * replacement for old suser, for callers who don't have a process
 */
int
suser_ucred(struct ucred *cred)
{
	if (cred->cr_uid == 0)
		return (0);
	return (EPERM);
}

/*
 * Allocate a zeroed cred structure.
 */
struct ucred *
crget(void)
{
	struct ucred *cr;

	cr = pool_get(&ucred_pool, PR_WAITOK|PR_ZERO);
	cr->cr_ref = 1;
	return (cr);
}

/*
 * Free a cred structure.
 * Throws away space when ref count gets to 0.
 */
void
crfree(struct ucred *cr)
{

	if (--cr->cr_ref == 0)
		pool_put(&ucred_pool, cr);
}

/*
 * Copy cred structure to a new one and free the old one.
 */
struct ucred *
crcopy(struct ucred *cr)
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
crdup(struct ucred *cr)
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
sys_getlogin(struct proc *p, void *v, register_t *retval)
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
sys_setlogin(struct proc *p, void *v, register_t *retval)
{
	struct sys_setlogin_args /* {
		syscallarg(const char *) namebuf;
	} */ *uap = v;
	int error;

	if ((error = suser(p, 0)) != 0)
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

	/* processes with shared filedescriptors shouldn't. */
	if (p->p_fd->fd_refcnt > 1)
		return (0);

	/* Allow. */
	return (1);
}
