/*	$OpenBSD: main.c,v 1.21 2002/06/09 05:47:27 todd Exp $	*/

/*
 * startup, main loop, environments and error handling
 */

#define	EXTERN				/* define EXTERNs in sh.h */

#include "sh.h"
#include "ksh_stat.h"
#include "ksh_time.h"

extern char **environ;

/*
 * global data
 */

static void	reclaim ARGS((void));
static void	remove_temps ARGS((struct temp *tp));
static int	is_restricted ARGS((char *name));

/*
 * shell initialization
 */

static const char initifs[] = "IFS= \t\n";

static const char initsubs[] = "${PS2=> } ${PS3=#? } ${PS4=+ }";

static const char version_param[] =
#ifdef KSH
	"KSH_VERSION"
#else /* KSH */
	"SH_VERSION"
#endif /* KSH */
	;

static const char *const initcoms [] = {
	"typeset", "-x", "SHELL", "PATH", "HOME", NULL,
	"typeset", "-r", version_param, NULL,
	"typeset", "-i", "PPID", NULL,
	"typeset", "-i", "OPTIND=1", NULL,
#ifdef KSH
	"eval", "typeset -i RANDOM MAILCHECK=\"${MAILCHECK-600}\" SECONDS=\"${SECONDS-0}\" TMOUT=\"${TMOUT-0}\"", NULL,
#endif /* KSH */
	"alias",
	 /* Standard ksh aliases */
	  "hash=alias -t",	/* not "alias -t --": hash -r needs to work */
	  "type=whence -v",
#ifdef JOBS
	  "stop=kill -STOP",
	  "suspend=kill -STOP $$",
#endif
#ifdef KSH
	  "autoload=typeset -fu",
	  "functions=typeset -f",
# ifdef HISTORY
	  "history=fc -l",
# endif /* HISTORY */
	  "integer=typeset -i",
	  "nohup=nohup ",
	  "local=typeset",
	  "r=fc -e -",
#endif /* KSH */
#ifdef KSH
	 /* Aliases that are builtin commands in at&t */
	  "login=exec login",
#ifndef __OpenBSD__
	  "newgrp=exec newgrp",
#endif /* __OpenBSD__ */
#endif /* KSH */
	  NULL,
	/* this is what at&t ksh seems to track, with the addition of emacs */
	"alias", "-tU",
	  "cat", "cc", "chmod", "cp", "date", "ed", "emacs", "grep", "ls",
	  "mail", "make", "mv", "pr", "rm", "sed", "sh", "vi", "who",
	  NULL,
#ifdef EXTRA_INITCOMS
	EXTRA_INITCOMS, NULL,
#endif /* EXTRA_INITCOMS */
	NULL
};

int
main(argc, argv)
	int argc;
	register char **argv;
{
	register int i;
	int argi;
	Source *s;
	struct block *l;
	int restricted, errexit;
	char **wp;
	struct env env;
	pid_t ppid;

#ifdef MEM_DEBUG
	chmem_set_defaults("ct", 1);
	/* chmem_push("+c", 1); */
#endif /* MEM_DEBUG */

#ifdef OS2
	setmode (0, O_BINARY);
	setmode (1, O_TEXT);
#endif

	/* make sure argv[] is sane */
	if (!*argv) {
		static const char	*empty_argv[] = {
					    "pdksh", (char *) 0
					};

		argv = (char **) empty_argv;
		argc = 1;
	}
	kshname = *argv;

	ainit(&aperm);		/* initialize permanent Area */

	/* set up base environment */
	memset(&env, 0, sizeof(env));
	env.type = E_NONE;
	ainit(&env.area);
	e = &env;
	newblock();		/* set up global l->vars and l->funs */

	/* Do this first so output routines (eg, errorf, shellf) can work */
	initio();

	initvar();

	initctypes();

	inittraps();

#ifdef KSH
	coproc_init();
#endif /* KSH */

	/* set up variable and command dictionaries */
	tinit(&taliases, APERM, 0);
	tinit(&aliases, APERM, 0);
	tinit(&homedirs, APERM, 0);

	/* define shell keywords */
	initkeywords();

	/* define built-in commands */
	tinit(&builtins, APERM, 64); /* must be 2^n (currently 40 builtins) */
	for (i = 0; shbuiltins[i].name != NULL; i++)
		builtin(shbuiltins[i].name, shbuiltins[i].func);
	for (i = 0; kshbuiltins[i].name != NULL; i++)
		builtin(kshbuiltins[i].name, kshbuiltins[i].func);

	init_histvec();

	def_path = DEFAULT__PATH;
#if defined(HAVE_CONFSTR) && defined(_CS_PATH)
	{
		size_t len = confstr(_CS_PATH, (char *) 0, 0);
		char *new;

		if (len > 0) {
			confstr(_CS_PATH, new = alloc(len + 1, APERM), len + 1);
			def_path = new;
		}
	}
#endif /* HAVE_CONFSTR && _CS_PATH */

	/* Set PATH to def_path (will set the path global variable).
	 * (import of environment below will probably change this setting).
	 */
	{
		struct tbl *vp = global("PATH");
		/* setstr can't fail here */
		setstr(vp, def_path, KSH_RETURN_ERROR);
	}


	/* Turn on nohup by default for how - will change to off
	 * by default once people are aware of its existance
	 * (at&t ksh does not have a nohup option - it always sends
	 * the hup).
	 */
	Flag(FNOHUP) = 1;

	/* Turn on brace expansion by default.  At&t ksh's that have
	 * alternation always have it on.  BUT, posix doesn't have
	 * brace expansion, so set this before setting up FPOSIX
	 * (change_flag() clears FBRACEEXPAND when FPOSIX is set).
	 */
#ifdef BRACE_EXPAND
	Flag(FBRACEEXPAND) = 1;
#endif /* BRACE_EXPAND */

	/* set posix flag just before environment so that it will have
	 * exactly the same effect as the POSIXLY_CORRECT environment
	 * variable.  If this needs to be done sooner to ensure correct posix
	 * operation, an initial scan of the environment will also have
	 * done sooner.
	 */
#ifdef POSIXLY_CORRECT
	change_flag(FPOSIX, OF_SPECIAL, 1);
#endif /* POSIXLY_CORRECT */

	/* Check to see if we're /bin/sh. */
	if (!strcmp(&kshname[strlen(kshname) - 3], "/sh")
	    || !strcmp(kshname, "sh") || !strcmp(kshname, "-sh"))
		Flag(FSH) = 1;

	/* Set edit mode to emacs by default, may be overridden
	 * by the environment or the user.  Also, we want tab completion
	 * on in vi by default. */
#if defined(EDIT) && defined(EMACS)
	change_flag(FEMACS, OF_SPECIAL, 1);
#endif /* EDIT && EMACS */
#if defined(EDIT) && defined(VI)
	Flag(FVITABCOMPLETE) = 1;
#endif /* EDIT && VI */

	/* import environment */
	if (environ != NULL)
		for (wp = environ; *wp != NULL; wp++)
			typeset(*wp, IMPORT|EXPORT, 0, 0, 0);

	kshpid = procpid = getpid();
	typeset(initifs, 0, 0, 0, 0);	/* for security */

	/* assign default shell variable values */
	substitute(initsubs, 0);

	/* Figure out the current working directory and set $PWD */
	{
		struct stat s_pwd, s_dot;
		struct tbl *pwd_v = global("PWD");
		char *pwd = str_val(pwd_v);
		char *pwdx = pwd;

		/* Try to use existing $PWD if it is valid */
		if (!ISABSPATH(pwd)
		    || stat(pwd, &s_pwd) < 0 || stat(".", &s_dot) < 0
		    || s_pwd.st_dev != s_dot.st_dev
		    || s_pwd.st_ino != s_dot.st_ino)
			pwdx = (char *) 0;
		set_current_wd(pwdx);
		if (current_wd[0])
			simplify_path(current_wd);
		/* Only set pwd if we know where we are or if it had a
		 * bogus value
		 */
		if (current_wd[0] || pwd != null)
			/* setstr can't fail here */
			setstr(pwd_v, current_wd, KSH_RETURN_ERROR);
	}
	ppid = getppid();
	setint(global("PPID"), (long) ppid);
#ifdef KSH
	setint(global("RANDOM"), (long) (time((time_t *)0) * kshpid * ppid));
#endif /* KSH */
	/* setstr can't fail here */
	setstr(global(version_param), ksh_version, KSH_RETURN_ERROR);

	/* execute initialization statements */
	for (wp = (char**) initcoms; *wp != NULL; wp++) {
		shcomexec(wp);
		for (; *wp != NULL; wp++)
			;
	}


	ksheuid = geteuid();
	safe_prompt = ksheuid ? "$ " : "# ";
	{
		struct tbl *vp = global("PS1");

		/* Set PS1 if it isn't set, or we are root and prompt doesn't
		 * contain a #.
		 */
		if (!(vp->flag & ISSET)
		    || (!ksheuid && !strchr(str_val(vp), '#')))
			/* setstr can't fail here */
			setstr(vp, safe_prompt, KSH_RETURN_ERROR);
	}

	/* Set this before parsing arguments */
	Flag(FPRIVILEGED) = getuid() != ksheuid || getgid() != getegid();

	/* this to note if monitor is set on command line (see below) */
	Flag(FMONITOR) = 127;
	argi = parse_args(argv, OF_CMDLINE, (int *) 0);
	if (argi < 0)
		exit(1);

	if (Flag(FCOMMAND)) {
		s = pushs(SSTRING, ATEMP);
		if (!(s->start = s->str = argv[argi++]))
			errorf("-c requires an argument");
		if (argv[argi])
			kshname = argv[argi++];
	} else if (argi < argc && !Flag(FSTDIN)) {
		s = pushs(SFILE, ATEMP);
#ifdef OS2
		/* a bug in os2 extproc shell processing doesn't
		 * pass full pathnames so we have to search for it.
		 * This changes the behavior of 'ksh arg' to search
		 * the users search path but it can't be helped.
		 */
		s->file = search(argv[argi++], path, R_OK, (int *) 0);
		if (!s->file || !*s->file)
		        s->file = argv[argi - 1];
#else
		s->file = argv[argi++];
#endif /* OS2 */
		s->u.shf = shf_open(s->file, O_RDONLY, 0, SHF_MAPHI|SHF_CLEXEC);
		if (s->u.shf == NULL) {
			exstat = 127; /* POSIX */
			errorf("%s: %s", s->file, strerror(errno));
		}
		kshname = s->file;
	} else {
		Flag(FSTDIN) = 1;
		s = pushs(SSTDIN, ATEMP);
		s->file = "<stdin>";
		s->u.shf = shf_fdopen(0, SHF_RD | can_seek(0),
				      (struct shf *) 0);
		if (isatty(0) && isatty(2)) {
			Flag(FTALKING) = Flag(FTALKING_I) = 1;
			/* The following only if isatty(0) */
			s->flags |= SF_TTY;
			s->u.shf->flags |= SHF_INTERRUPT;
			s->file = (char *) 0;
		}
	}

	/* This bizarreness is mandated by POSIX */
	{
		struct stat s_stdin;

		if (fstat(0, &s_stdin) >= 0 && S_ISCHR(s_stdin.st_mode) &&
		    Flag(FTALKING))
			reset_nonblock(0);
	}

	/* initialize job control */
	i = Flag(FMONITOR) != 127;
	Flag(FMONITOR) = 0;
	j_init(i);
#ifdef EDIT
	/* Do this after j_init(), as tty_fd is not initialized 'til then */
	if (Flag(FTALKING))
		x_init();
#endif

	l = e->loc;
	l->argv = &argv[argi - 1];
	l->argc = argc - argi;
	l->argv[0] = (char *) kshname;
	getopts_reset(1);

	/* Disable during .profile/ENV reading */
	restricted = Flag(FRESTRICTED);
	Flag(FRESTRICTED) = 0;
	errexit = Flag(FERREXIT);
	Flag(FERREXIT) = 0;

	/* Do this before profile/$ENV so that if it causes problems in them,
	 * user will know why things broke.
	 */
	if (!current_wd[0] && Flag(FTALKING))
		warningf(FALSE, "Cannot determine current working directory");

	if (Flag(FLOGIN)) {
#ifdef OS2
		char *profile;

		/* Try to find a profile - first see if $INIT has a value,
		 * then try /etc/profile.ksh, then c:/usr/etc/profile.ksh.
		 */
		if (!Flag(FPRIVILEGED)
		    && strcmp(profile = substitute("$INIT/profile.ksh", 0),
			      "/profile.ksh"))
			include(profile, 0, (char **) 0, 1);
		else if (include("/etc/profile.ksh", 0, (char **) 0, 1) < 0)
			include("c:/usr/etc/profile.ksh", 0, (char **) 0, 1);
		if (!Flag(FPRIVILEGED))
			include(substitute("$HOME/profile.ksh", 0), 0,
				(char **) 0, 1);
#else /* OS2 */
		include(KSH_SYSTEM_PROFILE, 0, (char **) 0, 1);
		if (!Flag(FPRIVILEGED))
			include(substitute("$HOME/.profile", 0), 0,
				(char **) 0, 1);
#endif /* OS2 */
	}

	if (Flag(FPRIVILEGED))
		include("/etc/suid_profile", 0, (char **) 0, 1);
	else {
		char *env_file;

#ifndef KSH
		if (!Flag(FPOSIX))
			env_file = null;
		else
#endif /* !KSH */
			/* include $ENV */
			env_file = str_val(global("ENV"));

#ifdef DEFAULT_ENV
		/* If env isn't set, include default environment */
		if (env_file == null)
			env_file = DEFAULT_ENV;
#endif /* DEFAULT_ENV */
		env_file = substitute(env_file, DOTILDE);
		if (*env_file != '\0')
			include(env_file, 0, (char **) 0, 1);
#ifdef OS2
		else if (Flag(FTALKING))
			include(substitute("$HOME/kshrc.ksh", 0), 0,
				(char **) 0, 1);
#endif /* OS2 */
	}

	if (is_restricted(argv[0]) || is_restricted(str_val(global("SHELL"))))
		restricted = 1;
	if (restricted) {
		static const char *const restr_com[] = {
						"typeset", "-r", "PATH",
						    "ENV", "SHELL",
						(char *) 0
					    };
		shcomexec((char **) restr_com);
		/* After typeset command... */
		Flag(FRESTRICTED) = 1;
	}
	if (errexit)
		Flag(FERREXIT) = 1;

	if (Flag(FTALKING)) {
		hist_init(s);
#ifdef KSH
		alarm_init();
#endif /* KSH */
	} else
		Flag(FTRACKALL) = 1;	/* set after ENV */

	shell(s, TRUE);	/* doesn't return */
	return 0;
}

int
include(name, argc, argv, intr_ok)
	const char *name;
	int argc;
	char **argv;
	int intr_ok;
{
	register Source *volatile s = NULL;
	struct shf *shf;
	char **volatile old_argv;
	volatile int old_argc;
	int i;

	shf = shf_open(name, O_RDONLY, 0, SHF_MAPHI|SHF_CLEXEC);
	if (shf == NULL)
		return -1;

	if (argv) {
		old_argv = e->loc->argv;
		old_argc = e->loc->argc;
	} else {
		old_argv = (char **) 0;
		old_argc = 0;
	}
	newenv(E_INCL);
	i = ksh_sigsetjmp(e->jbuf, 0);
	if (i) {
		if (s) /* Do this before quitenv(), which frees the memory */
			shf_close(s->u.shf);
		quitenv();
		if (old_argv) {
			e->loc->argv = old_argv;
			e->loc->argc = old_argc;
		}
		switch (i) {
		  case LRETURN:
		  case LERROR:
			return exstat & 0xff; /* see below */
		  case LINTR:
			/* intr_ok is set if we are including .profile or $ENV.
			 * If user ^C's out, we don't want to kill the shell...
			 */
			if (intr_ok && (exstat - 128) != SIGTERM)
				return 1;
			/* fall through... */
		  case LEXIT:
		  case LLEAVE:
		  case LSHELL:
			unwind(i);
			/*NOREACHED*/
		  default:
			internal_errorf(1, "include: %d", i);
			/*NOREACHED*/
		}
	}
	if (argv) {
		e->loc->argv = argv;
		e->loc->argc = argc;
	}
	s = pushs(SFILE, ATEMP);
	s->u.shf = shf;
	s->file = str_save(name, ATEMP);
	i = shell(s, FALSE);
	shf_close(s->u.shf);
	quitenv();
	if (old_argv) {
		e->loc->argv = old_argv;
		e->loc->argc = old_argc;
	}
	return i & 0xff;	/* & 0xff to ensure value not -1 */
}

int
command(comm)
	const char *comm;
{
	register Source *s;

	s = pushs(SSTRING, ATEMP);
	s->start = s->str = comm;
	return shell(s, FALSE);
}

/*
 * run the commands from the input source, returning status.
 */
int
shell(s, toplevel)
	Source *volatile s;		/* input source */
	int volatile toplevel;
{
	struct op *t;
	volatile int wastty = s->flags & SF_TTY;
	volatile int attempts = 13;
	volatile int interactive = Flag(FTALKING) && toplevel;
	Source *volatile old_source = source;
	int i;

	newenv(E_PARSE);
	if (interactive)
		really_exit = 0;
	i = ksh_sigsetjmp(e->jbuf, 0);
	if (i) {
		switch (i) {
		  case LINTR: /* we get here if SIGINT not caught or ignored */
		  case LERROR:
		  case LSHELL:
			if (interactive) {
				if (i == LINTR)
					shellf(newline);
				/* Reset any eof that was read as part of a
				 * multiline command.
				 */
				if (Flag(FIGNOREEOF) && s->type == SEOF
				    && wastty)
					s->type = SSTDIN;
				/* Used by exit command to get back to
				 * top level shell.  Kind of strange since
				 * interactive is set if we are reading from
				 * a tty, but to have stopped jobs, one only
				 * needs FMONITOR set (not FTALKING/SF_TTY)...
				 */
				/* toss any input we have so far */
				s->start = s->str = null;
				break;
			}
			/* fall through... */
		  case LEXIT:
		  case LLEAVE:
		  case LRETURN:
			source = old_source;
			quitenv();
			unwind(i);	/* keep on going */
			/*NOREACHED*/
		  default:
			source = old_source;
			quitenv();
			internal_errorf(1, "shell: %d", i);
			/*NOREACHED*/
		}
	}

	while (1) {
		if (trap)
			runtraps(0);

		if (s->next == NULL) {
			if (Flag(FVERBOSE))
				s->flags |= SF_ECHO;
			else
				s->flags &= ~SF_ECHO;
		}

		if (interactive) {
			j_notify();
#ifdef KSH
			mcheck();
#endif /* KSH */
			set_prompt(PS1, s);
		}

		t = compile(s);
		if (t != NULL && t->type == TEOF) {
			if (wastty && Flag(FIGNOREEOF) && --attempts > 0) {
				shellf("Use `exit' to leave ksh\n");
				s->type = SSTDIN;
			} else if (wastty && !really_exit
				   && j_stopped_running())
			{
				really_exit = 1;
				s->type = SSTDIN;
			} else {
				/* this for POSIX, which says EXIT traps
				 * shall be taken in the environment
				 * immediately after the last command
				 * executed.
				 */
				if (toplevel)
					unwind(LEXIT);
				break;
			}
		}

		if (t && (!Flag(FNOEXEC) || (s->flags & SF_TTY)))
			exstat = execute(t, 0);

		if (t != NULL && t->type != TEOF && interactive && really_exit)
			really_exit = 0;

		reclaim();
	}
	quitenv();
	source = old_source;
	return exstat;
}

/* return to closest error handler or shell(), exit if none found */
void
unwind(i)
	int i;
{
	/* ordering for EXIT vs ERR is a bit odd (this is what at&t ksh does) */
	if (i == LEXIT || (Flag(FERREXIT) && (i == LERROR || i == LINTR)
			   && sigtraps[SIGEXIT_].trap))
	{
		runtrap(&sigtraps[SIGEXIT_]);
		i = LLEAVE;
	} else if (Flag(FERREXIT) && (i == LERROR || i == LINTR)) {
		runtrap(&sigtraps[SIGERR_]);
		i = LLEAVE;
	}
	while (1) {
		switch (e->type) {
		  case E_PARSE:
		  case E_FUNC:
		  case E_INCL:
		  case E_LOOP:
		  case E_ERRH:
			ksh_siglongjmp(e->jbuf, i);
			/*NOTREACHED*/

		  case E_NONE:
			if (i == LINTR)
				e->flags |= EF_FAKE_SIGDIE;
			/* Fall through... */

		  default:
			quitenv();
		}
	}
}

void
newenv(type)
	int type;
{
	register struct env *ep;

	ep = (struct env *) alloc(sizeof(*ep), ATEMP);
	ep->type = type;
	ep->flags = 0;
	ainit(&ep->area);
	ep->loc = e->loc;
	ep->savefd = NULL;
	ep->oenv = e;
	ep->temps = NULL;
	e = ep;
}

void
quitenv()
{
	register struct env *ep = e;
	register int fd;

	if (ep->oenv && ep->oenv->loc != ep->loc)
		popblock();
	if (ep->savefd != NULL) {
		for (fd = 0; fd < NUFILE; fd++)
			/* if ep->savefd[fd] < 0, means fd was closed */
			if (ep->savefd[fd])
				restfd(fd, ep->savefd[fd]);
		if (ep->savefd[2]) /* Clear any write errors */
			shf_reopen(2, SHF_WR, shl_out);
	}
	reclaim();

	/* Bottom of the stack.
	 * Either main shell is exiting or cleanup_parents_env() was called.
	 */
	if (ep->oenv == NULL) {
		if (ep->type == E_NONE) {	/* Main shell exiting? */
			if (Flag(FTALKING))
				hist_finish();
			j_exit();
			if (ep->flags & EF_FAKE_SIGDIE) {
				int sig = exstat - 128;

				/* ham up our death a bit (at&t ksh
				 * only seems to do this for SIGTERM)
				 * Don't do it for SIGQUIT, since we'd
				 * dump a core..
				 */
				if (sig == SIGINT || sig == SIGTERM) {
					setsig(&sigtraps[sig], SIG_DFL,
						SS_RESTORE_CURR|SS_FORCE);
					kill(0, sig);
				}
			}
#ifdef MEM_DEBUG
			chmem_allfree();
#endif /* MEM_DEBUG */
		}
		exit(exstat);
	}

	e = e->oenv;
	afree(ep, ATEMP);
}

/* Called after a fork to cleanup stuff left over from parents environment */
void
cleanup_parents_env()
{
	struct env *ep;
	int fd;

	/* Don't clean up temporary files - parent will probably need them.
	 * Also, can't easily reclaim memory since variables, etc. could be
	 * anywyere.
	 */

	/* close all file descriptors hiding in savefd */
	for (ep = e; ep; ep = ep->oenv) {
		if (ep->savefd) {
			for (fd = 0; fd < NUFILE; fd++)
				if (ep->savefd[fd] > 0)
					close(ep->savefd[fd]);
			afree(ep->savefd, &ep->area);
			ep->savefd = (short *) 0;
		}
	}
	e->oenv = (struct env *) 0;
}

/* Called just before an execve cleanup stuff temporary files */
void
cleanup_proc_env()
{
	struct env *ep;

	for (ep = e; ep; ep = ep->oenv)
		remove_temps(ep->temps);
}

/* remove temp files and free ATEMP Area */
static void
reclaim()
{
	remove_temps(e->temps);
	e->temps = NULL;
	afreeall(&e->area);
}

static void
remove_temps(tp)
	struct temp *tp;
{
#ifdef OS2
	static struct temp *delayed_remove;
	struct temp *t, **tprev;

	if (delayed_remove) {
		for (tprev = &delayed_remove, t = delayed_remove; t; t = *tprev)
			/* No need to check t->pid here... */
			if (unlink(t->name) >= 0 || errno == ENOENT) {
				*tprev = t->next;
				afree(t, APERM);
			} else
				tprev = &t->next;
	}
#endif /* OS2 */

	for (; tp != NULL; tp = tp->next)
		if (tp->pid == procpid) {
#ifdef OS2
			/* OS/2 (and dos) do not allow files that are currently
			 * open to be removed, so we cache it away for future
			 * removal.
			 * XXX should only do this if errno
			 *     is Efile-still-open-can't-remove
			 *     (but I don't know what that is...)
			 */
			if (unlink(tp->name) < 0 && errno != ENOENT) {
				t = (struct temp *) alloc(
				    sizeof(struct temp) + strlen(tp->name) + 1,
				    APERM);
				memset(t, 0, sizeof(struct temp));
				t->name = (char *) &t[1];
				strcpy(t->name, tp->name);
				t->next = delayed_remove;
				delayed_remove = t;
			}
#else /* OS2 */
			unlink(tp->name);
#endif /* OS2 */
		}
}

/* Returns true if name refers to a restricted shell */
static int
is_restricted(name)
	char *name;
{
	char *p;

	if ((p = ksh_strrchr_dirsep(name)))
		name = p;
	/* accepts rsh, rksh, rpdksh, pdrksh, etc. */
	return (p = strchr(name, 'r')) && strstr(p, "sh");
}

void
aerror(ap, msg)
	Area *ap;
	const char *msg;
{
	internal_errorf(1, "alloc: %s", msg);
	errorf(null); /* this is never executed - keeps gcc quiet */
	/*NOTREACHED*/
}
