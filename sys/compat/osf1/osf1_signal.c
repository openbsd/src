#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>

#include <sys/syscallargs.h>

#include <compat/osf1/osf1_signal.h>
#include <compat/osf1/osf1_syscallargs.h>
#include <compat/osf1/osf1_util.h>

static void bsd_to_osf1_sigaction __P((const struct sigaction *bsa, 
				       struct osf1_sigaction *osa));
static void osf1_to_bsd_sigaction __P((const struct osf1_sigaction *osa,
				       struct sigaction *bsa));

#define sigemptyset(s)		bzero((s), sizeof(*(s)))
#define sigismember(s, n)	(*(s) & sigmask(n))
#define sigaddset(s, n)		(*(s) |= sigmask(n))

#define	osf1_sigmask(n)		(1 << ((n) - 1))
#define osf1_sigemptyset(s)	bzero((s), sizeof(*(s)))
#define osf1_sigismember(s, n)	(*(s) & sigmask(n))
#define osf1_sigaddset(s, n)	(*(s) |= sigmask(n))

int bsd_to_osf1_sig[] = {
	0,
	OSF1_SIGHUP,
	OSF1_SIGINT,
	OSF1_SIGQUIT,
	OSF1_SIGILL,
	OSF1_SIGTRAP,
	OSF1_SIGABRT,
	OSF1_SIGEMT,
	OSF1_SIGFPE,
	OSF1_SIGKILL,
	OSF1_SIGBUS,
	OSF1_SIGSEGV,
	OSF1_SIGSYS,
	OSF1_SIGPIPE,
	OSF1_SIGALRM,
	OSF1_SIGTERM,
	OSF1_SIGURG,
	OSF1_SIGSTOP,
	OSF1_SIGTSTP,
	OSF1_SIGCONT,
	OSF1_SIGCHLD,
	OSF1_SIGTTIN,
	OSF1_SIGTTOU,
	OSF1_SIGIO,
	OSF1_SIGXCPU,
	OSF1_SIGXFSZ,
	OSF1_SIGVTALRM,
	OSF1_SIGPROF,
	OSF1_SIGWINCH,
	OSF1_SIGINFO,
	OSF1_SIGUSR1,
	OSF1_SIGUSR2,
};

int osf1_to_bsd_sig[] = {
	0,
	SIGHUP,
	SIGINT,
	SIGQUIT,
	SIGILL,
	SIGTRAP,
	SIGABRT,
	SIGEMT,
	SIGFPE,
	SIGKILL,
	SIGBUS,
	SIGSEGV,
	SIGSYS,
	SIGPIPE,
	SIGALRM,
	SIGTERM,
	SIGURG,
	SIGSTOP,
	SIGTSTP,
	SIGCONT,
	SIGCHLD,
	SIGTTIN,
	SIGTTOU,
	SIGIO,
	SIGXCPU,
	SIGXFSZ,
	SIGVTALRM,
	SIGPROF,
	SIGWINCH,
	SIGINFO,
	SIGUSR1,
	SIGUSR2,
};

void
osf1_to_bsd_sigset(oss, bss)
	const osf1_sigset_t *oss;
	sigset_t *bss;
{
	int i, newsig;

	sigemptyset(bss);
	for (i = 1; i < OSF1_NSIG; i++) {
		if (osf1_sigismember(oss, i)) {
			newsig = osf1_to_bsd_sig[i];
			if (newsig)
				sigaddset(bss, newsig);
		}
	}
}


void
bsd_to_osf1_sigset(bss, oss)
	const sigset_t *bss;
	osf1_sigset_t *oss;
{
	int i, newsig;

	osf1_sigemptyset(oss);
	for (i = 1; i < NSIG; i++) {
		if (sigismember(bss, i)) {
			newsig = bsd_to_osf1_sig[i];
			if (newsig)
				osf1_sigaddset(oss, newsig);
		}
	}
}

/*
 * XXX: Only a subset of the flags is currently implemented.
 */
void
osf1_to_bsd_sigaction(osa, bsa)
	const struct osf1_sigaction *osa;
	struct sigaction *bsa;
{

	bsa->sa_handler = osa->sa_handler;
	osf1_to_bsd_sigset(&osa->sa_mask, &bsa->sa_mask);
	bsa->sa_flags = 0;
	if ((osa->sa_flags & OSF1_SA_ONSTACK) != 0)
		bsa->sa_flags |= SA_ONSTACK;
	if ((osa->sa_flags & OSF1_SA_RESTART) != 0)
		bsa->sa_flags |= SA_RESTART;
	if ((osa->sa_flags & OSF1_SA_RESETHAND) != 0)
		bsa->sa_flags |= SA_RESETHAND;
	if ((osa->sa_flags & OSF1_SA_NOCLDSTOP) != 0)
		bsa->sa_flags |= SA_NOCLDSTOP;
	if ((osa->sa_flags & OSF1_SA_NODEFER) != 0)
		bsa->sa_flags |= SA_NODEFER;
}

void
bsd_to_osf1_sigaction(bsa, osa)
	const struct sigaction *bsa;
	struct osf1_sigaction *osa;
{

	osa->sa_handler = bsa->sa_handler;
	bsd_to_osf1_sigset(&bsa->sa_mask, &osa->sa_mask);
	osa->sa_flags = 0;
	if ((bsa->sa_flags & SA_ONSTACK) != 0)
		osa->sa_flags |= SA_ONSTACK;
	if ((bsa->sa_flags & SA_RESTART) != 0)
		osa->sa_flags |= SA_RESTART;
	if ((bsa->sa_flags & SA_NOCLDSTOP) != 0)
		osa->sa_flags |= SA_NOCLDSTOP;
	if ((bsa->sa_flags & SA_NODEFER) != 0)
		osa->sa_flags |= SA_NODEFER;
	if ((bsa->sa_flags & SA_RESETHAND) != 0)
		osa->sa_flags |= SA_RESETHAND;
}

void
osf1_to_bsd_sigaltstack(oss, bss)
	const struct osf1_sigaltstack *oss;
	struct sigaltstack *bss;
{

	bss->ss_sp = oss->ss_sp;
	bss->ss_size = oss->ss_size;
	bss->ss_flags = 0;

	if ((oss->ss_flags & OSF1_SS_DISABLE) != 0)
		bss->ss_flags |= SS_DISABLE;
	if ((oss->ss_flags & OSF1_SS_ONSTACK) != 0)
		bss->ss_flags |= SS_ONSTACK;
}

void
bsd_to_osf1_sigaltstack(bss, oss)
	const struct sigaltstack *bss;
	struct osf1_sigaltstack *oss;
{

	oss->ss_sp = bss->ss_sp;
	oss->ss_size = bss->ss_size;
	oss->ss_flags = 0;

	if ((bss->ss_flags & SS_DISABLE) != 0)
		oss->ss_flags |= OSF1_SS_DISABLE;
	if ((bss->ss_flags & SS_ONSTACK) != 0)
		oss->ss_flags |= OSF1_SS_ONSTACK;
}

int
osf1_sys_sigaction(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_sigaction_args /* {
		syscallarg(int) signum;
		syscallarg(struct osf1_sigaction *) nsa;
		syscallarg(struct osf1_sigaction *) osa;
	} */ *uap = v;
	struct osf1_sigaction *nosa, *oosa, tmposa;
	struct sigaction *nbsa, *obsa, tmpbsa;
	struct sys_sigaction_args sa;
	caddr_t sg;
	int error;

	sg = stackgap_init(p->p_emul);
	nosa = SCARG(uap, nsa);
	oosa = SCARG(uap, osa);

	if (oosa != NULL)
		obsa = stackgap_alloc(&sg, sizeof(struct sigaction));
	else
		obsa = NULL;

	if (nosa != NULL) {
		nbsa = stackgap_alloc(&sg, sizeof(struct sigaction));
		if ((error = copyin(nosa, &tmposa, sizeof(tmposa))) != 0)
			return error;
		osf1_to_bsd_sigaction(&tmposa, &tmpbsa);
		if ((error = copyout(&tmpbsa, nbsa, sizeof(tmpbsa))) != 0)
			return error;
	} else
		nbsa = NULL;

	SCARG(&sa, signum) = osf1_to_bsd_sig[SCARG(uap, signum)];
	SCARG(&sa, nsa) = nbsa;
	SCARG(&sa, osa) = obsa;

	if ((error = sys_sigaction(p, &sa, retval)) != 0)
		return error;

	if (oosa != NULL) {
		if ((error = copyin(obsa, &tmpbsa, sizeof(tmpbsa))) != 0)
			return error;
		bsd_to_osf1_sigaction(&tmpbsa, &tmposa);
		if ((error = copyout(&tmposa, oosa, sizeof(tmposa))) != 0)
			return error;
	}

	return 0;
}

int 
osf1_sys_sigaltstack(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_sigaltstack_args /* {
		syscallarg(struct osf1_sigaltstack *) nss;
		syscallarg(struct osf1_sigaltstack *) oss;
	} */ *uap = v;
	struct osf1_sigaltstack *noss, *ooss, tmposs;
	struct sigaltstack *nbss, *obss, tmpbss;
	struct sys_sigaltstack_args sa;
	caddr_t sg;
	int error;

	sg = stackgap_init(p->p_emul);
	noss = SCARG(uap, nss);
	ooss = SCARG(uap, oss);

	if (ooss != NULL)
		obss = stackgap_alloc(&sg, sizeof(struct sigaltstack));
	else
		obss = NULL;

	if (noss != NULL) {
		nbss = stackgap_alloc(&sg, sizeof(struct sigaltstack));
		if ((error = copyin(noss, &tmposs, sizeof(tmposs))) != 0)
			return error;
		osf1_to_bsd_sigaltstack(&tmposs, &tmpbss);
		if ((error = copyout(&tmpbss, nbss, sizeof(tmpbss))) != 0)
			return error;
	} else
		nbss = NULL;

	SCARG(&sa, nss) = nbss;
	SCARG(&sa, oss) = obss;

	if ((error = sys_sigaltstack(p, &sa, retval)) != 0)
		return error;

	if (obss != NULL) {
		if ((error = copyin(obss, &tmpbss, sizeof(tmpbss))) != 0)
			return error;
		bsd_to_osf1_sigaltstack(&tmpbss, &tmposs);
		if ((error = copyout(&tmposs, ooss, sizeof(tmposs))) != 0)
			return error;
	}

	return 0;
}

#if 0
int
osf1_sys_signal(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_signal_args /* {
		syscallarg(int) signum;
		syscallarg(osf1_sig_t) handler;
	} */ *uap = v;
	int signum = osf1_to_bsd_sig[OSF1_SIGNO(SCARG(uap, signum))];
	int error;
	caddr_t sg = stackgap_init(p->p_emul);

	if (signum <= 0 || signum >= OSF1_NSIG) {
		if (OSF1_SIGCALL(SCARG(uap, signum)) == OSF1_SIGNAL_MASK ||
		    OSF1_SIGCALL(SCARG(uap, signum)) == OSF1_SIGDEFER_MASK)
			*retval = (int)OSF1_SIG_ERR;
		return EINVAL;
	}

	switch (OSF1_SIGCALL(SCARG(uap, signum))) {
	case OSF1_SIGDEFER_MASK:
		/*
		 * sigset is identical to signal() except
		 * that SIG_HOLD is allowed as
		 * an action.
		 */
		if (SCARG(uap, handler) == OSF1_SIG_HOLD) {
			struct sys_sigprocmask_args sa;

			SCARG(&sa, how) = SIG_BLOCK;
			SCARG(&sa, mask) = sigmask(signum);
			return sys_sigprocmask(p, &sa, retval);
		}
		/* FALLTHROUGH */

	case OSF1_SIGNAL_MASK:
		{
			struct sys_sigaction_args sa_args;
			struct sigaction *nbsa, *obsa, sa;

			nbsa = stackgap_alloc(&sg, sizeof(struct sigaction));
			obsa = stackgap_alloc(&sg, sizeof(struct sigaction));
			SCARG(&sa_args, signum) = signum;
			SCARG(&sa_args, nsa) = nbsa;
			SCARG(&sa_args, osa) = obsa;

			sa.sa_handler = SCARG(uap, handler);
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = 0;
#if 0
			if (signum != SIGALRM)
				sa.sa_flags = SA_RESTART;
#endif
			if ((error = copyout(&sa, nbsa, sizeof(sa))) != 0)
				return error;
			if ((error = sys_sigaction(p, &sa_args, retval)) != 0) {
				DPRINTF(("signal: sigaction failed: %d\n",
					 error));
				*retval = (int)OSF1_SIG_ERR;
				return error;
			}
			if ((error = copyin(obsa, &sa, sizeof(sa))) != 0)
				return error;
			*retval = (int)sa.sa_handler;
			return 0;
		}

	case OSF1_SIGHOLD_MASK:
		{
			struct sys_sigprocmask_args sa;

			SCARG(&sa, how) = SIG_BLOCK;
			SCARG(&sa, mask) = sigmask(signum);
			return sys_sigprocmask(p, &sa, retval);
		}

	case OSF1_SIGRELSE_MASK:
		{
			struct sys_sigprocmask_args sa;

			SCARG(&sa, how) = SIG_UNBLOCK;
			SCARG(&sa, mask) = sigmask(signum);
			return sys_sigprocmask(p, &sa, retval);
		}

	case OSF1_SIGIGNORE_MASK:
		{
			struct sys_sigaction_args sa_args;
			struct sigaction *bsa, sa;

			bsa = stackgap_alloc(&sg, sizeof(struct sigaction));
			SCARG(&sa_args, signum) = signum;
			SCARG(&sa_args, nsa) = bsa;
			SCARG(&sa_args, osa) = NULL;

			sa.sa_handler = SIG_IGN;
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = 0;
			if ((error = copyout(&sa, bsa, sizeof(sa))) != 0)
				return error;
			if ((error = sys_sigaction(p, &sa_args, retval)) != 0) {
				DPRINTF(("sigignore: sigaction failed\n"));
				return error;
			}
			return 0;
		}

	case OSF1_SIGPAUSE_MASK:
		{
			struct sys_sigsuspend_args sa;

			SCARG(&sa, mask) = p->p_sigmask & ~sigmask(signum);
			return sys_sigsuspend(p, &sa, retval);
		}

	default:
		return ENOSYS;
	}
}

int
osf1_sys_sigprocmask(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_sigprocmask_args /* {
		syscallarg(int) how;
		syscallarg(osf1_sigset_t *) set;
		syscallarg(osf1_sigset_t *) oset;
	} */ *uap = v;
	osf1_sigset_t oss;
	sigset_t bss;
	int error = 0;

	if (SCARG(uap, oset) != NULL) {
		/* Fix the return value first if needed */
		bsd_to_osf1_sigset(&p->p_sigmask, &oss);
		if ((error = copyout(&oss, SCARG(uap, oset), sizeof(oss))) != 0)
			return error;
	}

	if (SCARG(uap, set) == NULL)
		/* Just examine */
		return 0;

	if ((error = copyin(SCARG(uap, set), &oss, sizeof(oss))) != 0)
		return error;

	osf1_to_bsd_sigset(&oss, &bss);

	(void) splhigh();

	switch (SCARG(uap, how)) {
	case OSF1_SIG_BLOCK:
		p->p_sigmask |= bss & ~sigcantmask;
		break;

	case OSF1_SIG_UNBLOCK:
		p->p_sigmask &= ~bss;
		break;

	case OSF1_SIG_SETMASK:
		p->p_sigmask = bss & ~sigcantmask;
		break;

	default:
		error = EINVAL;
		break;
	}

	(void) spl0();

	return error;
}

int
osf1_sys_sigpending(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_sigpending_args /* {
		syscallarg(osf1_sigset_t *) mask;
	} */ *uap = v;
	sigset_t bss;
	osf1_sigset_t oss;

	bss = p->p_siglist & p->p_sigmask;
	bsd_to_osf1_sigset(&bss, &oss);

	return copyout(&oss, SCARG(uap, mask), sizeof(oss));
}

int
osf1_sys_sigsuspend(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_sigsuspend_args /* {
		syscallarg(osf1_sigset_t *) ss;
	} */ *uap = v;
	osf1_sigset_t oss;
	sigset_t bss;
	struct sys_sigsuspend_args sa;
	int error;

	if ((error = copyin(SCARG(uap, ss), &oss, sizeof(oss))) != 0)
		return error;

	osf1_to_bsd_sigset(&oss, &bss);

	SCARG(&sa, mask) = bss;
	return sys_sigsuspend(p, &sa, retval);
}

int
osf1_sys_kill(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_kill_args /* {
		syscallarg(int) pid;
		syscallarg(int) signum;
	} */ *uap = v;
	struct sys_kill_args ka;

	SCARG(&ka, pid) = SCARG(uap, pid);
	SCARG(&ka, signum) = osf1_to_bsd_sig[SCARG(uap, signum)];
	return sys_kill(p, &ka, retval);
}
#endif
