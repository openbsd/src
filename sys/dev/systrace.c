/*	$OpenBSD: systrace.c,v 1.58 2011/07/07 18:11:24 art Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tree.h>
#include <sys/malloc.h>
#include <sys/syscall.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/signalvar.h>
#include <sys/rwlock.h>
#include <sys/pool.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/poll.h>
#include <sys/ptrace.h>

#include <compat/common/compat_util.h>

#include <dev/systrace.h>

void	systraceattach(int);

int	systraceopen(dev_t, int, int, struct proc *);
int	systraceclose(dev_t, int, int, struct proc *);
int	systraceioctl(dev_t, u_long, caddr_t, int, struct proc *);

uid_t	systrace_seteuid(struct proc *,  uid_t);
gid_t	systrace_setegid(struct proc *,  gid_t);
int	systracef_read(struct file *, off_t *, struct uio *, struct ucred *);
int	systracef_write(struct file *, off_t *, struct uio *, struct ucred *);
int	systracef_ioctl(struct file *, u_long, caddr_t, struct proc *p);
int	systracef_poll(struct file *, int, struct proc *);
int	systracef_kqfilter(struct file *, struct knote *);
int	systracef_stat(struct file *, struct stat *, struct proc *);
int	systracef_close(struct file *, struct proc *);

struct str_policy {
	TAILQ_ENTRY(str_policy) next;

	int nr;

	struct emul *emul;	/* Is only valid for this emulation */

	int refcount;

	int nsysent;
	u_char *sysent;
};

struct str_inject {
	caddr_t kaddr;
	caddr_t uaddr;
	size_t  len;
};

#define STR_PROC_ONQUEUE	0x01
#define STR_PROC_WAITANSWER	0x02
#define STR_PROC_SYSCALLRES	0x04
#define STR_PROC_REPORT		0x08	/* Report emulation */
#define STR_PROC_NEEDSEQNR	0x10	/* Answer must quote seqnr */
#define STR_PROC_SETEUID	0x20	/* Elevate privileges */
#define STR_PROC_SETEGID	0x40

struct str_process {
	TAILQ_ENTRY(str_process) next;
	TAILQ_ENTRY(str_process) msg_next;
	struct str_process *firstmsg;

	struct proc *proc;
	pid_t pid;

	struct fsystrace *parent;
	struct str_policy *policy;

	struct systrace_replace *replace;
	char *fname[SYSTR_MAXFNAME];
	size_t nfname;

	int flags;
	short answer;
	short error;
	u_int16_t seqnr;	/* expected reply sequence number */

	uid_t seteuid;
	uid_t saveuid;
	gid_t setegid;
	gid_t savegid;

	int isscript;
	char scriptname[MAXPATHLEN];

	struct str_message msg;

	caddr_t sg;
	struct str_inject injects[SYSTR_MAXINJECTS];
	int  injectind;
};

struct rwlock systrace_lck;

static __inline void
systrace_lock(void)
{
	rw_enter_write(&systrace_lck);
}

static __inline void
systrace_unlock(void)
{
	rw_exit_write(&systrace_lck);
}

/* Needs to be called with fst locked */

int	systrace_attach(struct fsystrace *, pid_t);
int	systrace_detach(struct str_process *);
int	systrace_answer(struct str_process *, struct systrace_answer *);
int     systrace_setscriptname(struct str_process *,
	    struct systrace_scriptname *);
int     systrace_prepinject(struct str_process *, struct systrace_inject *);
int     systrace_inject(struct str_process *, int);
int	systrace_io(struct str_process *, struct systrace_io *);
int	systrace_policy(struct fsystrace *, struct systrace_policy *);
int	systrace_preprepl(struct str_process *, struct systrace_replace *);
int	systrace_replace(struct str_process *, size_t, register_t []);
int	systrace_getcwd(struct fsystrace *, struct str_process *);
int	systrace_fname(struct str_process *, caddr_t, size_t);
void	systrace_replacefree(struct str_process *);

int	systrace_processready(struct str_process *);
struct proc *systrace_find(struct str_process *);
struct str_process *systrace_findpid(struct fsystrace *fst, pid_t pid);
void	systrace_wakeup(struct fsystrace *);
void	systrace_closepolicy(struct fsystrace *, struct str_policy *);
void	systrace_insert_process(struct fsystrace *, struct proc *,
	    struct str_process *);
struct str_policy *systrace_newpolicy(struct fsystrace *, int);
int	systrace_msg_child(struct fsystrace *, struct str_process *, pid_t);
int	systrace_msg_policyfree(struct fsystrace *, struct str_policy *);
int	systrace_msg_ask(struct fsystrace *, struct str_process *,
	    int, size_t, register_t []);
int	systrace_msg_result(struct fsystrace *, struct str_process *,
	    int, int, size_t, register_t [], register_t []);
int	systrace_msg_emul(struct fsystrace *, struct str_process *);
int	systrace_msg_ugid(struct fsystrace *, struct str_process *);
int	systrace_make_msg(struct str_process *, int);

static struct fileops systracefops = {
	systracef_read,
	systracef_write,
	systracef_ioctl,
	systracef_poll,
	systracef_kqfilter,
	systracef_stat,
	systracef_close
};

struct pool systr_proc_pl;
struct pool systr_policy_pl;

int systrace_debug = 0;

#define DPRINTF(y)	if (systrace_debug) printf y;

/* ARGSUSED */
int
systracef_read(struct file *fp, off_t *poff, struct uio *uio,
    struct ucred *cred)
{
	struct fsystrace *fst = (struct fsystrace *)fp->f_data;
	struct str_process *process;
	int error = 0;

	if (uio->uio_resid != sizeof(struct str_message))
		return (EINVAL);

 again:
	systrace_lock();
	rw_enter_write(&fst->lock);
	systrace_unlock();
	if ((process = TAILQ_FIRST(&fst->messages)) != NULL) {
		error = uiomove((caddr_t)&process->msg,
		    sizeof(struct str_message), uio);
		if (!error) {
			TAILQ_REMOVE(&fst->messages, process, msg_next);
			CLR(process->flags, STR_PROC_ONQUEUE);

			if (SYSTR_MSG_NOPROCESS(process))
				pool_put(&systr_proc_pl, process);

		}
	} else if (TAILQ_FIRST(&fst->processes) == NULL) {
		/* EOF situation */
		;
	} else {
		if (fp->f_flag & FNONBLOCK)
			error = EAGAIN;
		else {
			rw_exit_write(&fst->lock);
			error = tsleep(fst, PWAIT|PCATCH, "systrrd", 0);
			if (error)
				goto out;
			goto again;
		}

	}

	rw_exit_write(&fst->lock);
 out:
	return (error);
}

/* ARGSUSED */
int
systracef_write(struct file *fp, off_t *poff, struct uio *uio,
    struct ucred *cred)
{
	return (EIO);
}

#define POLICY_VALID(x)	((x) == SYSTR_POLICY_PERMIT || \
			 (x) == SYSTR_POLICY_ASK || \
			 (x) == SYSTR_POLICY_NEVER || \
			 (x) == SYSTR_POLICY_KILL)

/* ARGSUSED */
int
systracef_ioctl(struct file *fp, u_long cmd, caddr_t data, struct proc *p)
{
	int ret = 0;
	struct fsystrace *fst = (struct fsystrace *)fp->f_data;
	struct filedesc *fdp;
	struct str_process *strp;
	pid_t pid = 0;

	switch (cmd) {
	case FIONBIO:
	case FIOASYNC:
		return (0);

	case STRIOCDETACH:
	case STRIOCREPORT:
		pid = *(pid_t *)data;
		if (!pid)
			ret = EINVAL;
		break;
	case STRIOCANSWER:
		pid = ((struct systrace_answer *)data)->stra_pid;
		if (!pid)
			ret = EINVAL;
		break;
	case STRIOCIO:
		pid = ((struct systrace_io *)data)->strio_pid;
		if (!pid)
			ret = EINVAL;
		break;
	case STRIOCSCRIPTNAME:
		pid = ((struct systrace_scriptname *)data)->sn_pid;
		if (!pid)
			ret = EINVAL;
		break;
	case STRIOCINJECT:
		pid = ((struct systrace_inject *)data)->stri_pid;
		if (!pid)
			ret = EINVAL;
		break;
	case STRIOCGETCWD:
		pid = *(pid_t *)data;
		if (!pid)
			ret = EINVAL;
		break;
	case STRIOCATTACH:
	case STRIOCRESCWD:
	case STRIOCPOLICY:
		break;
	case STRIOCREPLACE:
		pid = ((struct systrace_replace *)data)->strr_pid;
		if (!pid)
			ret = EINVAL;
		break;
	default:
		ret = ENOTTY;
		break;
	}

	if (ret)
		return (ret);

	systrace_lock();
	rw_enter_write(&fst->lock);
	systrace_unlock();
	if (pid) {
		strp = systrace_findpid(fst, pid);
		if (strp == NULL) {
			ret = ESRCH;
			goto unlock;
		}
	}

	switch (cmd) {
	case STRIOCATTACH:
		pid = *(pid_t *)data;
		if (!pid)
			ret = EINVAL;
		else
			ret = systrace_attach(fst, pid);
		DPRINTF(("%s: attach to %u: %d\n", __func__, pid, ret));
		break;
	case STRIOCDETACH:
		ret = systrace_detach(strp);
		break;
	case STRIOCREPORT:
		SET(strp->flags, STR_PROC_REPORT);
		break;
	case STRIOCANSWER:
		ret = systrace_answer(strp, (struct systrace_answer *)data);
		break;
	case STRIOCIO:
		ret = systrace_io(strp, (struct systrace_io *)data);
		break;
	case STRIOCSCRIPTNAME:
		ret = systrace_setscriptname(strp,
		    (struct systrace_scriptname *)data);
		break;
	case STRIOCINJECT:
		ret = systrace_prepinject(strp, (struct systrace_inject *)data);
		break;
	case STRIOCPOLICY:
		ret = systrace_policy(fst, (struct systrace_policy *)data);
		break;
	case STRIOCREPLACE:
		ret = systrace_preprepl(strp, (struct systrace_replace *)data);
		break;
	case STRIOCRESCWD:
		if (!fst->fd_pid) {
			ret = EINVAL;
			break;
		}
		fdp = p->p_fd;

		/* Release cwd from other process */
		if (fdp->fd_cdir)
			vrele(fdp->fd_cdir);
		if (fdp->fd_rdir)
			vrele(fdp->fd_rdir);
		/* This restores the cwd we had before */
		fdp->fd_cdir = fst->fd_cdir;
		fdp->fd_rdir = fst->fd_rdir;
		/* Note that we are normal again */
		fst->fd_pid = 0;
		fst->fd_cdir = fst->fd_rdir = NULL;
		break;
	case STRIOCGETCWD:
		ret = systrace_getcwd(fst, strp);
		break;
	default:
		ret = ENOTTY;
		break;
	}

 unlock:
	rw_exit_write(&fst->lock);
	return (ret);
}

/* ARGSUSED */
int
systracef_poll(struct file *fp, int events, struct proc *p)
{
	struct fsystrace *fst = (struct fsystrace *)fp->f_data;
	int revents = 0;

	if ((events & (POLLIN | POLLRDNORM)) == 0)
		return (0);

	systrace_lock();
	rw_enter_write(&fst->lock);
	systrace_unlock();
	if (!TAILQ_EMPTY(&fst->messages))
		revents = events & (POLLIN | POLLRDNORM);
	else
		selrecord(p, &fst->si);
	rw_exit_write(&fst->lock);

	return (revents);
}

/* ARGSUSED */
int
systracef_kqfilter(struct file *fp, struct knote *kn)
{
	return (1);
}

/* ARGSUSED */
int
systracef_stat(struct file *fp, struct stat *sb, struct proc *p)
{
	return (EOPNOTSUPP);
}

/* ARGSUSED */
int
systracef_close(struct file *fp, struct proc *p)
{
	struct fsystrace *fst = (struct fsystrace *)fp->f_data;
	struct str_process *strp;
	struct str_policy *strpol;

	systrace_lock();
	rw_enter_write(&fst->lock);
	systrace_unlock();

	/* Untrace all processes */
	for (strp = TAILQ_FIRST(&fst->processes); strp;
	    strp = TAILQ_FIRST(&fst->processes)) {
		struct proc *q = strp->proc;

		systrace_detach(strp);
		psignal(q, SIGKILL);
	}

	/* Clean up fork and exit messages */
	for (strp = TAILQ_FIRST(&fst->messages); strp;
	    strp = TAILQ_FIRST(&fst->messages)) {
		TAILQ_REMOVE(&fst->messages, strp, msg_next);
		pool_put(&systr_proc_pl, strp);
	}

	/* Clean up all policies */
	for (strpol = TAILQ_FIRST(&fst->policies); strpol;
	    strpol = TAILQ_FIRST(&fst->policies))
		systrace_closepolicy(fst, strpol);

	/* Release vnodes */
	if (fst->fd_cdir)
		vrele(fst->fd_cdir);
	if (fst->fd_rdir)
		vrele(fst->fd_rdir);
	rw_exit_write(&fst->lock);

	free(fp->f_data, M_XDATA);
	fp->f_data = NULL;

	return (0);
}

void
systraceattach(int n)
{
	pool_init(&systr_proc_pl, sizeof(struct str_process), 0, 0, 0,
	    "strprocpl", NULL);
	pool_init(&systr_policy_pl, sizeof(struct str_policy), 0, 0, 0,
	    "strpolpl", NULL);
	rw_init(&systrace_lck, "systrace");
}

int
systraceopen(dev_t dev, int flag, int mode, struct proc *p)
{
	return (0);
}

int
systraceclose(dev_t dev, int flag, int mode, struct proc *p)
{
	return (0);
}

int
systraceioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct file *f;
	struct fsystrace *fst = NULL;
	int fd, error;

	switch (cmd) {
	case STRIOCCLONE:
		fst = (struct fsystrace *)malloc(sizeof(struct fsystrace),
		    M_XDATA, M_WAITOK | M_ZERO);
		rw_init(&fst->lock, "systrace");
		TAILQ_INIT(&fst->processes);
		TAILQ_INIT(&fst->messages);
		TAILQ_INIT(&fst->policies);

		if (suser(p, 0) == 0)
			fst->issuser = 1;
		fst->p_ruid = p->p_cred->p_ruid;
		fst->p_rgid = p->p_cred->p_rgid;

		error = falloc(p, &f, &fd);
		if (error) {
			free(fst, M_XDATA);
			return (error);
		}
		f->f_flag = FREAD | FWRITE;
		f->f_type = DTYPE_SYSTRACE;
		f->f_ops = &systracefops;
		f->f_data = (caddr_t) fst;
		*(int *)data = fd;
		FILE_SET_MATURE(f);
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

void
systrace_wakeup(struct fsystrace *fst)
{
	wakeup((caddr_t)fst);
	selwakeup(&fst->si);
}

struct proc *
systrace_find(struct str_process *strp)
{
	struct proc *proc;

	if ((proc = pfind(strp->pid)) == NULL)
		return (NULL);

	if (proc != strp->proc)
		return (NULL);

	if (!ISSET(proc->p_flag, P_SYSTRACE))
		return (NULL);

	return (proc);
}

void
systrace_exit(struct proc *proc)
{
	struct str_process *strp;
	struct fsystrace *fst;

	systrace_lock();
	strp = proc->p_systrace;
	if (strp != NULL) {
		fst = strp->parent;
		rw_enter_write(&fst->lock);
		systrace_unlock();

		/* Insert Exit message */
		systrace_msg_child(fst, strp, -1);

		systrace_detach(strp);
		proc->p_systrace = NULL;
		rw_exit_write(&fst->lock);
	} else
		systrace_unlock();
	atomic_clearbits_int(&proc->p_flag, P_SYSTRACE);
}

struct str_process *
systrace_getproc(void)
{
	struct str_process *newstrp;

	newstrp = pool_get(&systr_proc_pl, PR_WAITOK|PR_ZERO);
	newstrp->firstmsg = pool_get(&systr_proc_pl, PR_WAITOK|PR_ZERO);
	return (newstrp);
}

void
systrace_freeproc(struct str_process *strp)
{
	if (strp->firstmsg)
		pool_put(&systr_proc_pl, strp->firstmsg);
	pool_put(&systr_proc_pl, strp);
}

void
systrace_fork(struct proc *oldproc, struct proc *p, struct str_process *newstrp)
{
	struct str_process *oldstrp, *strp;
	struct fsystrace *fst;

	systrace_lock();
	oldstrp = oldproc->p_systrace;
	if (oldstrp == NULL) {
		systrace_unlock();
		systrace_freeproc(newstrp);
		return;
	}

	fst = oldstrp->parent;
	rw_enter_write(&fst->lock);
	systrace_unlock();

	systrace_insert_process(fst, p, newstrp);
	if ((strp = systrace_findpid(fst, p->p_pid)) == NULL)
		panic("systrace_fork");

	/* Reference policy */
	if ((strp->policy = oldstrp->policy) != NULL)
		strp->policy->refcount++;

	/* Insert fork message */
	systrace_msg_child(fst, oldstrp, p->p_pid);
	rw_exit_write(&fst->lock);
}

#define REACQUIRE_LOCK	do { \
	systrace_lock(); \
	strp = p->p_systrace; \
	if (strp == NULL) { \
		systrace_unlock(); \
		return (error); \
	} \
	fst = strp->parent; \
	rw_enter_write(&fst->lock); \
	systrace_unlock(); \
} while (0)

int
systrace_redirect(int code, struct proc *p, void *v, register_t *retval)
{
	struct sysent *callp;
	struct str_process *strp;
	struct str_policy *strpolicy;
	struct fsystrace *fst = NULL;
	struct emul *oldemul;
	struct pcred *pc;
	uid_t olduid;
	gid_t oldgid;
	int policy, error = 0, report = 0, maycontrol = 0, issuser = 0;

	KERNEL_LOCK();
	systrace_lock();
	strp = p->p_systrace;
	if (strp == NULL) {
		systrace_unlock();
		KERNEL_UNLOCK();
		return (EINVAL);
	}

	if (code < 0 || code >= p->p_emul->e_nsysent) {
		systrace_unlock();
		KERNEL_UNLOCK();
		return (EINVAL);
	}

	KASSERT(strp->proc == p);

	fst = strp->parent;

	rw_enter_write(&fst->lock);
	systrace_unlock();

	/*
	 * We can not monitor a SUID process unless we are root,
	 * but we wait until it executes something unprivileged.
	 * A non-root user may only monitor if the real uid and
	 * real gid match the monitored process.  Changing the
	 * uid or gid causes PS_SUGID to be set.
	 */
	if (fst->issuser) {
		maycontrol = 1;
		issuser = 1;
	} else if (!ISSET(p->p_p->ps_flags, PS_SUGID | PS_SUGIDEXEC)) {
		maycontrol = fst->p_ruid == p->p_cred->p_ruid &&
		    fst->p_rgid == p->p_cred->p_rgid;
	}

	if (!maycontrol) {
		policy = SYSTR_POLICY_PERMIT;
	} else {
		/* Find out current policy */
		if ((strpolicy = strp->policy) == NULL)
			policy = SYSTR_POLICY_ASK;
		else {
			if (code >= strpolicy->nsysent)
				policy = SYSTR_POLICY_NEVER;
			else
				policy = strpolicy->sysent[code];
		}
	}

	callp = p->p_emul->e_sysent + code;

	/* Fast-path */
	if (policy != SYSTR_POLICY_ASK) {
		if (policy != SYSTR_POLICY_PERMIT &&
		    policy != SYSTR_POLICY_KILL) {
			if (policy > 0)
				error = policy;
			else
				error = EPERM;
		}
		systrace_replacefree(strp);
		rw_exit_write(&fst->lock);
		if (policy == SYSTR_POLICY_KILL) {
			error = EPERM;
			DPRINTF(("systrace: pid %u killed on syscall %d\n",
			    p->p_pid, code));
			psignal(p, SIGKILL);
		} else if (policy == SYSTR_POLICY_PERMIT)
			error = (*callp->sy_call)(p, v, retval);
		KERNEL_UNLOCK();
		return (error);
	}

	/*
	 * Reset our stackgap allocation.  Note that when resetting
	 * the stackgap allocation, we expect to get the same address
	 * base; i.e. that stackgap_init() is idempotent.
	 */
	systrace_inject(strp, 0 /* Just reset internal state */);
	strp->sg = stackgap_init(p->p_emul);

	/* Puts the current process to sleep, return unlocked */
	error = systrace_msg_ask(fst, strp, code, callp->sy_argsize, v);
	/* lock has been released in systrace_msg_ask() */

	if (error) {
		KERNEL_UNLOCK();
		return (error);
	}

	/* We might have detached by now for some reason */
	systrace_lock();
	if ((strp = p->p_systrace) == NULL) {
		systrace_unlock();
		KERNEL_UNLOCK();
		return (error);
	}

	fst = strp->parent;
	rw_enter_write(&fst->lock);
	systrace_unlock();

	if (strp->answer == SYSTR_POLICY_NEVER) {
		error = strp->error;
		systrace_replacefree(strp);
		goto out_unlock;
	}

	if (ISSET(strp->flags, STR_PROC_SYSCALLRES)) {
		CLR(strp->flags, STR_PROC_SYSCALLRES);
		report = 1;
	}

	error = systrace_inject(strp, 1/* Perform copies */);
	/* Replace the arguments if necessary */
	if (!error && strp->replace != NULL)
		error = systrace_replace(strp, callp->sy_argsize, v);
	if (error)
		goto out_unlock;

	oldemul = p->p_emul;
	pc = p->p_cred;
	olduid = pc->p_ruid;
	oldgid = pc->p_rgid;
		
	/* Elevate privileges as desired */
	if (issuser) {
		if (ISSET(strp->flags, STR_PROC_SETEUID))
			strp->saveuid = systrace_seteuid(p, strp->seteuid);
		if (ISSET(strp->flags, STR_PROC_SETEGID))
			strp->savegid = systrace_setegid(p, strp->setegid);
	} else
		CLR(strp->flags, STR_PROC_SETEUID|STR_PROC_SETEGID);

	rw_exit_write(&fst->lock);
				
	error = (*callp->sy_call)(p, v, retval);

	/* Return to old privileges */
	systrace_lock();
	if ((strp = p->p_systrace) == NULL) {
		systrace_unlock();
		KERNEL_UNLOCK();
		return (error);
	}

	if (issuser) {
		if (ISSET(strp->flags, STR_PROC_SETEUID)) {
			if (pc->pc_ucred->cr_uid == strp->seteuid)
				systrace_seteuid(p, strp->saveuid);
			CLR(strp->flags, STR_PROC_SETEUID);
		}
		if (ISSET(strp->flags, STR_PROC_SETEGID)) {
			if (pc->pc_ucred->cr_gid == strp->setegid)
				systrace_setegid(p, strp->savegid);
			CLR(strp->flags, STR_PROC_SETEGID);
		}
	}

	systrace_replacefree(strp);

	if (ISSET(p->p_p->ps_flags, PS_SUGID | PS_SUGIDEXEC)) {
		if ((fst = strp->parent) == NULL || !fst->issuser) {
			systrace_unlock();
			KERNEL_UNLOCK();
			return (error);
		}
	}

	/* Report change in emulation */

	/* See if we should force a report */
	if (ISSET(strp->flags, STR_PROC_REPORT)) {
		CLR(strp->flags, STR_PROC_REPORT);
		oldemul = NULL;
	}

	/* Acquire lock */
	fst = strp->parent;
	rw_enter_write(&fst->lock);
	systrace_unlock();

	if (p->p_emul != oldemul) {
		/* Old policy is without meaning now */
		if (strp->policy) {
			systrace_closepolicy(fst, strp->policy);
			strp->policy = NULL;
		}
		systrace_msg_emul(fst, strp);

		REACQUIRE_LOCK;
	}

	/* Report if effective uid or gid changed */
	if (olduid != p->p_cred->p_ruid ||
	    oldgid != p->p_cred->p_rgid) {
		systrace_msg_ugid(fst, strp);

		REACQUIRE_LOCK;
	}

	/* Report result from system call */
	if (report) {
		systrace_msg_result(fst, strp, error, code,
		    callp->sy_argsize, v, retval);

		/* not locked */
		goto out;
	}

out_unlock:
	rw_exit_write(&fst->lock);
out:
	KERNEL_UNLOCK();
	return (error);
}

uid_t
systrace_seteuid(struct proc *p,  uid_t euid)
{
	struct pcred *pc = p->p_cred;
	uid_t oeuid = pc->pc_ucred->cr_uid;

	if (pc->pc_ucred->cr_uid == euid)
		return (oeuid);

	/*
	 * Copy credentials so other references do not see our changes.
	 */
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_uid = euid;
	atomic_setbits_int(&p->p_p->ps_flags, PS_SUGID);

	return (oeuid);
}

gid_t
systrace_setegid(struct proc *p,  gid_t egid)
{
	struct pcred *pc = p->p_cred;
	gid_t oegid = pc->pc_ucred->cr_gid;

	if (pc->pc_ucred->cr_gid == egid)
		return (oegid);

	/*
	 * Copy credentials so other references do not see our changes.
	 */
	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_gid = egid;
	atomic_setbits_int(&p->p_p->ps_flags, PS_SUGID);

	return (oegid);
}

/* Called with fst locked */

int
systrace_answer(struct str_process *strp, struct systrace_answer *ans)
{
	int error = 0;

	DPRINTF(("%s: %u: policy %d\n", __func__,
	    ans->stra_pid, ans->stra_policy));

	if (!POLICY_VALID(ans->stra_policy)) {
		error = EINVAL;
		goto out;
	}

	/* Check if answer is in sync with us */
	if (ans->stra_seqnr != strp->seqnr) {
		error = ESRCH;
		goto out;
	}

	if ((error = systrace_processready(strp)) != 0)
		goto out;

	strp->answer = ans->stra_policy;
	strp->error = ans->stra_error;
	if (!strp->error)
		strp->error = EPERM;
	if (ISSET(ans->stra_flags, SYSTR_FLAGS_RESULT))
		SET(strp->flags, STR_PROC_SYSCALLRES);

	/* See if we should elevate privileges for this system call */
	if (ISSET(ans->stra_flags, SYSTR_FLAGS_SETEUID)) {
		SET(strp->flags, STR_PROC_SETEUID);
		strp->seteuid = ans->stra_seteuid;
	}
	if (ISSET(ans->stra_flags, SYSTR_FLAGS_SETEGID)) {
		SET(strp->flags, STR_PROC_SETEGID);
		strp->setegid = ans->stra_setegid;
	}

	/* Clearing the flag indicates to the process that it woke up */
	CLR(strp->flags, STR_PROC_WAITANSWER);
	wakeup(strp);
 out:

	return (error);
}

int
systrace_setscriptname(struct str_process *strp, struct systrace_scriptname *ans)
{
	strlcpy(strp->scriptname,
	    ans->sn_scriptname, sizeof(strp->scriptname));

	return (0);
}

int
systrace_inject(struct str_process *strp, int docopy)
{
	int ind, ret = 0;

	for (ind = 0; ind < strp->injectind; ind++) {
		struct str_inject *inject = &strp->injects[ind];
		if (!ret && docopy &&
		    copyout(inject->kaddr, inject->uaddr, inject->len))
			ret = EINVAL;
		free(inject->kaddr, M_XDATA);
	}

	strp->injectind = 0;
	return (ret);
}

int
systrace_prepinject(struct str_process *strp, struct systrace_inject *inj)
{
	caddr_t udata, kaddr = NULL;
	int ret = 0;
	struct str_inject *inject;

	if (strp->injectind >= SYSTR_MAXINJECTS)
		return (ENOBUFS);

	udata = stackgap_alloc(&strp->sg, inj->stri_len);
	if (udata == NULL)
		return (ENOMEM);

	/*
	 * We have infact forced a maximum length on stri_len because
	 * of the stackgap.
	 */

	kaddr = malloc(inj->stri_len, M_XDATA, M_WAITOK);
	ret = copyin(inj->stri_addr, kaddr, inj->stri_len);
	if (ret) {
		free(kaddr, M_XDATA);
		return (ret);
	}

	inject = &strp->injects[strp->injectind++];
	inject->kaddr = kaddr;
	inject->uaddr = inj->stri_addr = udata;
	inject->len = inj->stri_len;

	return (0);
}

int
systrace_policy(struct fsystrace *fst, struct systrace_policy *pol)
{
	struct str_policy *strpol;
	struct str_process *strp;

	switch(pol->strp_op) {
	case SYSTR_POLICY_NEW:
		DPRINTF(("%s: new, ents %d\n", __func__,
			    pol->strp_maxents));
		if (pol->strp_maxents <= 0 || pol->strp_maxents > 1024)
			return (EINVAL);
		strpol = systrace_newpolicy(fst, pol->strp_maxents);
		if (strpol == NULL)
			return (ENOBUFS);
		pol->strp_num = strpol->nr;
		break;
	case SYSTR_POLICY_ASSIGN:
		DPRINTF(("%s: %d -> pid %d\n", __func__,
			    pol->strp_num, pol->strp_pid));

		/* Find right policy by number */
		TAILQ_FOREACH(strpol, &fst->policies, next)
		    if (strpol->nr == pol->strp_num)
			    break;
		if (strpol == NULL)
			return (EINVAL);

		strp = systrace_findpid(fst, pol->strp_pid);
		if (strp == NULL)
			return (EINVAL);

		/* Check that emulation matches */
		if (strpol->emul && strpol->emul != strp->proc->p_emul)
			return (EINVAL);

		if (strp->policy)
			systrace_closepolicy(fst, strp->policy);
		strp->policy = strpol;

		/* LRU for policy use */
		TAILQ_REMOVE(&fst->policies, strpol, next);
		TAILQ_INSERT_TAIL(&fst->policies, strpol, next);
		strpol->refcount++;

		/* Record emulation for this policy */
		if (strpol->emul == NULL)
			strpol->emul = strp->proc->p_emul;

		break;
	case SYSTR_POLICY_MODIFY:
		DPRINTF(("%s: %d: code %d -> policy %d\n", __func__,
		    pol->strp_num, pol->strp_code, pol->strp_policy));
		if (!POLICY_VALID(pol->strp_policy))
			return (EINVAL);
		TAILQ_FOREACH(strpol, &fst->policies, next)
		    if (strpol->nr == pol->strp_num)
			    break;
		if (strpol == NULL)
			return (EINVAL);
		if (pol->strp_code < 0 || pol->strp_code >= strpol->nsysent)
			return (EINVAL);
		strpol->sysent[pol->strp_code] = pol->strp_policy;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

int
systrace_processready(struct str_process *strp)
{
	if (ISSET(strp->flags, STR_PROC_ONQUEUE))
		return (EBUSY);

	if (!ISSET(strp->flags, STR_PROC_WAITANSWER))
		return (EBUSY);

	if (strp->proc->p_stat != SSLEEP)
		return (EBUSY);

	return (0);
}

int
systrace_getcwd(struct fsystrace *fst, struct str_process *strp)
{
	struct filedesc *myfdp, *fdp;
	int error;

	DPRINTF(("%s: %d\n", __func__, strp->pid));

	error = systrace_processready(strp);
	if (error)
		return (error);

	myfdp = curproc->p_fd;
	fdp = strp->proc->p_fd;
	if (myfdp == NULL || fdp == NULL)
		return (EINVAL);

	/* Store our current values */
	fst->fd_pid = strp->pid;
	fst->fd_cdir = myfdp->fd_cdir;
	fst->fd_rdir = myfdp->fd_rdir;

	if ((myfdp->fd_cdir = fdp->fd_cdir) != NULL)
		vref(myfdp->fd_cdir);
	if ((myfdp->fd_rdir = fdp->fd_rdir) != NULL)
		vref(myfdp->fd_rdir);

	return (0);
}

int
systrace_io(struct str_process *strp, struct systrace_io *io)
{
	struct proc *p = curproc, *t = strp->proc;
	struct uio uio;
	struct iovec iov;
	int error = 0;

	DPRINTF(("%s: %u: %p(%lu)\n", __func__,
	    io->strio_pid, io->strio_offs, (u_long)io->strio_len));

	switch (io->strio_op) {
	case SYSTR_READ:
		uio.uio_rw = UIO_READ;
		break;
	case SYSTR_WRITE:
		uio.uio_rw = UIO_WRITE;
		break;
	default:
		return (EINVAL);
	}

	error = systrace_processready(strp);
	if (error)
		goto out;

	iov.iov_base = io->strio_addr;
	iov.iov_len = io->strio_len;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)(u_long)io->strio_offs;
	uio.uio_resid = io->strio_len;
	uio.uio_segflg = UIO_USERSPACE;
	uio.uio_procp = p;

	error = process_domem(p, t, &uio, PT_WRITE_I);
	io->strio_len -= uio.uio_resid;
 out:

	return (error);
}

int
systrace_attach(struct fsystrace *fst, pid_t pid)
{
	int error = 0;
	struct proc *proc, *p = curproc;
	struct str_process *newstrp;

	if ((proc = pfind(pid)) == NULL) {
		error = ESRCH;
		goto out;
	}

	if (ISSET(proc->p_flag, P_INEXEC)) {
		error = EAGAIN;
		goto out;
	}

	/*
	 * You can't attach to a process if:
	 *	(1) it's the process that's doing the attaching,
	 */
	if (proc->p_pid == p->p_pid) {
		error = EINVAL;
		goto out;
	}

	/*
	 *	(2) it's a system process
	 */
	if (ISSET(proc->p_flag, P_SYSTEM)) {
		error = EPERM;
		goto out;
	}

	/*
	 *	(3) it's being traced already
	 */
	if (ISSET(proc->p_flag, P_SYSTRACE)) {
		error = EBUSY;
		goto out;
	}

	/*
	 *	(4) it's not owned by you, or the last exec
	 *	    gave us setuid/setgid privs (unless
	 *	    you're root), or...
	 *
	 *      [Note: once PS_SUGID or PS_SUGIDEXEC gets set in execve(),
	 *      it stays set until the process does another execve(). Hence
	 *	this prevents a setuid process which revokes its
	 *	special privileges using setuid() from being
	 *	traced. This is good security.]
	 */
	if ((proc->p_cred->p_ruid != p->p_cred->p_ruid ||
		ISSET(proc->p_flag, PS_SUGID | PS_SUGIDEXEC)) &&
	    (error = suser(p, 0)) != 0)
		goto out;

	/*
	 *	(5) ...it's init, which controls the security level
	 *	    of the entire system, and the system was not
	 *          compiled with permanently insecure mode turned
	 *	    on.
	 */
	if ((proc->p_pid == 1) && (securelevel > -1)) {
		error = EPERM;
		goto out;
	}

	newstrp = systrace_getproc();
	systrace_insert_process(fst, proc, newstrp);

 out:
	return (error);
}

void
systrace_execve0(struct proc *p)
{  
	struct str_process *strp;

	systrace_lock();
	strp = p->p_systrace;
	strp->isscript = 0;
	systrace_unlock();
}

void
systrace_execve1(char *path, struct proc *p)
{
	struct str_process *strp;
	struct fsystrace *fst;
	struct str_msg_execve *msg_execve;

	do { 
		systrace_lock();
		strp = p->p_systrace;
		if (strp == NULL) {
			systrace_unlock();
			return;
		}

		msg_execve = &strp->msg.msg_data.msg_execve;
		fst = strp->parent;
		rw_enter_write(&fst->lock);
		systrace_unlock();

		/*
		 * susers will get the execve call anyway.  Also, if
		 * we're not allowed to control the process, escape.
		 */

		if (fst->issuser ||
		    fst->p_ruid != p->p_cred->p_ruid ||
		    fst->p_rgid != p->p_cred->p_rgid) {
			rw_exit_write(&fst->lock);
			return;
		}
		strlcpy(msg_execve->path, path, MAXPATHLEN);
	} while (systrace_make_msg(strp, SYSTR_MSG_EXECVE) != 0);
}

/* Prepare to replace arguments */

int
systrace_preprepl(struct str_process *strp, struct systrace_replace *repl)
{
	size_t len;
	int i, ret = 0;

	ret = systrace_processready(strp);
	if (ret)
		return (ret);

	if (strp->replace != NULL) {
		free(strp->replace, M_XDATA);
		strp->replace = NULL;
	}

	if (repl->strr_nrepl < 0 || repl->strr_nrepl > SYSTR_MAXARGS)
		return (EINVAL);

	for (i = 0, len = 0; i < repl->strr_nrepl; i++) {
		if (repl->strr_argind[i] < 0 ||
		    repl->strr_argind[i] >= SYSTR_MAXARGS)
			return (EINVAL);
		if (repl->strr_offlen[i] == 0)
			continue;
		len += repl->strr_offlen[i];
		if (repl->strr_offlen[i] > SYSTR_MAXREPLEN ||
		    repl->strr_off[i] > SYSTR_MAXREPLEN ||
		    len > SYSTR_MAXREPLEN)
			return (EINVAL);
		if (repl->strr_offlen[i] + repl->strr_off[i] > len)
			return (EINVAL);
	}

	/* Make sure that the length adds up */
	if (repl->strr_len != len)
		return (EINVAL);

	/* Check against a maximum length */
	if (repl->strr_len > SYSTR_MAXREPLEN)
		return (EINVAL);

	strp->replace = (struct systrace_replace *)
	    malloc(sizeof(struct systrace_replace) + len, M_XDATA, M_WAITOK);

	memcpy(strp->replace, repl, sizeof(struct systrace_replace));
	ret = copyin(repl->strr_base, strp->replace + 1, len);
	if (ret) {
		free(strp->replace, M_XDATA);
		strp->replace = NULL;
		return (ret);
	}

	/* Adjust the offset */
	repl = strp->replace;
	repl->strr_base = (caddr_t)(repl + 1);

	return (0);
}

/*
 * Replace the arguments with arguments from the monitoring process.
 */

int
systrace_replace(struct str_process *strp, size_t argsize, register_t args[])
{
	struct systrace_replace *repl = strp->replace;
	caddr_t kdata, kbase;
	caddr_t udata, ubase;
	int i, maxarg, ind, ret = 0;

	maxarg = argsize/sizeof(register_t);
	ubase = stackgap_alloc(&strp->sg, repl->strr_len);
	if (ubase == NULL) {
		ret = EINVAL;
		goto out;
	}

	kbase = repl->strr_base;
	for (i = 0; i < maxarg && i < repl->strr_nrepl; i++) {
		ind = repl->strr_argind[i];
		if (ind < 0 || ind >= maxarg) {
			ret = EINVAL;
			goto out;
		}
		if (repl->strr_offlen[i] == 0) {
			args[ind] = repl->strr_off[i];
			continue;
		}
		kdata = kbase + repl->strr_off[i];
		if (repl->strr_flags[i] & SYSTR_NOLINKS) {
			ret = systrace_fname(strp, kdata, repl->strr_offlen[i]);
			if (ret != 0)
				goto out;
		}
		udata = ubase + repl->strr_off[i];
		if (copyout(kdata, udata, repl->strr_offlen[i])) {
			ret = EINVAL;
			goto out;
		}

		/* Replace the argument with the new address */
		args[ind] = (register_t)udata;
	}

 out:
	return (ret);
}

int
systrace_fname(struct str_process *strp, caddr_t kdata, size_t len)
{
	if (strp->nfname >= SYSTR_MAXFNAME || len < 1)
		return EINVAL;

	strp->fname[strp->nfname] = kdata;
	strp->fname[strp->nfname][len - 1] = '\0';
	strp->nfname++;

	return 0;
}

void
systrace_replacefree(struct str_process *strp)
{
	if (strp->replace != NULL) {
		free(strp->replace, M_XDATA);
		strp->replace = NULL;
	}
	while (strp->nfname > 0) {
		strp->nfname--;
		strp->fname[strp->nfname] = NULL;
	}
}
int
systrace_scriptname(struct proc *p, char *dst)
{
	struct str_process *strp;
	struct fsystrace *fst;
	int error = 0;

	systrace_lock();  
	strp = p->p_systrace;
	fst = strp->parent;

	rw_enter_write(&fst->lock);
	systrace_unlock();

	if (!fst->issuser &&
		(ISSET(p->p_p->ps_flags, PS_SUGID | PS_SUGIDEXEC) ||
		fst->p_ruid != p->p_cred->p_ruid ||
		fst->p_rgid != p->p_cred->p_rgid)) {
		error = EPERM;
		goto out;
	}

	if (strp != NULL) {
		if (strp->scriptname[0] == '\0') {
			error = ENOENT;
			goto out;
		}

		strlcpy(dst, strp->scriptname, MAXPATHLEN);
		strp->isscript = 1;
	}

 out:
	strp->scriptname[0] = '\0';
	rw_exit_write(&fst->lock);

	return (error);
}

void
systrace_namei(struct nameidata *ndp)
{
	struct str_process *strp;
	struct fsystrace *fst;
	struct componentname *cnp = &ndp->ni_cnd;
	size_t i;
	int hamper = 0;

	systrace_lock();
	strp = cnp->cn_proc->p_systrace;
	if (strp != NULL) {
		fst = strp->parent;
		rw_enter_write(&fst->lock);
		systrace_unlock();

		for (i = 0; i < strp->nfname; i++)
			if (strcmp(cnp->cn_pnbuf, strp->fname[i]) == 0) {
				hamper = 1;
				break;
			}

		if (!hamper && strp->isscript &&
		    strcmp(cnp->cn_pnbuf, strp->scriptname) == 0)
			hamper = 1;

		rw_exit_write(&fst->lock);
	} else
		systrace_unlock();

	if (hamper) {
		/* ELOOP if namei() tries to readlink */
		ndp->ni_loopcnt = MAXSYMLINKS;
		cnp->cn_flags &= ~FOLLOW;
		cnp->cn_flags |= NOFOLLOW;
	}
}

struct str_process *
systrace_findpid(struct fsystrace *fst, pid_t pid)
{
	struct str_process *strp;
	struct proc *proc = NULL;

	TAILQ_FOREACH(strp, &fst->processes, next)
	    if (strp->pid == pid)
		    break;

	if (strp == NULL)
		return (NULL);

	proc = systrace_find(strp);

	return (proc ? strp : NULL);
}

int
systrace_detach(struct str_process *strp)
{
	struct proc *proc;
	struct fsystrace *fst = NULL;
	int error = 0;

	DPRINTF(("%s: Trying to detach from %d\n", __func__, strp->pid));

	if ((proc = systrace_find(strp)) != NULL) {
		atomic_clearbits_int(&proc->p_flag, P_SYSTRACE);
		proc->p_systrace = NULL;
	} else
		error = ESRCH;

	if (ISSET(strp->flags, STR_PROC_WAITANSWER)) {
		CLR(strp->flags, STR_PROC_WAITANSWER);
		wakeup(strp);
	}

	fst = strp->parent;
	systrace_wakeup(fst);

	if (ISSET(strp->flags, STR_PROC_ONQUEUE))
		TAILQ_REMOVE(&fst->messages, strp, msg_next);

	TAILQ_REMOVE(&fst->processes, strp, next);
	fst->nprocesses--;

	if (strp->policy)
		systrace_closepolicy(fst, strp->policy);
	systrace_replacefree(strp);
	systrace_freeproc(strp);
	return (error);
}

void
systrace_closepolicy(struct fsystrace *fst, struct str_policy *policy)
{
	if (--policy->refcount)
		return;

	fst->npolicies--;

	if (policy->nsysent)
		free(policy->sysent, M_XDATA);

	TAILQ_REMOVE(&fst->policies, policy, next);

	pool_put(&systr_policy_pl, policy);
}


void
systrace_insert_process(struct fsystrace *fst, struct proc *proc,
	struct str_process *strp)
{
	strp->pid = proc->p_pid;
	strp->proc = proc;
	strp->parent = fst;

	TAILQ_INSERT_TAIL(&fst->processes, strp, next);
	fst->nprocesses++;

	proc->p_systrace = strp;
	atomic_setbits_int(&proc->p_flag, P_SYSTRACE);
}

struct str_policy *
systrace_newpolicy(struct fsystrace *fst, int maxents)
{
	struct str_policy *pol;
	int i;

	if (fst->npolicies > SYSTR_MAX_POLICIES && !fst->issuser) {
		struct str_policy *tmp;

		/* Try to find a policy for freeing */
		TAILQ_FOREACH(tmp, &fst->policies, next) {
			if (tmp->refcount == 1)
				break;
		}

		if (tmp == NULL)
			return (NULL);

		/* Notify userland about freed policy */
		systrace_msg_policyfree(fst, tmp);
		/* Free this policy */
		systrace_closepolicy(fst, tmp);
	}

	pol = pool_get(&systr_policy_pl, PR_NOWAIT|PR_ZERO);
	if (pol == NULL)
		return (NULL);

	DPRINTF(("%s: allocating %d -> %lu\n", __func__,
		     maxents, (u_long)maxents * sizeof(int)));

	pol->sysent = (u_char *)malloc(maxents * sizeof(u_char),
	    M_XDATA, M_WAITOK);
	pol->nsysent = maxents;
	for (i = 0; i < maxents; i++)
		pol->sysent[i] = SYSTR_POLICY_ASK;

	fst->npolicies++;
	pol->nr = fst->npolicynr++;
	pol->refcount = 1;

	TAILQ_INSERT_TAIL(&fst->policies, pol, next);

	return (pol);
}

int
systrace_msg_ask(struct fsystrace *fst, struct str_process *strp,
    int code, size_t argsize, register_t args[])
{
	struct str_msg_ask *msg_ask = &strp->msg.msg_data.msg_ask;
	int i;

	msg_ask->code = code;
	msg_ask->argsize = argsize;
	for (i = 0; i < (argsize/sizeof(register_t)) && i < SYSTR_MAXARGS; i++)
		msg_ask->args[i] = args[i];

	return (systrace_make_msg(strp, SYSTR_MSG_ASK));
}

int
systrace_msg_result(struct fsystrace *fst, struct str_process *strp,
    int error, int code, size_t argsize, register_t args[], register_t rval[])
{
	struct str_msg_ask *msg_ask = &strp->msg.msg_data.msg_ask;
	int i;

	msg_ask->code = code;
	msg_ask->argsize = argsize;
	msg_ask->result = error;
	for (i = 0; i < (argsize/sizeof(register_t)) && i < SYSTR_MAXARGS; i++)
		msg_ask->args[i] = args[i];

	msg_ask->rval[0] = rval[0];
	msg_ask->rval[1] = rval[1];

	return (systrace_make_msg(strp, SYSTR_MSG_RES));
}

int
systrace_msg_emul(struct fsystrace *fst, struct str_process *strp)
{
	struct str_msg_emul *msg_emul = &strp->msg.msg_data.msg_emul;
	struct proc *p = strp->proc;

	memcpy(msg_emul->emul, p->p_emul->e_name, SYSTR_EMULEN);

	return (systrace_make_msg(strp, SYSTR_MSG_EMUL));
}

int
systrace_msg_ugid(struct fsystrace *fst, struct str_process *strp)
{
	struct str_msg_ugid *msg_ugid = &strp->msg.msg_data.msg_ugid;
	struct proc *p = strp->proc;

	msg_ugid->uid = p->p_cred->p_ruid;
	msg_ugid->gid = p->p_cred->p_rgid;

	return (systrace_make_msg(strp, SYSTR_MSG_UGID));
}

int
systrace_make_msg(struct str_process *strp, int type)
{
	struct str_message *msg = &strp->msg;
	struct fsystrace *fst = strp->parent;
	int st, pri;

	pri = PWAIT|PCATCH;
	if (type == SYSTR_MSG_EXECVE)
		pri &= ~PCATCH;

	msg->msg_seqnr = ++strp->seqnr;
	msg->msg_type = type;
	msg->msg_pid = strp->pid;
	if (strp->policy)
		msg->msg_policy = strp->policy->nr;
	else
		msg->msg_policy = -1;

	SET(strp->flags, STR_PROC_WAITANSWER);
	if (ISSET(strp->flags, STR_PROC_ONQUEUE))
		goto out;

	TAILQ_INSERT_TAIL(&fst->messages, strp, msg_next);
	SET(strp->flags, STR_PROC_ONQUEUE);

 out:
	systrace_wakeup(fst);

	/* Release the lock - XXX */
	rw_exit_write(&fst->lock);

	while (1) {
		st = tsleep(strp, pri, "systrmsg", 0);
		if (st != 0)
			return (ERESTART);
		/* If we detach, then everything is permitted */
		if ((strp = curproc->p_systrace) == NULL)
			return (0);
		if (!ISSET(strp->flags, STR_PROC_WAITANSWER))
			break;
	}

	return (0);
}

int
systrace_msg_child(struct fsystrace *fst, struct str_process *strp, pid_t npid)
{
	struct str_process *nstrp;
	struct str_message *msg;
	struct str_msg_child *msg_child;

	if (strp->firstmsg) {
		nstrp = strp->firstmsg;
		strp->firstmsg = NULL;
	} else
		nstrp = pool_get(&systr_proc_pl, PR_WAITOK|PR_ZERO);

	DPRINTF(("%s: %p: pid %d -> pid %d\n", __func__,
		    nstrp, strp->pid, npid));

	msg = &nstrp->msg;
	msg_child = &msg->msg_data.msg_child;

	msg->msg_type = SYSTR_MSG_CHILD;
	msg->msg_pid = strp->pid;
	if (strp->policy)
		msg->msg_policy = strp->policy->nr;
	else
		msg->msg_policy = -1;
	msg_child->new_pid = npid;

	TAILQ_INSERT_TAIL(&fst->messages, nstrp, msg_next);

	systrace_wakeup(fst);

	return (0);
}

int
systrace_msg_policyfree(struct fsystrace *fst, struct str_policy *strpol)
{
	struct str_process *nstrp;
	struct str_message *msg;

	nstrp = pool_get(&systr_proc_pl, PR_WAITOK|PR_ZERO);

	DPRINTF(("%s: free %d\n", __func__, strpol->nr));

	msg = &nstrp->msg;

	msg->msg_type = SYSTR_MSG_POLICYFREE;
	msg->msg_policy = strpol->nr;

	TAILQ_INSERT_TAIL(&fst->messages, nstrp, msg_next);

	systrace_wakeup(fst);

	return (0);
}
