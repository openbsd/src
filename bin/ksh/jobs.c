/*	$OpenBSD: jobs.c,v 1.1.1.1 1996/08/14 06:19:11 downsj Exp $	*/

/*
 * Process and job control
 */

/*
 * Reworked/Rewritten version of Eric Gisin's/Ron Natalie's code by
 * Larry Bouzane (larry@cs.mun.ca) and hacked again by
 * Michael Rendell (michael@cs.mun.ca)
 *
 * The interface to the rest of the shell should probably be changed
 * to allow use of vfork() when available but that would be way too much
 * work :)
 *
 * Notes regarding the copious ifdefs:
 *	- JOB_SIGS is independent of JOBS - it is defined if there are modern
 *	  signal and wait routines available.  This is prefered, even when
 *	  JOBS is not defined, since the shell will not otherwise notice when
 *	  background jobs die until the shell waits for a foreground process
 *	  to die.
 *	- TTY_PGRP defined iff JOBS is defined - defined if there are tty
 *	  process groups
 *	- NEED_PGRP_SYNC defined iff JOBS is defined - see comment below
 */

#include "sh.h"
#include "ksh_stat.h"
#include "ksh_wait.h"
#include "ksh_times.h"
#include "tty.h"

/* Start of system configuration stuff */

/* We keep CHILD_MAX zombie processes around (exact value isn't critical) */
#ifndef CHILD_MAX
# if defined(HAVE_SYSCONF) && defined(_SC_CHILD_MAX)
#  define CHILD_MAX sysconf(_SC_CHILD_MAX)
# else /* _SC_CHILD_MAX */
#  ifdef _POSIX_CHILD_MAX
#   define CHILD_MAX	((_POSIX_CHILD_MAX) * 2)
#  else /* _POSIX_CHILD_MAX */
#   define CHILD_MAX	20
#  endif /* _POSIX_CHILD_MAX */
# endif /* _SC_CHILD_MAX */
#endif /* !CHILD_MAX */

#ifdef JOBS
# if defined(HAVE_TCSETPGRP) || defined(TIOCSPGRP)
#  define TTY_PGRP
# endif
# ifdef BSD_PGRP
#  define setpgid	setpgrp
#  define getpgID()	getpgrp(0)
# else
#  define getpgID()	getpgrp()
# endif
# if defined(TTY_PGRP) && !defined(HAVE_TCSETPGRP)
int tcsetpgrp ARGS((int fd, pid_t grp));
int tcgetpgrp ARGS((int fd));

int
tcsetpgrp(fd, grp)
	int fd;
	pid_t grp;
{
	return ioctl(fd, TIOCSPGRP, &grp);
}

int
tcgetpgrp(fd)
	int	fd;
{
	int r, grp;

	if ((r = ioctl(fd, TIOCGPGRP, &grp)) < 0)
		return r;
	return grp;
}
# endif /* !HAVE_TCSETPGRP && TIOCSPGRP */
#else /* JOBS */
/* These so we can use ifdef xxx instead of if defined(JOBS) && defined(xxx) */
# undef TTY_PGRP
# undef NEED_PGRP_SYNC
#endif /* JOBS */

/* End of system configuration stuff */


/* Order important! */
#define PRUNNING	0
#define PEXITED		1
#define PSIGNALLED	2
#define PSTOPPED	3

typedef struct proc	Proc;
struct proc {
	Proc	*next;		/* next process in pipeline (if any) */
	int	state;
	WAIT_T	status;		/* wait status */
	pid_t	pid;		/* process id */
	char	command[48];	/* process command string */
};

/* Notify/print flag - j_print() argument */
#define JP_NONE		0	/* don't print anything */
#define JP_SHORT	1	/* print signals processes were killed by */
#define JP_MEDIUM	2	/* print [job-num] -/+ command */
#define JP_LONG		3	/* print [job-num] -/+ pid command */
#define JP_PGRP		4	/* print pgrp */

/* put_job() flags */
#define PJ_ON_FRONT	0	/* at very front */
#define PJ_PAST_STOPPED	1	/* just past any stopped jobs */

/* Job.flags values */
#define JF_STARTED	0x001	/* set when all processes in job are started */
#define JF_WAITING	0x002	/* set if j_waitj() is waiting on job */
#define JF_W_ASYNCNOTIFY 0x004	/* set if waiting and async notification ok */
#define JF_XXCOM	0x008	/* set for `command` jobs */
#define JF_FG		0x010	/* running in foreground (also has tty pgrp) */
#define JF_SAVEDTTY	0x020	/* j->ttystate is valid */
#define JF_CHANGED	0x040	/* process has changed state */
#define JF_KNOWN	0x080	/* $! referenced */
#define JF_ZOMBIE	0x100	/* known, unwaited process */
#define JF_REMOVE	0x200	/* flaged for removal (j_jobs()/j_noityf()) */
#define JF_USETTYMODE	0x400	/* tty mode saved if process exits normally */

typedef struct job Job;
struct job {
	Job	*next;		/* next job in list */
	int	job;		/* job number: %n */
	int	flags;		/* see JF_* */
	int	state;		/* job state */
	int	status;		/* exit status of last process */
	pid_t	pgrp;		/* process group of job */
	pid_t	ppid;		/* pid of process that forked job */
	INT32	age;		/* number of jobs started */
	clock_t	systime;	/* system time used by job */
	clock_t	usrtime;	/* user time used by job */
	Proc	*proc_list;	/* process list */
	Proc	*last_proc;	/* last process in list */
#ifdef TTY_PGRP
	TTY_state ttystate;	/* saved tty state for stopped jobs */
#endif /* TTY_PGRP */
};

/* Flags for j_waitj() */
#define JW_NONE		0x00
#define JW_INTERRUPT	0x01	/* ^C will stop the wait */
#define JW_ASYNCNOTIFY	0x02	/* asynchronous notification during wait ok */
#define JW_STOPPEDWAIT	0x04	/* wait even if job stopped */

/* Error codes for j_lookup() */
#define JL_OK		0
#define JL_NOSUCH	1	/* no such job */
#define JL_AMBIG	2	/* %foo or %?foo is ambiguous */
#define JL_INVALID	3	/* non-pid, non-% job id */

static const char	*const lookup_msgs[] = {
				null,
				"no such job",
				"ambiguous",
				"argument must be %job or process id",
				(char *) 0
			    };
clock_t	j_systime, j_usrtime;	/* user and system time of last j_waitjed job */

static Job		*job_list;	/* job list */
static Job		*last_job;
static Job		*async_job;
static pid_t		async_pid;

static int		nzombie;	/* # of zombies owned by this process */
static INT32		njobs;		/* # of jobs started */
static int		child_max;	/* CHILD_MAX */


#ifdef JOB_SIGS
static sigset_t		sm_default, sm_sigchld;
/* held_sigchld is set if sigchld occurs before a job is completely started */
static int		held_sigchld;
#endif /* JOB_SIGS */

#ifdef JOBS
static struct shf	*shl_j;
#endif /* JOBS */

#ifdef NEED_PGRP_SYNC
/* On some systems, the kernel doesn't count zombie processes when checking
 * if a process group is valid, which can cause problems in creating the
 * pipeline "cmd1 | cmd2": if cmd1 can die (and go into the zombie state)
 * before cmd2 is started, the kernel doesn't allow the setpgid() for cmd2
 * to succeed.  Solution is to create a pipe between the parent and the first
 * process; the first process doesn't do anything until the pipe is closed
 * and the parent doesn't close the pipe until all the processes are started.
 */
static int		j_sync_pipe[2];
static int		j_sync_open;
#endif /* NEED_PGRP_SYNC */

#ifdef TTY_PGRP
static int		ttypgrp_ok;	/* set if can use tty pgrps */
static pid_t		restore_ttypgrp = -1;
static pid_t		our_pgrp;
static int const	tt_sigs[] = { SIGTSTP, SIGTTIN, SIGTTOU };
#endif /* TTY_PGRP */

static void		j_set_async ARGS((Job *j));
static void		j_startjob ARGS((Job *j));
static int		j_waitj ARGS((Job *j, int flags, const char *where));
static RETSIGTYPE	j_sigchld ARGS((int sig));
static void		j_print ARGS((Job *j, int how, struct shf *shf));
static Job		*j_lookup ARGS((const char *cp, int *ecodep));
static Job		*new_job ARGS((void));
static Proc		*new_proc ARGS((void));
static void		check_job ARGS((Job *j));
static void		put_job ARGS((Job *j, int where));
static void		remove_job ARGS((Job *j, const char *where));
static void		kill_job ARGS((Job *j));
static void	 	fill_command ARGS((char *c, int len, struct op *t));

/* initialize job control */
void
j_init(mflagset)
	int mflagset;
{
	child_max = CHILD_MAX; /* so syscon() isn't always being called */

#ifdef JOB_SIGS
	sigemptyset(&sm_default);
	sigprocmask(SIG_SETMASK, &sm_default, (sigset_t *) 0);

	sigemptyset(&sm_sigchld);
	sigaddset(&sm_sigchld, SIGCHLD);

	setsig(&sigtraps[SIGCHLD], j_sigchld, SS_RESTORE_ORIG|SS_FORCE);
#else /* JOB_SIGS */
	/* Make sure SIGCHLD isn't ignored - can do odd things under SYSV */
	setsig(&sigtraps[SIGCHLD], SIG_DFL, SS_RESTORE_ORIG|SS_FORCE);
#endif /* JOB_SIGS */

#ifdef JOBS
	if (!mflagset && Flag(FTALKING))
		Flag(FMONITOR) = 1;

	/* shl_j is used to do asynchronous notification (used in
	 * an interrupt handler, so need a distinct shf)
	 */
	shl_j = shf_fdopen(2, SHF_WR, (struct shf *) 0);

# ifdef TTY_PGRP
	if (Flag(FMONITOR) || Flag(FTALKING)) {
		int i;

		/* j_change() sets these to SS_RESTORE_DFL if FMONITOR */
		for (i = NELEM(tt_sigs); --i >= 0; ) {
			sigtraps[tt_sigs[i]].flags |= TF_SHELL_USES;
			setsig(&sigtraps[tt_sigs[i]], SIG_IGN,
				SS_RESTORE_IGN|SS_FORCE);
		}
	}
# endif /* TTY_PGRP */

	/* j_change() calls tty_init() */
	if (Flag(FMONITOR))
		j_change();
	else
#endif /* JOBS */
	  if (Flag(FTALKING))
		tty_init(TRUE);
}

/* job cleanup before shell exit */
void
j_exit()
{
	/* kill stopped, and possibly running, jobs */
	Job	*j;
	int	killed = 0;

	for (j = job_list; j != (Job *) 0; j = j->next) {
		if (j->ppid == procpid
		    && (j->state == PSTOPPED
			|| (j->state == PRUNNING
			    && ((j->flags & JF_FG)
				|| (Flag(FLOGIN) && !Flag(FNOHUP)
				    && procpid == kshpid)))))
		{
			killed = 1;
			killpg(j->pgrp, SIGHUP);
#ifdef JOBS
			if (j->state == PSTOPPED)
				killpg(j->pgrp, SIGCONT);
#endif /* JOBS */
		}
	}
	if (killed)
		sleep(1);
	j_notify();

#ifdef JOBS
# ifdef TTY_PGRP
	if (kshpid == procpid && restore_ttypgrp >= 0) {
		/* Need to restore the tty pgrp to what it was when the
		 * shell started up, so that the process that started us
		 * will be able to access the tty when we are done.
		 * Also need to restore our process group in case we are
		 * about to do an exec so that both our parent and the
		 * process we are to become will be able to access the tty.
		 */
		tcsetpgrp(tty_fd, restore_ttypgrp);
		setpgid(0, restore_ttypgrp);
	}
# endif /* TTY_PGRP */
	if (Flag(FMONITOR)) {
		Flag(FMONITOR) = 0;
		j_change();
	}
#endif /* JOBS */
}

#ifdef JOBS
/* turn job control on or off according to Flag(FMONITOR) */
void
j_change()
{
	int i;

	if (Flag(FMONITOR)) {
		/* Don't call get_tty() 'til we own the tty process group */
		tty_init(FALSE);

# ifdef TTY_PGRP
		/* no controlling tty, no SIGT* */
		ttypgrp_ok = tty_fd >= 0 && tty_devtty;

		if (ttypgrp_ok && (our_pgrp = getpgID()) < 0) {
			warningf(FALSE, "j_init: getpgrp() failed: %s",
				strerror(errno));
			ttypgrp_ok = 0;
		}
		if (ttypgrp_ok) {
			setsig(&sigtraps[SIGTTIN], SIG_DFL,
				SS_RESTORE_ORIG|SS_FORCE);
			/* wait to be given tty (POSIX.1, B.2, job control) */
			while (1) {
				pid_t ttypgrp;

				if ((ttypgrp = tcgetpgrp(tty_fd)) < 0) {
					warningf(FALSE,
					"j_init: tcgetpgrp() failed: %s",
						strerror(errno));
					ttypgrp_ok = 0;
					break;
				}
				if (ttypgrp == our_pgrp)
					break;
				kill(0, SIGTTIN);
			}
		}
		for (i = NELEM(tt_sigs); --i >= 0; )
			setsig(&sigtraps[tt_sigs[i]], SIG_IGN,
				SS_RESTORE_DFL|SS_FORCE);
		if (ttypgrp_ok && our_pgrp != kshpid) {
			if (setpgid(0, kshpid) < 0) {
				warningf(FALSE,
					"j_init: setpgid() failed: %s",
					strerror(errno));
				ttypgrp_ok = 0;
			} else {
				if (tcsetpgrp(tty_fd, kshpid) < 0) {
					warningf(FALSE,
					"j_init: tcsetpgrp() failed: %s",
						strerror(errno));
					ttypgrp_ok = 0;
				} else
					restore_ttypgrp = our_pgrp;
				our_pgrp = kshpid;
			}
		}
#  if defined(NTTYDISC) && defined(TIOCSETD) && !defined(HAVE_TERMIOS_H) && !defined(HAVE_TERMIO_H)
		if (ttypgrp_ok) {
			int ldisc = NTTYDISC;

			if (ioctl(tty_fd, TIOCSETD, &ldisc) < 0)
				warningf(FALSE,
				"j_init: can't set new line discipline: %s",
					strerror(errno));
		}
#  endif /* NTTYDISC && TIOCSETD */
		if (!ttypgrp_ok)
			warningf(FALSE, "warning: won't have full job control");
# endif /* TTY_PGRP */
		if (tty_fd >= 0)
			get_tty(tty_fd, &tty_state);
	} else {
# ifdef TTY_PGRP
		ttypgrp_ok = 0;
		/* the TF_SHELL_USES test is a kludge that lets us know if
		 * if the signals have been changed by the shell.
		 */
		if (Flag(FTALKING))
			for (i = NELEM(tt_sigs); --i >= 0; )
				setsig(&sigtraps[tt_sigs[i]], SIG_IGN,
					SS_RESTORE_IGN|SS_FORCE);
		else
			for (i = NELEM(tt_sigs); --i >= 0; ) {
				if (sigtraps[tt_sigs[i]].flags & (TF_ORIG_IGN
							          |TF_ORIG_DFL))
					setsig(&sigtraps[tt_sigs[i]],
						(sigtraps[tt_sigs[i]].flags & TF_ORIG_IGN) ? SIG_IGN : SIG_DFL,
						SS_RESTORE_CURR|SS_FORCE);
			}
# endif /* TTY_PGRP */
		if (!Flag(FTALKING))
			tty_close();
	}
}
#endif /* JOBS */

/* execute tree in child subprocess */
int
exchild(t, flags, close_fd)
	struct op	*t;
	int		flags;
	int		close_fd;	/* used if XPCLOSE or XCCLOSE */
{
	static Proc	*last_proc;	/* for pipelines */

	int		i;
#ifdef JOB_SIGS
	sigset_t	omask;
#endif /* JOB_SIGS */
	Proc		*p;
	Job		*j;
	int		rv = 0;
	int		forksleep;
	int		orig_flags = flags;
	int		ischild;

	flags &= ~(XFORK|XPCLOSE|XCCLOSE|XCOPROC);
	if (flags & XEXEC)
		return execute(t, flags);

#ifdef JOB_SIGS
	/* no SIGCHLD's while messing with job and process lists */
	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif /* JOB_SIGS */

	p = new_proc();
	p->next = (Proc *) 0;
	p->state = PRUNNING;
	WSTATUS(p->status) = 0;
	p->pid = 0;

	/* link process into jobs list */
	if (flags&XPIPEI) {	/* continuing with a pipe */
		j = last_job;
		last_proc->next = p;
		last_proc = p;
	} else {
#ifdef NEED_PGRP_SYNC
		if (j_sync_open) {
			closepipe(j_sync_pipe);
			j_sync_open = 0;
		}
		/* don't do the sync pipe business if there is no pipeline */
		if (flags & XPIPEO) {
			openpipe(j_sync_pipe);
			j_sync_open = 1;
		}
#endif /* NEED_PGRP_SYNC */
		j = new_job(); /* fills in j->job */
		/* we don't consider XXCOM's foreground since they don't get
		 * tty process group and we don't save or restore tty modes.
		 */
		j->flags = (flags & XXCOM) ? JF_XXCOM
			: ((flags & XBGND) ? 0 : (JF_FG|JF_USETTYMODE));
		j->usrtime = j->systime = 0;
		j->state = PRUNNING;
		j->pgrp = 0;
		j->ppid = procpid;
		j->age = ++njobs;
		j->proc_list = p;
		last_job = j;
		last_proc = p;
		if (flags & XXCOM)
			j->flags |= JF_XXCOM;
		else if (!(flags & XBGND))
			j->flags |= JF_FG;
		put_job(j, PJ_PAST_STOPPED);
	}

	fill_command(p->command, sizeof(p->command), t);

	/* create child process */
	forksleep = 1;
	while ((i = fork()) < 0 && errno == EAGAIN && forksleep < 32) {
		if (intrsig)	 /* allow user to ^C out... */
			break;
		sleep(forksleep);
		forksleep <<= 1;
	}
	if (i < 0) {
		kill_job(j);
		remove_job(j, "fork failed");
#ifdef NEED_PGRP_SYNC
		if (j_sync_open) {
			closepipe(j_sync_pipe);
			j_sync_open = 0;
		}
#endif /* NEED_PGRP_SYNC */
#ifdef JOB_SIGS
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */
		errorf("cannot fork - try again");
	}
	ischild = i == 0;
	if (ischild)
		p->pid = procpid = getpid();
	else
		p->pid = i;

#ifdef JOBS
	/* job control set up */
	if (Flag(FMONITOR) && !(flags&XXCOM)) {
		int	dotty = 0;
# ifdef NEED_PGRP_SYNC
		int	dosync = 0;
# endif /* NEED_PGRP_SYNC */

		if (j->pgrp == 0) {	/* First process */
			j->pgrp = p->pid;
			dotty = 1;
# ifdef NEED_PGRP_SYNC
			if (j_sync_open) {
				close(j_sync_pipe[ischild ? 1 : 0]);
				j_sync_pipe[ischild ? 1 : 0] = -1;
				dosync = ischild;
			}
# endif /* NEED_PGRP_SYNC */
		}

		/* set pgrp in both parent and child to deal with race
		 * condition
		 */
		setpgid(p->pid, j->pgrp);
# ifdef TTY_PGRP
		/* YYY: should this be
		   if (ttypgrp_ok && ischild && !(flags&XBGND))
			tcsetpgrp(tty_fd, j->pgrp);
		   instead? (see also YYY below)
		 */
		if (ttypgrp_ok && dotty && !(flags & XBGND))
			tcsetpgrp(tty_fd, j->pgrp);
# endif /* TTY_PGRP */
# ifdef NEED_PGRP_SYNC
		if (ischild && j_sync_open) {
			if (dosync) {
				char c;
				while (read(j_sync_pipe[0], &c, 1) == -1
				       && errno == EINTR)
					;
			}
			close(j_sync_pipe[0]);
			j_sync_open = 0;
		}
# endif /* NEED_PGRP_SYNC */
	}
#endif /* JOBS */

	/* used to close pipe input fd */
	if (close_fd >= 0 && (((orig_flags & XPCLOSE) && i != 0)
			      || ((orig_flags & XCCLOSE) && i == 0)))
		close(close_fd);
	if (i == 0) {		/* child */
#ifdef JOB_SIGS
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */
		cleanup_parents_env();
#ifdef KSH
		if (orig_flags & XCOPROC)
			cleanup_coproc(FALSE);
#endif /* KSH */
#ifdef TTY_PGRP
		/* If FMONITOR or FTALKING is set, these signals are ignored,
		 * if neither FMONITOR nor FTALKING are set, the signals have
		 * their inherited values.
		 */
		if (Flag(FMONITOR) && !(flags & XXCOM)) {
			for (i = NELEM(tt_sigs); --i >= 0; )
				setsig(&sigtraps[tt_sigs[i]], SIG_DFL,
					SS_RESTORE_DFL|SS_FORCE);
		}
#endif /* TTY_PGRP */
#ifdef HAVE_NICE
		if (Flag(FBGNICE) && (flags & XBGND))
			nice(4);
#endif /* HAVE_NICE */
		if ((flags & XBGND) && !Flag(FMONITOR)) {
			setsig(&sigtraps[SIGINT], SIG_IGN,
				SS_RESTORE_IGN|SS_FORCE);
			setsig(&sigtraps[SIGQUIT], SIG_IGN,
				SS_RESTORE_IGN|SS_FORCE);
			if (!(orig_flags & (XPIPEI | XCOPROC))) {
				i = open("/dev/null", 0);
				(void) ksh_dup2(i, 0, TRUE);
				close(i);
			}
		}
		remove_job(j, "child");	/* in case of `jobs` command */
		nzombie = 0;
#ifdef JOBS
		ttypgrp_ok = 0;
		Flag(FMONITOR) = 0;
#endif /* JOBS */
		Flag(FTALKING) = 0;
		tty_close();
		cleartraps();
		execute(t, flags|XEXEC); /* no return */
		internal_errorf(0, "exchild: execute() returned");
		unwind(LLEAVE);
		/* NOTREACHED */
	}

	/* shell (parent) stuff */
	if (!(flags & XPIPEO)) {	/* last process in a job */
#ifdef TTY_PGRP
		/* YYY: Is this needed? (see also YYY above)
		   if (Flag(FMONITOR) && !(flags&(XXCOM|XBGND)))
			tcsetpgrp(tty_fd, j->pgrp);
		*/
#endif /* TTY_PGRP */
		j_startjob(j);
#ifdef KSH
		if (flags & XCOPROC)
			coproc.job = (void *) j;
#endif /* KSH */
		if (flags & XBGND) {
			j_set_async(j);
			if (Flag(FTALKING)) {
				shf_fprintf(shl_out, "[%d]", j->job);
				for (p = j->proc_list; p; p = p->next)
					shf_fprintf(shl_out, " %d", p->pid);
				shf_putchar('\n', shl_out);
				shf_flush(shl_out);
			}
		} else
			rv = j_waitj(j, JW_NONE, "jw:last proc");
	}

#ifdef JOB_SIGS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */

	return rv;
}

/* start the last job: only used for `command` jobs */
void
startlast()
{
#ifdef JOB_SIGS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif /* JOB_SIGS */

	if (last_job) { /* no need to report error - waitlast() will do it */
		/* ensure it isn't removed by check_job() */
		last_job->flags |= JF_WAITING;
		j_startjob(last_job);
	}
#ifdef JOB_SIGS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */
}

/* wait for last job: only used for `command` jobs */
int
waitlast()
{
	int	rv;
	Job	*j;
#ifdef JOB_SIGS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif /* JOB_SIGS */

	j = last_job;
	if (!j || !(j->flags & JF_STARTED)) {
		if (!j)
			warningf(TRUE, "waitlast: no last job");
		else
			internal_errorf(0, "waitlast: not started");
#ifdef JOB_SIGS
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */
		return 125; /* not so arbitrary, non-zero value */
	}

	rv = j_waitj(j, JW_NONE, "jw:waitlast");

#ifdef JOB_SIGS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */

	return rv;
}

/* wait for child, interruptable. */
int
waitfor(cp, sigp)
	const char *cp;
	int	*sigp;
{
	int	rv;
	Job	*j;
	int	ecode;
	int	flags = JW_INTERRUPT|JW_ASYNCNOTIFY;
#ifdef JOB_SIGS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif /* JOB_SIGS */

	*sigp = 0;

	if (cp == (char *) 0) {
		/* wait for an unspecified job - always returns 0, so
		 * don't have to worry about exited/signaled jobs
		 */
		for (j = job_list; j; j = j->next)
			/* at&t ksh will wait for stopped jobs - we don't */
			if (j->ppid == procpid && j->state == PRUNNING)
				break;
		if (!j) {
#ifdef JOB_SIGS
			sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */
			return -1;
		}
	} else if ((j = j_lookup(cp, &ecode))) {
		/* don't report normal job completion */
		flags &= ~JW_ASYNCNOTIFY;
		if (j->ppid != procpid) {
#ifdef JOB_SIGS
			sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */
			return -1;
		}
	} else {
#ifdef JOB_SIGS
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */
		if (ecode == JL_NOSUCH)
			return -1;
		bi_errorf("%s: %s", cp, lookup_msgs[ecode]);
	}

	/* at&t ksh will wait for stopped jobs - we don't */
	rv = j_waitj(j, flags, "jw:waitfor");

#ifdef JOB_SIGS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */

	if (rv < 0) /* we were interrupted */
		*sigp = 128 + -rv;

	return rv;
}

/* kill (built-in) a job */
int
j_kill(cp, sig)
	const char *cp;
	int	sig;
{
	Job	*j;
	Proc	*p;
	int	rv = 0;
	int	ecode;
#ifdef JOB_SIGS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif /* JOB_SIGS */

	if ((j = j_lookup(cp, &ecode)) == (Job *) 0) {
#ifdef JOB_SIGS
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */
		bi_errorf("%s: %s", cp, lookup_msgs[ecode]);
		return 1;
	}

	if (j->pgrp == 0) {	/* started when !Flag(FMONITOR) */
		for (p=j->proc_list; p != (Proc *) 0; p = p->next)
			if (kill(p->pid, sig) < 0) {
				bi_errorf("%s: %s", cp, strerror(errno));
				rv = 1;
			}
	} else {
#ifdef JOBS
		if (j->state == PSTOPPED && (sig == SIGTERM || sig == SIGHUP))
			(void) killpg(j->pgrp, SIGCONT);
#endif /* JOBS */
		if (killpg(j->pgrp, sig) < 0) {
			bi_errorf("%s: %s", cp, strerror(errno));
			rv = 1;
		}
	}

#ifdef JOB_SIGS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */

	return rv;
}

#ifdef JOBS
/* fg and bg built-ins: called only if Flag(FMONITOR) set */
int
j_resume(cp, bg)
	const char *cp;
	int	bg;
{
	Job	*j;
	Proc	*p;
	int	ecode;
	int	running;
	int	rv = 0;
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);

	if ((j = j_lookup(cp, &ecode)) == (Job *) 0) {
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
		bi_errorf("%s: %s", cp, lookup_msgs[ecode]);
		return 1;
	}

	if (j->pgrp == 0) {
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
		bi_errorf("job not job-controlled");
		return 1;
	}

	if (bg)
		shprintf("[%d] ", j->job);

	running = 0;
	for (p = j->proc_list; p != (Proc *) 0; p = p->next) {
		if (p->state == PSTOPPED) {
			p->state = PRUNNING;
			WSTATUS(p->status) = 0;
			running = 1;
		}
		shprintf("%s%s", p->command, p->next ? "| " : null);
	}
	shprintf(newline);
	shf_flush(shl_stdout);
	if (running)
		j->state = PRUNNING;

	put_job(j, PJ_PAST_STOPPED);
	if (bg)
		j_set_async(j);
	else {
# ifdef TTY_PGRP
		/* attach tty to job */
		if (j->state == PRUNNING) {
			if (ttypgrp_ok && (j->flags & JF_SAVEDTTY)) {
				set_tty(tty_fd, &j->ttystate, TF_NONE);
			}
			if (ttypgrp_ok && tcsetpgrp(tty_fd, j->pgrp) < 0) {
				if (j->flags & JF_SAVEDTTY)
					set_tty(tty_fd, &tty_state, TF_NONE);
				sigprocmask(SIG_SETMASK, &omask,
					(sigset_t *) 0);
				bi_errorf("1st tcsetpgrp(%d, %d) failed: %s",
					tty_fd, (int) j->pgrp, strerror(errno));
				return 1;
			}
		}
# endif /* TTY_PGRP */
		j->flags |= JF_FG;
		j->flags &= ~JF_KNOWN;
		if (j == async_job)
			async_job = (Job *) 0;
	}

	if (j->state == PRUNNING && killpg(j->pgrp, SIGCONT) < 0) {
		int	err = errno;

		if (!bg) {
			j->flags &= ~JF_FG;
# ifdef TTY_PGRP
			if (ttypgrp_ok && (j->flags & JF_SAVEDTTY))
				set_tty(tty_fd, &tty_state, TF_NONE);
			if (ttypgrp_ok && tcsetpgrp(tty_fd, our_pgrp) < 0) {
				warningf(TRUE,
				"fg: 2nd tcsetpgrp(%d, %d) failed: %s",
					tty_fd, (int) our_pgrp,
					strerror(errno));
			}
# endif /* TTY_PGRP */
		}
		sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
		bi_errorf("cannot continue job %s: %s",
			cp, strerror(err));
		return 1;
	}
	if (!bg) {
# ifdef TTY_PGRP
		if (ttypgrp_ok) {
			j->flags &= ~JF_SAVEDTTY;
		}
# endif /* TTY_PGRP */
		rv = j_waitj(j, JW_NONE, "jw:resume");
	}
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
	return rv;
}
#endif /* JOBS */

/* are there any running or stopped jobs ? */
int
j_stopped_running()
{
	Job	*j;
	int	which = 0;

	for (j = job_list; j != (Job *) 0; j = j->next) {
#ifdef JOBS
		if (j->ppid == procpid && j->state == PSTOPPED)
			which |= 1;
#endif /* JOBS */
		if (Flag(FLOGIN) && !Flag(FNOHUP) && procpid == kshpid
		    && j->ppid == procpid && j->state == PRUNNING)
			which |= 2;
	}
	if (which) {
		shellf("You have %s%s%s jobs\n",
			which & 1 ? "stopped" : "",
			which == 3 ? " and " : "",
			which & 2 ? "running" : "");
		return 1;
	}

	return 0;
}

/* list jobs for jobs built-in */
int
j_jobs(cp, slp, nflag)
	const char *cp;
	int	slp;		/* 0: short, 1: long, 2: pgrp */
	int	nflag;
{
	Job	*j, *tmp;
	int	how;
	int	zflag = 0;
#ifdef JOB_SIGS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif /* JOB_SIGS */

	if (nflag < 0) { /* kludge: print zombies */
		nflag = 0;
		zflag = 1;
	}
	if (cp) {
		int	ecode;

		if ((j = j_lookup(cp, &ecode)) == (Job *) 0) {
#ifdef JOB_SIGS
			sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */
			bi_errorf("%s: %s", cp, lookup_msgs[ecode]);
			return 1;
		}
	} else
		j = job_list;
	how = slp == 0 ? JP_MEDIUM : (slp == 1 ? JP_LONG : JP_PGRP);
	for (; j; j = j->next) {
		if ((!(j->flags & JF_ZOMBIE) || zflag)
		    && (!nflag || (j->flags & JF_CHANGED)))
		{
			j_print(j, how, shl_stdout);
			if (j->state == PEXITED || j->state == PSIGNALLED)
				j->flags |= JF_REMOVE;
		}
		if (cp)
			break;
	}
	/* Remove jobs after printing so there won't be multiple + or - jobs */
	for (j = job_list; j; j = tmp) {
		tmp = j->next;
		if (j->flags & JF_REMOVE)
			remove_job(j, "jobs");
	}
#ifdef JOB_SIGS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */
	return 0;
}

/* list jobs for top-level notification */
void
j_notify()
{
	Job	*j, *tmp;
#ifdef JOB_SIGS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif /* JOB_SIGS */
	for (j = job_list; j; j = j->next) {
#ifdef JOBS
		if (Flag(FMONITOR) && (j->flags & JF_CHANGED))
			j_print(j, JP_MEDIUM, shl_out);
#endif /* JOBS */
		/* Remove job after doing reports so there aren't
		 * multiple +/- jobs.
		 */
		if (j->state == PEXITED || j->state == PSIGNALLED)
			j->flags |= JF_REMOVE;
	}
	for (j = job_list; j; j = tmp) {
		tmp = j->next;
		if (j->flags & JF_REMOVE)
			remove_job(j, "notify");
	}
	shf_flush(shl_out);
#ifdef JOB_SIGS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */
}

/* Return pid of last process in last asynchornous job */
pid_t
j_async()
{
#ifdef JOB_SIGS
	sigset_t omask;

	sigprocmask(SIG_BLOCK, &sm_sigchld, &omask);
#endif /* JOB_SIGS */

	if (async_job)
		async_job->flags |= JF_KNOWN;

#ifdef JOB_SIGS
	sigprocmask(SIG_SETMASK, &omask, (sigset_t *) 0);
#endif /* JOB_SIGS */

	return async_pid;
}

/* Make j the last async process
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
j_set_async(j)
	Job *j;
{
	Job	*jl, *oldest;

	if (async_job && (async_job->flags & (JF_KNOWN|JF_ZOMBIE)) == JF_ZOMBIE)
		remove_job(async_job, "async");
	if (!(j->flags & JF_STARTED)) {
		internal_errorf(0, "j_async: job not started");
		return;
	}
	async_job = j;
	async_pid = j->last_proc->pid;
	while (nzombie > child_max) {
		oldest = (Job *) 0;
		for (jl = job_list; jl; jl = jl->next)
			if (jl != async_job && (jl->flags & JF_ZOMBIE)
			    && (!oldest || jl->age < oldest->age))
				oldest = jl;
		if (!oldest) {
			/* XXX debugging */
			if (!(async_job->flags & JF_ZOMBIE) || nzombie != 1) {
				internal_errorf(0, "j_async: bad nzombie (%d)", nzombie);
				nzombie = 0;
			}
			break;
		}
		remove_job(oldest, "zombie");
	}
}

/* Start a job: set STARTED, check for held signals and set j->last_proc
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
j_startjob(j)
	Job *j;
{
	Proc	*p;

	j->flags |= JF_STARTED;
	for (p = j->proc_list; p->next; p = p->next)
		;
	j->last_proc = p;

#ifdef NEED_PGRP_SYNC
	if (j_sync_open) {
		closepipe(j_sync_pipe);
		j_sync_open = 0;
	}
#endif /* NEED_PGRP_SYNC */
#ifdef JOB_SIGS
	if (held_sigchld) {
		held_sigchld = 0;
		/* Don't call j_sigchild() as it may remove job... */
		kill(procpid, SIGCHLD);
	}
#endif /* JOB_SIGS */
}

/*
 * wait for job to complete or change state
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static int
j_waitj(j, flags, where)
	Job	*j;
	int	flags;		/* see JW_* */
	const char *where;
{
	int	rv;

	/*
	 * No auto-notify on the job we are waiting on.
	 */
	j->flags |= JF_WAITING;
	if (flags & JW_ASYNCNOTIFY)
		j->flags |= JF_W_ASYNCNOTIFY;

	if (!Flag(FMONITOR))
		flags |= JW_STOPPEDWAIT;

	while ((volatile int) j->state == PRUNNING
		|| ((flags & JW_STOPPEDWAIT)
		    && (volatile int) j->state == PSTOPPED))
	{
#ifdef JOB_SIGS
		sigsuspend(&sm_default);
#else /* JOB_SIGS */
		j_sigchld(SIGCHLD);
#endif /* JOB_SIGS */
		if (fatal_trap) {
			int oldf = j->flags & (JF_WAITING|JF_W_ASYNCNOTIFY);
			j->flags &= ~(JF_WAITING|JF_W_ASYNCNOTIFY);
			runtraps(TF_FATAL);
			j->flags |= oldf; /* not reached... */
		}
		if ((flags & JW_INTERRUPT) && (rv = trap_pending())) {
			j->flags &= ~(JF_WAITING|JF_W_ASYNCNOTIFY);
			return -rv;
		}
	}
	j->flags &= ~(JF_WAITING|JF_W_ASYNCNOTIFY);

	if (j->flags & JF_FG) {
		WAIT_T	status;

		j->flags &= ~JF_FG;
#ifdef TTY_PGRP
		if (Flag(FMONITOR) && ttypgrp_ok && j->pgrp) {
			if (tcsetpgrp(tty_fd, our_pgrp) < 0) {
				warningf(TRUE,
				"j_waitj: tcsetpgrp(%d, %d) failed: %s",
					tty_fd, (int) our_pgrp,
					strerror(errno));
			}
			if (j->state == PSTOPPED) {
				j->flags |= JF_SAVEDTTY;
				get_tty(tty_fd, &j->ttystate);
			}
		}
#endif /* TTY_PGRP */
		if (tty_fd >= 0) {
			/* Only restore tty settings if job was originally
			 * started in the foreground.  Problems can be
			 * caused by things like `more foobar &' which will
			 * typically get and save the shell's vi/emacs tty
			 * settings before setting up the tty for itself;
			 * when more exits, it restores the `original'
			 * settings, and things go down hill from there...
			 */
			if (j->state == PEXITED && j->status == 0
			    && (j->flags & JF_USETTYMODE))
			{
				get_tty(tty_fd, &tty_state);
			} else {
				set_tty(tty_fd, &tty_state,
				    (j->state == PEXITED) ? 0 : TF_MIPSKLUDGE);
				/* Don't use tty mode if job is stopped and
				 * later restarted and exits.  Consider
				 * the sequence:
				 *	vi foo (stopped)
				 *	...
				 *	stty something
				 *	...
				 *	fg (vi; ZZ)
				 * mode should be that of the stty, not what
				 * was before the vi started.
				 */
				if (j->state == PSTOPPED)
					j->flags &= ~JF_USETTYMODE;
			}
		}
#ifdef JOBS
		/* If it looks like user hit ^C to kill a job, pretend we got
		 * one too to break out of for loops, etc.  (at&t ksh does this
		 * even when not monitoring, but this doesn't make sense since
		 * a tty generated ^C goes to the whole process group)
		 */
		status = j->last_proc->status;
		if (Flag(FMONITOR) && j->state == PSIGNALLED
		    && WIFSIGNALED(status)
		    && (sigtraps[WTERMSIG(status)].flags & TF_TTY_INTR))
			trapsig(WTERMSIG(status));
#endif /* JOBS */
	}

	j_usrtime = j->usrtime;
	j_systime = j->systime;
	rv = j->status;

	if (!(flags & JW_ASYNCNOTIFY) 
	    && (!Flag(FMONITOR) || j->state != PSTOPPED))
	{
		j_print(j, JP_SHORT, shl_out);
		shf_flush(shl_out);
	}
	if (j->state != PSTOPPED
	    && (!Flag(FMONITOR) || !(flags & JW_ASYNCNOTIFY)))
		remove_job(j, where);

	return rv;
}

/* SIGCHLD handler to reap children and update job states
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static RETSIGTYPE
j_sigchld(sig)
	int	sig;
{
	int		errno_ = errno;
	Job		*j;
	Proc		UNINITIALIZED(*p);
	int		pid;
	WAIT_T		status;
	struct tms	t0, t1;

	trapsig(sig);

#ifdef JOB_SIGS
	/* Don't wait for any processes if a job is partially started.
	 * This is so we don't do away with the process group leader
	 * before all the processes in a pipe line are started (so the
	 * setpgid() won't fail)
	 */
	for (j = job_list; j; j = j->next)
		if (j->ppid == procpid && !(j->flags & JF_STARTED)) {
			held_sigchld = 1;
			return RETSIGVAL;
		}
#endif /* JOB_SIGS */

	ksh_times(&t0);
	do {
#ifdef JOB_SIGS
		pid = ksh_waitpid(-1, &status, (WNOHANG|WUNTRACED));
#else /* JOB_SIGS */
		pid = wait(&status);
#endif /* JOB_SIGS */

		if (pid <= 0)	/* return if would block (0) ... */
			break;	/* ... or no children or interrupted (-1) */

		ksh_times(&t1);

		/* find job and process structures for this pid */
		for (j = job_list; j != (Job *) 0; j = j->next)
			for (p = j->proc_list; p != (Proc *) 0; p = p->next)
				if (p->pid == pid)
					goto found;
found:
		if (j == (Job *) 0) {
			/* Can occur if process has kids, then execs shell
			warningf(TRUE, "bad process waited for (pid = %d)",
				pid);
			 */
			t0 = t1;
			continue;
		}

		j->usrtime += t1.tms_cutime - t0.tms_cutime;
		j->systime += t1.tms_cstime - t0.tms_cstime;
		t0 = t1;
		p->status = status;
#ifdef JOBS
		if (WIFSTOPPED(status))
			p->state = PSTOPPED;
		else
#endif /* JOBS */
		if (WIFSIGNALED(status))
			p->state = PSIGNALLED;
		else
			p->state = PEXITED;

		check_job(j);	/* check to see if entire job is done */
	}
#ifdef JOB_SIGS
	while (1);
#else /* JOB_SIGS */
	while (0);
#endif /* JOB_SIGS */

	errno = errno_;

	return RETSIGVAL;
}

/*
 * Called only when a process in j has exited/stopped (ie, called only
 * from j_sigchild()).  If no processes are running, the job status
 * and state are updated, asynchronous job notification is done and,
 * if unneeded, the job is removed.
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
check_job(j)
	Job	*j;
{
	int	jstate;
	Proc	*p;

	/* XXX debugging (nasty - interrupt routine using shl_out) */
	if (!(j->flags & JF_STARTED)) {
		internal_errorf(0, "check_job: job started (flags 0x%x)",
			j->flags);
		return;
	}

	jstate = PRUNNING;
	for (p=j->proc_list; p != (Proc *) 0; p = p->next) {
		if (p->state == PRUNNING)
			return;	/* some processes still running */
		if (p->state > jstate)
			jstate = p->state;
	}
	j->state = jstate;

	switch (j->last_proc->state) {
	case PEXITED:
		j->status = WEXITSTATUS(j->last_proc->status);
		break;
	case PSIGNALLED:
		j->status = 128 + WTERMSIG(j->last_proc->status);
		break;
	default:
		j->status = 0;
		break;
	}

#ifdef KSH
	/* Note when co-process dies: can't be done in j_wait() nor
	 * remove_job() since neither may be called for non-interactive 
	 * shells.
	 */
	if ((j->state == PEXITED || j->state == PSIGNALLED)
	    && coproc.job == (void *) j)
		coproc.job = (void *) 0;
#endif /* KSH */

	j->flags |= JF_CHANGED;
#ifdef JOBS
	if (Flag(FMONITOR) && !(j->flags & JF_XXCOM)) {
		/* Only put stopped jobs at the front to avoid confusing
		 * the user (don't want finished jobs effecting %+ or %-)
		 */
		if (j->state == PSTOPPED)
			put_job(j, PJ_ON_FRONT);
		if (Flag(FNOTIFY)
		    && (j->flags & (JF_WAITING|JF_W_ASYNCNOTIFY)) != JF_WAITING)
		{
			/* Look for the real file descriptor 2 */
			{
				struct env *ep;
				int fd = 2;

				for (ep = e; ep; ep = ep->oenv)
					if (ep->savefd && ep->savefd[2])
						fd = ep->savefd[2];
				shf_reopen(fd, SHF_WR, shl_j);
			}
			/* Can't call j_notify() as it removes jobs.  The job
			 * must stay in the job list as j_waitj() may be
			 * running with this job.
			 */
			j_print(j, JP_MEDIUM, shl_j);
			shf_flush(shl_j);
			if (!(j->flags & JF_WAITING) && j->state != PSTOPPED)
				remove_job(j, "notify");
		}
	}
#endif /* JOBS */
	if (!Flag(FMONITOR) && !(j->flags & (JF_WAITING|JF_FG))
	    && j->state != PSTOPPED)
	{
		if (j == async_job || (j->flags & JF_KNOWN)) {
			j->flags |= JF_ZOMBIE;
			j->job = -1;
			nzombie++;
		} else
			remove_job(j, "checkjob");
	}
}

/*
 * Print job status in either short, medium or long format.
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
j_print(j, how, shf)
	Job		*j;
	int		how;
	struct shf	*shf;
{
	Proc	*p;
	int	state;
	WAIT_T	status;
	int	coredumped;
	char	jobchar = ' ';
	char	buf[64];
	const char *filler;
	int	output = 0;

	if (how == JP_PGRP) {
		/* POSIX doesn't say what to do it there is no process
		 * group leader (ie, !FMONITOR).  We arbitrarily return
		 * last pid (which is what $! returns).
		 */
		shf_fprintf(shf, "%d\n", j->pgrp ? j->pgrp
				: (j->last_proc ? j->last_proc->pid : 0));
		return;
	}
	j->flags &= ~JF_CHANGED;
	filler = j->job > 10 ?  "\n       " : "\n      ";
	if (j == job_list)
		jobchar = '+';
	else if (j == job_list->next)
		jobchar = '-';

	for (p = j->proc_list; p != (Proc *) 0;) {
		coredumped = 0;
		switch (p->state) {
		case PRUNNING:
			strcpy(buf, "Running");
			break;
		case PSTOPPED:
			strcpy(buf, sigtraps[WSTOPSIG(p->status)].mess);
			break;
		case PEXITED:
			if (how == JP_SHORT)
				buf[0] = '\0';
			else if (WEXITSTATUS(p->status) == 0)
				strcpy(buf, "Done");
			else
				shf_snprintf(buf, sizeof(buf), "Done (%d)",
					WEXITSTATUS(p->status));
			break;
		case PSIGNALLED:
			if (WIFCORED(p->status))
				coredumped = 1;
			/* kludge for not reporting `normal termination signals'
			 * (ie, SIGINT, SIGPIPE)
			 */
			if (how == JP_SHORT && !coredumped
			    && (WTERMSIG(p->status) == SIGINT
				|| WTERMSIG(p->status) == SIGPIPE)) {
				buf[0] = '\0';
			} else
				strcpy(buf, sigtraps[WTERMSIG(p->status)].mess);
			break;
		}

		if (how != JP_SHORT)
			if (p == j->proc_list)
				shf_fprintf(shf, "[%d] %c ", j->job, jobchar);
			else
				shf_fprintf(shf, "%s", filler);

		if (how == JP_LONG)
			shf_fprintf(shf, "%5d ", p->pid);

		if (how == JP_SHORT) {
			if (buf[0]) {
				output = 1;
				shf_fprintf(shf, "%s%s ",
					buf, coredumped ? " (core dumped)" : null);
			}
		} else {
			output = 1;
			shf_fprintf(shf, "%-20s %s%s%s", buf, p->command,
				p->next ? "|" : null,
				coredumped ? " (core dumped)" : null);
		}

		state = p->state;
		status = p->status;
		p = p->next;
		while (p && p->state == state
		       && WSTATUS(p->status) == WSTATUS(status))
		{
			if (how == JP_LONG)
				shf_fprintf(shf, "%s%5d %-20s %s%s", filler, p->pid,
					space, p->command, p->next ? "|" : null);
			else if (how == JP_MEDIUM)
				shf_fprintf(shf, " %s%s", p->command,
					p->next ? "|" : null);
			p = p->next;
		}
	}
	if (output)
		shf_fprintf(shf, newline);
}

/* Convert % sequence to job
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static Job *
j_lookup(cp, ecodep)
	const char *cp;
	int	*ecodep;
{
	Job		*j, *last_match;
	Proc		*p;
	int		len, job = 0;

	if (digit(*cp)) {
		job = atoi(cp);
		/* Look for last_proc->pid (what $! returns) first... */
		for (j = job_list; j != (Job *) 0; j = j->next)
			if (j->last_proc && j->last_proc->pid == job)
				return j;
		/* ...then look for process group (this is non-POSIX),
		 * but should not break anything (so FPOSIX isn't used).
		 */
		for (j = job_list; j != (Job *) 0; j = j->next)
			if (j->pgrp && j->pgrp == job)
				return j;
		if (ecodep)
			*ecodep = JL_NOSUCH;
		return (Job *) 0;
	}
	if (*cp != '%') {
		if (ecodep)
			*ecodep = JL_INVALID;
		return (Job *) 0;
	}
	switch (*++cp) {
	  case '\0': /* non-standard */
	  case '+':
	  case '%':
		if (job_list != (Job *) 0)
			return job_list;
		break;

	  case '-':
		if (job_list != (Job *) 0 && job_list->next)
			return job_list->next;
		break;

	  case '0': case '1': case '2': case '3': case '4':
	  case '5': case '6': case '7': case '8': case '9':
		job = atoi(cp);
		for (j = job_list; j != (Job *) 0; j = j->next)
			if (j->job == job)
				return j;
		break;

	  case '?':		/* %?string */
		last_match = (Job *) 0;
		for (j = job_list; j != (Job *) 0; j = j->next)
			for (p = j->proc_list; p != (Proc *) 0; p = p->next)
				if (strstr(p->command, cp+1) != (char *) 0) {
					if (last_match) {
						if (ecodep)
							*ecodep = JL_AMBIG;
						return (Job *) 0;
					}
					last_match = j;
				}
		if (last_match)
			return last_match;
		break;

	  default:		/* %string */
		len = strlen(cp);
		last_match = (Job *) 0;
		for (j = job_list; j != (Job *) 0; j = j->next)
			if (strncmp(cp, j->proc_list->command, len) == 0) {
				if (last_match) {
					if (ecodep)
						*ecodep = JL_AMBIG;
					return (Job *) 0;
				}
				last_match = j;
			}
		if (last_match)
			return last_match;
		break;
	}
	if (ecodep)
		*ecodep = JL_NOSUCH;
	return (Job *) 0;
}

static Job	*free_jobs;
static Proc	*free_procs;

/* allocate a new job and fill in the job number.
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static Job *
new_job()
{
	int	i;
	Job	*newj, *j;

	if (free_jobs != (Job *) 0) {
		newj = free_jobs;
		free_jobs = free_jobs->next;
	} else
		newj = (Job *) alloc(sizeof(Job), APERM);

	/* brute force method */
	for (i = 1; ; i++) {
		for (j = job_list; j && j->job != i; j = j->next)
			;
		if (j == (Job *) 0)
			break;
	}
	newj->job = i;

	return newj;
}

/* Allocate new process strut
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static Proc *
new_proc()
{
	Proc	*p;

	if (free_procs != (Proc *) 0) {
		p = free_procs;
		free_procs = free_procs->next;
	} else
		p = (Proc *) alloc(sizeof(Proc), APERM);

	return p;
}

/* Take job out of job_list and put old structures into free list.
 * Keeps nzombies, last_job and async_job up to date.
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
remove_job(j, where)
	Job	*j;
	const char *where;
{
	Proc	*p, *tmp;
	Job	**prev, *curr;

	prev = &job_list;
	curr = *prev;
	for (; curr != (Job *) 0 && curr != j; prev = &curr->next, curr = *prev)
		;
	if (curr != j) {
		internal_errorf(0, "remove_job: job not found (%s)", where);
		return;
	}
	*prev = curr->next;

	/* free up proc structures */
	for (p = j->proc_list; p != (Proc *) 0; ) {
		tmp = p;
		p = p->next;
		tmp->next = free_procs;
		free_procs = tmp;
	}

	if ((j->flags & JF_ZOMBIE) && j->ppid == procpid)
		--nzombie;
	j->next = free_jobs;
	free_jobs = j;

	if (j == last_job)
		last_job = (Job *) 0;
	if (j == async_job)
		async_job = (Job *) 0;
}

/* put j in a particular location (taking it out job_list if it is there
 * already)
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
put_job(j, where)
	Job	*j;
	int	where;
{
	Job	**prev, *curr;

	/* Remove job from list (if there) */
	prev = &job_list;
	curr = job_list;
	for (; curr && curr != j; prev = &curr->next, curr = *prev)
		;
	if (curr == j)
		*prev = curr->next;

	switch (where) {
	case PJ_ON_FRONT:
		j->next = job_list;
		job_list = j;
		break;

	case PJ_PAST_STOPPED:
		prev = &job_list;
		curr = job_list;
		for (; curr && curr->state == PSTOPPED; prev = &curr->next,
							curr = *prev)
			;
		j->next = curr;
		*prev = j;
		break;
	}
}

/* nuke a job (called when unable to start full job).
 *
 * If jobs are compiled in then this routine expects sigchld to be blocked.
 */
static void
kill_job(j)
	Job	*j;
{
	Proc	*p;

	for (p = j->proc_list; p != (Proc *) 0; p = p->next)
		if (p->pid != 0)
			(void) kill(p->pid, SIGKILL);
}

/* put a more useful name on a process than snptreef does (in certain cases) */
static void
fill_command(c, len, t)
	char		*c;
	int		len;
	struct op	*t;
{
	int		alen;
	char		**ap;

	if (t->type == TEXEC || t->type == TCOM) {
		if (t->type == TCOM)
			ap = eval(t->args, DOBLANK|DONTRUNCOMMAND);
		else
			ap = t->args;
		--len; /* save room for the null */
		while (len > 0 && *ap != (char *) 0) {
			alen = strlen(*ap);
			if (alen > len)
				alen = len;
			memcpy(c, *ap, alen);
			c += alen;
			len -= alen;
			if (len > 0) {
				*c++ = ' '; len--;
			}
			ap++;
		}
		*c = '\0';
	} else
		snptreef(c, len, "%T", t);
}
