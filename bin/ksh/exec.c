/*	$OpenBSD: exec.c,v 1.1.1.1 1996/08/14 06:19:11 downsj Exp $	*/

/*
 * execute command tree
 */

#include "sh.h"
#include "c_test.h"
#include <ctype.h>
#include "ksh_stat.h"

static int	comexec	 ARGS((struct op *t, struct tbl *volatile tp, char **ap,
			      int volatile flags));
static void	scriptexec ARGS((struct op *tp, char **ap));
static int	call_builtin ARGS((struct tbl *tp, char **wp));
static int	iosetup ARGS((struct ioword *iop));
static int	herein ARGS((char *hname, int sub));
#ifdef KSH
static char 	*do_selectargs ARGS((char **ap));
#endif /* KSH */
#ifdef KSH
static int	dbteste_isa ARGS((Test_env *te, Test_meta meta));
static const char *dbteste_getopnd ARGS((Test_env *te, Test_op op,
					 int do_eval));
static int	dbteste_eval ARGS((Test_env *te, Test_op op, const char *opnd1,
				const char *opnd2, int do_eval));
static void	dbteste_error ARGS((Test_env *te, int offset, const char *msg));
#endif /* KSH */
#ifdef OS2
static int	search_access1 ARGS((const char *path, int mode));
#endif /* OS2 */


/*
 * handle systems that don't have F_SETFD
 */
#ifndef F_SETFD
# ifndef MAXFD
#   define  MAXFD 64
# endif
/* a bit field would be smaller, but this will work */
static char clexec_tab[MAXFD+1];
#endif

/*
 * we now use this function always.
 */
int
fd_clexec(fd)
    int fd;
{
#ifndef F_SETFD
	if (fd >= 0 && fd < sizeof(clexec_tab)) {
		clexec_tab[fd] = 1;
		return 0;
	}
	return -1;
#else
	return fcntl(fd, F_SETFD, 1);
#endif
}


/*
 * execute command tree
 */
int
execute(t, flags)
	struct op * volatile t;
	volatile int flags;	/* if XEXEC don't fork */
{
	int i;
	volatile int rv = 0;
	int pv[2];
	char ** volatile ap;
	char *s, *cp;
	struct ioword **iowp;
	struct tbl *tp = NULL;

	if (t == NULL)
		return 0;

	if ((flags&XFORK) && !(flags&XEXEC) && t->type != TPIPE)
		return exchild(t, flags, -1); /* run in sub-process */

	newenv(E_EXEC);
	if (trap)
		runtraps(0);
 
	if (t->type == TCOM) {
		/* Clear subst_exstat before argument expansion.  Used by
		 * null commands (see comexec()) and by c_set().
		 */
		subst_exstat = 0;

		/* POSIX says expand command words first, then redirections,
		 * and assignments last..
		 */
		ap = eval(t->args, t->evalflags | DOBLANK | DOGLOB | DOTILDE);
		if (Flag(FXTRACE) && ap[0]) {
			shf_fprintf(shl_out, "%s",
				substitute(str_val(global("PS4")), 0));
			for (i = 0; ap[i]; i++)
				shf_fprintf(shl_out, "%s%s", ap[i],
					ap[i + 1] ? space : newline);
			shf_flush(shl_out);
		}
		if (ap[0])
			tp = findcom(ap[0], FC_BI|FC_FUNC);
	}

	if (t->ioact != NULL || t->type == TPIPE || t->type == TCOPROC) {
		e->savefd = (short *) alloc(sizeofN(short, NUFILE), ATEMP);
		/* initialize to not redirected */
		memset(e->savefd, 0, sizeofN(short, NUFILE));
	}

	/* do redirection, to be restored in quitenv() */
	if (t->ioact != NULL)
		for (iowp = t->ioact; *iowp != NULL; iowp++) {
			if (iosetup(*iowp) < 0) {
				exstat = rv = 1;
				/* Redirection failures for special commands
				 * cause (non-interactive) shell to exit.
				 */
				if (tp && tp->type == CSHELL
				    && (tp->flag & SPEC_BI))
					errorf(null);
				/* Deal with FERREXIT, quitenv(), etc. */
				goto Break;
			}
		}
	
	switch(t->type) {
	  case TCOM:
		rv = comexec(t, tp, ap, flags);
		break;

	  case TPAREN:
		rv = execute(t->left, flags|XFORK);
		break;

	  case TPIPE:
		flags |= XFORK;
		flags &= ~XEXEC;
		e->savefd[0] = savefd(0, 0);
		(void) ksh_dup2(e->savefd[0], 0, FALSE); /* stdin of first */
		e->savefd[1] = savefd(1, 0);
		while (t->type == TPIPE) {
			openpipe(pv);
			(void) ksh_dup2(pv[1], 1, FALSE); /* stdout of curr */
			/* Let exchild() close pv[0] in child
			 * (if this isn't done, commands like
			 *    (: ; cat /etc/termcap) | sleep 1
			 *  will hang forever).
			 */
			exchild(t->left, flags|XPIPEO|XCCLOSE, pv[0]);
			(void) ksh_dup2(pv[0], 0, FALSE); /* stdin of next */
			closepipe(pv);
			flags |= XPIPEI;
			t = t->right;
		}
		restfd(1, e->savefd[1]); /* stdout of last */
		e->savefd[1] = 0; /* no need to re-restore this */
		/* Let exchild() close 0 in parent, after fork, before wait */
		i = exchild(t, flags|XPCLOSE, 0);
		if (!(flags&XBGND) && !(flags&XXCOM))
			rv = i;
		break;

	  case TLIST:
		while (t->type == TLIST) {
			execute(t->left, flags & XERROK);
			t = t->right;
		}
		rv = execute(t, flags & XERROK);
		break;

#ifdef KSH
	  case TCOPROC:
	  {
		if (coproc.job && coproc.write >= 0)
			errorf("coprocess already exists");
		/* Can we re-use the existing co-process pipe? */
		cleanup_coproc(TRUE);
		/* do this before opening pipes, in case these fail */
		e->savefd[0] = savefd(0, 0);
		e->savefd[1] = savefd(1, 0);

		openpipe(pv);
		ksh_dup2(pv[0], 0, FALSE);
		close(pv[0]);
		coproc.write = pv[1];

		if (coproc.readw >= 0)
			ksh_dup2(coproc.readw, 1, FALSE);
		else {
			openpipe(pv);
			coproc.read = pv[0];
			ksh_dup2(pv[1], 1, FALSE);
			coproc.readw = pv[1];	 /* closed before first read */
		}

		/* exchild() closes coproc.* in child after fork */
		flags &= ~XEXEC;
		exchild(t->left, flags|XBGND|XFORK|XCOPROC|XCCLOSE,
			coproc.readw);
		break;
	  }
#endif /* KSH */

	  case TASYNC:
		/* XXX non-optimal, I think - "(foo &)", forks for (),
		 * forks again for async...  parent should optimize
		 * this to "foo &"...
		 */
		rv = execute(t->left, (flags&~XEXEC)|XBGND|XFORK);
		break;

	  case TOR:
	  case TAND:
		rv = execute(t->left, XERROK);
		if (t->right != NULL && (rv == 0) == (t->type == TAND))
			rv = execute(t->right, 0);
		else
			flags |= XERROK;
		break;

	  case TBANG:
		rv = !execute(t->right, XERROK);
		break;

#ifdef KSH
	  case TDBRACKET:
	    {
		Test_env te;

		te.flags = TEF_DBRACKET;
		te.pos.wp = t->args;
		te.isa = dbteste_isa;
		te.getopnd = dbteste_getopnd;
		te.eval = dbteste_eval;
		te.error = dbteste_error;

		rv = test_parse(&te);
		break;
	    }
#endif /* KSH */

	  case TFOR:
#ifdef KSH
	  case TSELECT:
#endif /* KSH */
		ap = (t->vars != NULL) ?
			  eval(t->vars, DOBLANK|DOGLOB|DOTILDE)
			: e->loc->argv + 1;
		e->type = E_LOOP;
		while (1) {
			i = ksh_sigsetjmp(e->jbuf, 0);
			if (!i)
				break;
			if ((e->flags&EF_BRKCONT_PASS)
			    || (i != LBREAK && i != LCONTIN))
			{
				quitenv();
				unwind(i);
			} else if (i == LBREAK) {
				rv = 0;
				goto Break;
			}
		}
		rv = 0; /* in case of a continue */
		if (t->type == TFOR) {
			struct tbl *vq;

			while (*ap != NULL) {
				vq = global(t->str);
				if (vq->flag & RDONLY)
					errorf("%s is read only", t->str);
				setstr(vq, *ap++);
				rv = execute(t->left, flags & XERROK);
			}
		}
#ifdef KSH
		else { /* TSELECT */
			struct tbl *vq;

			for (;;) {
				if ((cp = do_selectargs(ap)) == (char *) 0) {
					rv = 1;
					break;
				}
				vq = global(t->str);
				if (vq->flag & RDONLY)
					errorf("%s is read only", t->str);
				setstr(vq, cp);
				rv = execute(t->left, flags & XERROK);
			}
		}
#endif /* KSH */
		break;

	  case TWHILE:
	  case TUNTIL:
		e->type = E_LOOP;
		while (1) {
			i = ksh_sigsetjmp(e->jbuf, 0);
			if (!i)
				break;
			if ((e->flags&EF_BRKCONT_PASS)
			    || (i != LBREAK && i != LCONTIN))
			{
				quitenv();
				unwind(i);
			} else if (i == LBREAK) {
				rv = 0;
				goto Break;
			}
		}
		rv = 0; /* in case of a continue */
		while ((execute(t->left, XERROK) == 0) == (t->type == TWHILE))
			rv = execute(t->right, flags & XERROK);
		break;

	  case TIF:
	  case TELIF:
		if (t->right == NULL)
			break;	/* should be error */
		rv = execute(t->left, XERROK) == 0 ?
			execute(t->right->left, flags & XERROK) :
			execute(t->right->right, flags & XERROK);
		break;

	  case TCASE:
		cp = evalstr(t->str, DOTILDE);
		for (t = t->left; t != NULL && t->type == TPAT; t = t->right)
		    for (ap = t->vars; *ap; ap++)
			if ((s = evalstr(*ap, DOTILDE|DOPAT))
			    && gmatch(cp, s, FALSE))
				goto Found;
		break;
	  Found:
		rv = execute(t->left, flags & XERROK);
		break;

	  case TBRACE:
		rv = execute(t->left, flags & XERROK);
		break;

	  case TFUNCT:
		rv = define(t->str, t->left);
		break;

	  case TTIME:
		rv = timex(t, flags);
		break;

	  case TEXEC:		/* an eval'd TCOM */
		s = t->args[0];
		ap = makenv();
#ifndef F_SETFD
		for (i = 0; i < sizeof(clexec_tab); i++)
			if (clexec_tab[i]) {
				close(i);
				clexec_tab[i] = 0;
			}
#endif
		restoresigs();
		ksh_execve(t->str, t->args, ap);
		if (errno == ENOEXEC)
			scriptexec(t, ap);
		else
			errorf("%s: %s", s, strerror(errno));
	}
    Break:
	exstat = rv;

	quitenv();		/* restores IO */
	if ((flags&XEXEC))
		exit(rv);	/* exit child */
	if (rv != 0 && !(flags & XERROK)) {
		if (Flag(FERREXIT))
			unwind(LERROR);
		trapsig(SIGERR_);
	}
	return rv;
}

/*
 * execute simple command
 */

static int
comexec(t, tp, ap, flags)
	struct op *t;
	struct tbl *volatile tp;
	register char **ap;
	int volatile flags;
{
	int i;
	int rv = 0;
	register char *cp;
	register char **lastp;
	static struct op texec; /* Must be static (XXX but why?) */
	int type_flags;
	int keepasn_ok;
	int fcflags = FC_BI|FC_FUNC|FC_PATH;

	/* snag the last argument for $_ XXX not the same as at&t ksh,
	 * which only seems to set $_ after a newline (but not in
	 * functions/dot scripts, but in interactive and scipt) -
	 * perhaps save last arg here and set it in shell()?.
	 */
	if (*(lastp = ap)) {
		while (*++lastp)
			;
		setstr(typeset("_", LOCAL, 0, 0, 0), *--lastp);
	}

	/* Deal with the shell builtins builtin, exec and command since
	 * they can be followed by other commands.  This must be done before
	 * we know if we should create a local block, which must be done
	 * before we can do a path search (in case the assignments change
	 * PATH).
	 * Odd cases:
	 *   FOO=bar exec > /dev/null		FOO is kept but not exported
	 *   FOO=bar exec foobar		FOO is exported
	 *   FOO=bar command exec > /dev/null	FOO is neither kept nor exported
	 *   FOO=bar command			FOO is neither kept nor exported
	 *   PATH=... foobar			use new PATH in foobar search
	 */
	keepasn_ok = 1;
	while (tp && tp->type == CSHELL) {
		fcflags = FC_BI|FC_FUNC|FC_PATH;/* undo effects of command */
		if (tp->val.f == c_builtin) {
			if ((cp = *++ap) == NULL) {
				tp = NULL;
				break;
			}
			tp = findcom(cp, FC_BI);
			if (tp == NULL)
				errorf("builtin: %s: not a builtin", cp);
			continue;
		} else if (tp->val.f == c_exec) {
			if (ap[1] == NULL)
				break;
			ap++;
			flags |= XEXEC;
		} else if (tp->val.f == c_command) {
			int optc, saw_p = 0;

			/* Ugly dealing with options in two places (here and
			 * in c_command(), but such is life)
			 */
			ksh_getopt_reset(&builtin_opt, 0);
			while ((optc = ksh_getopt(ap, &builtin_opt, ":p"))
									== 'p')
				saw_p = 1;
			if (optc != EOF)
				break;	/* command -vV or something */
			/* don't look for functions */
			fcflags = FC_BI|FC_PATH;
			if (saw_p) {
				if (Flag(FRESTRICTED)) {
					warningf(TRUE,
						"command -p: restricted");
					rv = 1;
					goto Leave;
				}
				fcflags |= FC_DEFPATH;
			}
			ap += builtin_opt.optind;
			/* POSIX says special builtins loose their status
			 * if accessed using command.
			 */
			keepasn_ok = 0;
			if (!ap[0]) {
				/* ensure command with no args exits with 0 */
				subst_exstat = 0;
				break;
			}
		} else
			break;
		tp = findcom(ap[0], fcflags & (FC_BI|FC_FUNC));
	}
	/* todo: POSIX says assignments preceding a function are kept, at&t
	 * ksh does not do this
	 */
	if (keepasn_ok && (!ap[0] || (tp && tp->flag & KEEPASN)))
		type_flags = 0;
	else {
		/* create new variable/function block */
		newblock();
		type_flags = LOCAL|LOCAL_COPY|EXPORT;
	}
	if (Flag(FEXPORT))
		type_flags |= EXPORT;
	for (i = 0; t->vars[i]; i++) {
		cp = evalstr(t->vars[i], DOASNTILDE);
		if (Flag(FXTRACE)) {
			if (i == 0)
				shf_fprintf(shl_out, "%s",
					substitute(str_val(global("PS4")), 0));
			shf_fprintf(shl_out, "%s%s", cp,
				t->vars[i + 1] ? space : newline);
			if (!t->vars[i + 1])
				shf_flush(shl_out);
		}
		typeset(cp, type_flags, 0, 0, 0);
	}

	if ((cp = *ap) == NULL) {
		rv = subst_exstat;
		goto Leave;
	} else if (!tp) {
		if (Flag(FRESTRICTED) && ksh_strchr_dirsep(cp)) {
			warningf(TRUE, "%s: restricted", cp);
			rv = 1;
			goto Leave;
		}
		tp = findcom(cp, fcflags);
	}

	switch (tp->type) {
	  case CSHELL:			/* shell built-in */
		rv = call_builtin(tp, ap);
		break;

	  case CFUNC:			/* function call */
	  {
		volatile int old_xflag;
		volatile int old_inuse;
		const char *volatile old_kshname;

		if (!(tp->flag & ISSET)) {
			struct tbl *ftp;

			if (!tp->u.fpath) {
				/* XXX: exit code 126 vs 127 */
				warningf(TRUE,
				"%s: can't find function definition file", cp);
				rv = 127;
				break;
			}
			if (include(tp->u.fpath, 0, (char **) 0, 0) < 0) {
				warningf(TRUE,
			    "%s: can't open function definition file %s - %s",
					cp, tp->u.fpath, strerror(errno));
				rv = 126;
				break;
			}
			if (!(ftp = findfunc(cp, hash(cp), FALSE))
			    || !(ftp->flag & ISSET))
			{
				warningf(TRUE,
					"%s: function not defined by %s",
					cp, tp->u.fpath);
				rv = 127;
				break;
			}
			tp = ftp;
		}

		/* posix says $0 remains unchanged, at&t ksh changes it */
		old_kshname = kshname;
		if (!Flag(FPOSIX))
			kshname = ap[0];
		e->loc->argv = ap;
		for (i = 0; *ap++ != NULL; i++)
			;
		e->loc->argc = i - 1;
		getopts_reset(1);

		old_xflag = Flag(FXTRACE);
		Flag(FXTRACE) = tp->flag & TRACE ? TRUE : FALSE;

		old_inuse = tp->flag & FINUSE;
		tp->flag |= FINUSE;

		e->type = E_FUNC;
		i = ksh_sigsetjmp(e->jbuf, 0);
		if (i == 0) {
			/* seems odd to pass XERROK here, but at&t ksh does */
			exstat = execute(tp->val.t, flags & XERROK);
			i = LRETURN;
		}
		kshname = old_kshname;
		Flag(FXTRACE) = old_xflag;
		tp->flag = (tp->flag & ~FINUSE) | old_inuse;
		/* Were we deleted while executing?  If so, free the execution
		 * tree.  Unfortunately, the table entry is never re-used.
		 */
		if ((tp->flag & (FDELETE|FINUSE)) == FDELETE) {
			if (tp->flag & ALLOC) {
				tp->flag &= ~ALLOC;
				tfree(tp->val.t, tp->areap);
			}
			tp->flag = 0;
		}
		switch (i) {
		  case LRETURN:
		  case LERROR:
			rv = exstat;
			break;
		  case LINTR:
		  case LEXIT:
		  case LLEAVE:
		  case LSHELL:
			quitenv();
			unwind(i);
			/*NOTREACHED*/
		  default:
			quitenv();
			internal_errorf(1, "CFUNC %d", i);
		}
		break;
	  }

	  case CEXEC:		/* executable command */
	  case CTALIAS:		/* tracked alias */
		if (!(tp->flag&ISSET)) {
			/*
			 * mlj addition:
			 *
			 * If you specify a full path to a file
			 * (or type the name of a file in .) which
			 * doesn't have execute priv's, it used to
			 * just say "not found".  Kind of annoying,
			 * particularly if you just wrote a script
			 * but forgot to say chmod 755 script.
			 *
			 * This should probably be done in eaccess(),
			 * but it works here (at least I haven't noticed
			 * changing errno here breaking something
			 * else).
			 *
			 * So, we assume that if the file exists, it
			 * doesn't have execute privs; else, it really
			 * is not found.
			 */
			if (access(cp, F_OK) < 0)
				warningf(TRUE, "%s: not found", cp);
			else
				warningf(TRUE, "%s: cannot execute", cp);
			/* XXX posix says 126 if in path and cannot execute */
			rv = 127;
			break;
		}

		/* set $_ to program's full path */
		setstr(typeset("_", LOCAL|EXPORT, 0, 0, 0), tp->val.s);

		if ((flags&XEXEC)) {
			j_exit();
			if (!(flags&XBGND) || Flag(FMONITOR)) {
				setexecsig(&sigtraps[SIGINT], SS_RESTORE_ORIG);
				setexecsig(&sigtraps[SIGQUIT], SS_RESTORE_ORIG);
			}
		}

		/* to fork we set up a TEXEC node and call execute */
		texec.type = TEXEC;
		texec.left = t;	/* for tprint */
		texec.str = tp->val.s;
		texec.args = ap;
		rv = exchild(&texec, flags, -1);
		break;
	}
  Leave:
	if (flags & XEXEC) {
		exstat = rv;
		unwind(LLEAVE);
	}
	return rv;
}

static void
scriptexec(tp, ap)
	register struct op *tp;
	register char **ap;
{
	char *shell;

	shell = str_val(global(EXECSHELL_STR));
	if (shell && *shell)
		shell = search(shell, path, X_OK);
	if (!shell || !*shell)
		shell = EXECSHELL;

	*tp->args-- = tp->str;
#ifdef	SHARPBANG
	{
		char buf[LINE];
		register char *cp;
		register int fd, n;

		buf[0] = '\0';
		if ((fd = open(tp->str, O_RDONLY)) >= 0) {
			if ((n = read(fd, buf, LINE - 1)) > 0)
				buf[n] = '\0';
			(void) close(fd);
		}
		if ((buf[0] == '#' && buf[1] == '!' && (cp = &buf[2]))
# ifdef OS2
		    || (strncmp(buf, "extproc", 7) == 0 && isspace(buf[7])
			&& (cp = &buf[7]))
# endif /* OS2 */
		    )
		{
			while (*cp && (*cp == ' ' || *cp == '\t'))
				cp++;
			if (*cp && *cp != '\n') {
				char *a0 = cp, *a1 = (char *) 0;
# ifdef OS2
				char *a2 = cp;
# endif /* OS2 */

				while (*cp && *cp != '\n' && *cp != ' '
				       && *cp != '\t')
				{
# ifdef OS2
			/* Allow shell search without prepended path
			 * if shell with / in pathname cannot be found.
			 * Use / explicitly so \ can be used if explicit
			 * needs to be forced.
			 */
					if (*cp == '/')
						a2 = cp + 1;
# endif /* OS2 */
					cp++;
				}
				if (*cp && *cp != '\n') {
					*cp++ = '\0';
					while (*cp
					       && (*cp == ' ' || *cp == '\t'))
						cp++;
					if (*cp && *cp != '\n') {
						a1 = cp;
						/* all one argument */
						while (*cp && *cp != '\n')
							cp++;
					}
				}
				if (*cp == '\n') {
					*cp = '\0';
					if (a1)
						*tp->args-- = a1;
# ifdef OS2
					if (a0 != a2 && search_access(a0, X_OK))
						a0 = a2;
# endif /* OS2 */
					shell = a0;
				}
			}
# ifdef OS2
		} else {
		        /* Use ksh documented shell default if present
			 * else use OS2_SHELL which is assumed to need
			 * the /c option and '\' as dir separater.
			 */
		         char *p = shell;

			 shell = str_val(global("EXECSHELL"));
			 if (shell && *shell)
				 shell = search(shell, path, X_OK);
			 if (!shell || !*shell) {
				 shell = p;
				 *tp->args-- = "/c";
				 for (p = tp->str; *p; p++)
					 if (*p == '/')
						 *p = '\\';
			 }
# endif /* OS2 */
		}
	}
#endif	/* SHARPBANG */
	*tp->args = shell;

	ksh_execve(tp->args[0], tp->args, ap);

	/* report both the program that was run and the bogus shell */
	errorf("%s: %s: %s", tp->str, shell, strerror(errno));
}

int
shcomexec(wp)
	register char **wp;
{
	register struct tbl *tp;

	tp = tsearch(&builtins, *wp, hash(*wp));
	if (tp == NULL)
		internal_errorf(1, "shcomexec: %s", *wp);
	return call_builtin(tp, wp);
}

/*
 * Search function tables for a function.  If create set, a table entry
 * is created if none is found.
 */
struct tbl *
findfunc(name, h, create)
	const char *name;
	unsigned int	h;
	int	create;
{
	struct block *l;
	struct tbl *tp = (struct tbl *) 0;

	for (l = e->loc; l; l = l->next) {
		tp = tsearch(&l->funs, name, h);
		if (tp && (tp->flag & DEFINED))
			break;
		if (!l->next && create) {
			tp = tenter(&l->funs, name, h);
			tp->flag = DEFINED;
			tp->type = CFUNC;
			tp->val.t = (struct op *) 0;
			break;
		}
	}
	return tp;
}

/*
 * define function
 */
int
define(name, t)
	const char *name;
	struct op *t;
{
	register struct tbl *tp;

	tp = findfunc(name, hash(name), TRUE);

	/* If this function is currently being executed, we zap this
	 * table entry so findfunc() won't see it
	 */
	if (tp->flag & FINUSE) {
		tp->name[0] = '\0';
		tp->flag &= ~DEFINED; /* ensure it won't be found */
		tp->flag |= FDELETE;
		return define(name, t);
	}

	if (tp->flag & ALLOC) {
		tp->flag &= ~(ISSET|ALLOC);
		tfree(tp->val.t, tp->areap);
	}

	if (t == NULL) {		/* undefine */
		tdelete(tp);
		return 0;
	}

	tp->val.t = tcopy(t, tp->areap);
	tp->flag |= (ISSET|ALLOC);

	return 0;
}

/*
 * add builtin
 */
void
builtin(name, func)
	const char *name;
	int (*func) ARGS((char **));
{
	register struct tbl *tp;
	int flag;

	/* see if any flags should be set for this builtin */
	for (flag = 0; ; name++) {
		if (*name == '=')	/* command does variable assignment */
			flag |= KEEPASN;
		else if (*name == '*')	/* POSIX special builtin */
			flag |= SPEC_BI;
		else if (*name == '+')	/* POSIX regular builtin */
			flag |= REG_BI;
		else
			break;
	}

	tp = tenter(&builtins, name, hash(name));
	tp->flag = DEFINED | flag;
	tp->type = CSHELL;
	tp->val.f = func;
}

/*
 * find command
 * either function, hashed command, or built-in (in that order)
 */
struct tbl *
findcom(name, flags)
	const char *name;
	int	flags;		/* FC_* */
{
	static struct tbl temp;
	unsigned int h = hash(name);
	struct tbl *tp = NULL, *tbi;
	int insert = Flag(FTRACKALL);	/* insert if not found */
	char *fpath;			/* for function autoloading */
	char *npath;

	if (ksh_strchr_dirsep(name) != NULL) {
		insert = 0;
		/* prevent FPATH search below */
		flags &= ~FC_FUNC;
		goto Search;
	}
	tbi = (flags & FC_BI) ? tsearch(&builtins, name, h) : NULL;
	/* POSIX says special builtins first, then functions, then
	 * POSIX regular builtins, then search path...
	 */
	if ((flags & FC_SPECBI) && tbi && (tbi->flag & SPEC_BI))
		tp = tbi;
	if (!tp && (flags & FC_FUNC)) {
		tp = findfunc(name, h, FALSE);
		if (tp && !(tp->flag & ISSET)) {
			if ((fpath = str_val(global("FPATH"))) == null)
				tp->u.fpath = (char *) 0;
			else
				tp->u.fpath = search(name, fpath, R_OK);
		}
	}
	if (!tp && (flags & FC_REGBI) && tbi && (tbi->flag & REG_BI))
		tp = tbi;
	/* todo: posix says non-special/non-regular builtins must
	 * be triggered by some user-controllable means like a
	 * special directory in PATH.  Requires modifications to
	 * the search() function.  Tracked aliases should be
	 * modified to allow tracking of builtin commands.
	 * This should be under control of the FPOSIX flag.
	 * If this is changed, also change c_whence...
	 */
	if (!tp && (flags & FC_UNREGBI) && tbi)
		tp = tbi;
	if (!tp && (flags & FC_PATH) && !(flags & FC_DEFPATH)) {
		tp = tsearch(&taliases, name, h);
		if (tp && (tp->flag & ISSET) && eaccess(tp->val.s, X_OK) != 0) {
			if (tp->flag & ALLOC) {
				tp->flag &= ~ALLOC;
				afree(tp->val.s, APERM);
			}
			tp->flag &= ~ISSET;
		}
	}

  Search:
	if ((!tp || (tp->type == CTALIAS && !(tp->flag&ISSET)))
	    && (flags & FC_PATH))
	{
		if (!tp) {
			if (insert && !(flags & FC_DEFPATH)) {
				tp = tenter(&taliases, name, h);
				tp->type = CTALIAS;
			} else {
				tp = &temp;
				tp->type = CEXEC;
			}
			tp->flag = DEFINED;	/* make ~ISSET */
		}
		npath = search(name, flags & FC_DEFPATH ? def_path : path,
				X_OK);
		if (npath) {
			tp->val.s = tp == &temp ? npath : str_save(npath, APERM);
			tp->flag |= ISSET|ALLOC;
		} else if ((flags & FC_FUNC)
			   && (fpath = str_val(global("FPATH"))) != null
			   && (npath = search(name, fpath, R_OK)) != (char *) 0)
		{
			/* An undocumented feature of at&t ksh is that it
			 * searches FPATH if a command is not found, even
			 * if the command hasn't been set up as an autoloaded
			 * function (ie, no typeset -uf).
			 */
			tp = &temp;
			tp->type = CFUNC;
			tp->flag = DEFINED; /* make ~ISSET */
			tp->u.fpath = npath;
		}
	}
	return tp;
}

/*
 * flush executable commands with relative paths
 */
void
flushcom(all)
	int all;		/* just relative or all */
{
	struct tbl *tp;
	struct tstate ts;

	for (twalk(&ts, &taliases); (tp = tnext(&ts)) != NULL; )
		if ((tp->flag&ISSET) && (all || !ISDIRSEP(tp->val.s[0]))) {
			if (tp->flag&ALLOC) {
				tp->flag &= ~(ALLOC|ISSET);
				afree(tp->val.s, APERM);
			}
			tp->flag = ~ISSET;
		}
}

/* Check if path is something we want to find.  Returns -1 for failure. */
int
search_access(path, mode)
	const char *path;
	int mode;
{
#ifndef OS2
	int ret = eaccess(path, mode);
	struct stat statb;

	/* if executable pipes come along, this will have to change */
	if (ret == 0 && (mode == X_OK)
	    && (stat(path, &statb) < 0 || !S_ISREG(statb.st_mode)
	       /* This 'cause access() says root can execute everything */
	       || !(statb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))))
		ret = -1;
	return ret;
#else /* !OS2 */
	/*
	 * NOTE: ASSUMES path can be modified and has enough room at the
	 *       end of the string for a suffix (ie, 4 extra characters).
	 *	 Certain code knows this (eg, eval.c(globit()),
	 *	 exec.c(search())).
	 */
	static char *xsuffixes[] = { ".ksh", ".exe", ".", ".sh", ".cmd",
				     ".com", ".bat", (char *) 0
				   };
	static char *rsuffixes[] = { ".ksh", ".", ".sh", ".cmd", ".bat",
				      (char *) 0
				   };
	int i;
	char *mpath = (char *) path;
	char *tp = mpath + strlen(mpath);
	char *p;
	char **sfx;
 
	/* If a suffix has been specified, check if it is one of the
	 * suffixes that indicate the file is executable - if so, change
	 * the access test to R_OK...
	 * This code assumes OS/2 files can have only one suffix...
	 */
	if ((p = strrchr((p = ksh_strrchr_dirsep(mpath)) ? p : mpath, '.'))) {
		if (mode == X_OK) 
			mode = R_OK;
		return search_access1(mpath, mode);
	}
	/* Try appending the various suffixes.  Different suffixes for
	 * read and execute 'cause we don't want to read an executable...
	 */
	sfx = mode == R_OK ? rsuffixes : xsuffixes;
	for (i = 0; sfx[i]; i++) {
		strcpy(tp, p = sfx[i]);
		if (search_access1(mpath, R_OK) == 0)
			return 0;
		*tp = '\0';
	}
	return -1;
#endif /* !OS2 */
}

#ifdef OS2
static int
search_access1(path, mode)
	const char *path;
	int mode;
{
	int ret = eaccess(path, mode);
	struct stat statb;

	/* if executable pipes come along, this will have to change */
	if (ret == 0 && (mode == X_OK || mode == R_OK)
	    && (stat(path, &statb) < 0 || !S_ISREG(statb.st_mode)))
		ret = -1;
	return ret;
}
#endif /* OS2 */

/*
 * search for command with PATH
 */
char *
search(name, path, mode)
	const char *name;
	const char *path;
	int mode;		/* R_OK or X_OK */
{
	const char *sp, *p;
	char *xp;
	XString xs;
	int namelen;

#ifdef OS2
	/* Xinit() allocates 8 additional bytes, so appended suffixes won't
	 * overflow the memory.
	 */
	namelen = strlen(name) + 1;
	Xinit(xs, xp, namelen, ATEMP);
	memcpy(Xstring(xs, xp), name, namelen);

 	if (ksh_strchr_dirsep(name)) {
		if (search_access(Xstring(xs, xp), mode) >= 0)
			return Xstring(xs, xp); /* not Xclose() - see above */
		Xfree(xs, xp);
		return NULL;
	}

	/* Look in current context always. (os2 style) */
	if (search_access(Xstring(xs, xp), mode) == 0) 
		return Xstring(xs, xp); /* not Xclose() - xp may be wrong */
#else /* OS2 */
	if (ksh_strchr_dirsep(name)) {
		if (search_access(name, mode) == 0)
			return (char *) name;
		return NULL;
	}

	namelen = strlen(name) + 1;
	Xinit(xs, xp, 128, ATEMP);
#endif /* OS2 */

	sp = path;
	while (sp != NULL) {
		xp = Xstring(xs, xp);
		if (!(p = strchr(sp, PATHSEP)))
			p = sp + strlen(sp);
		if (p != sp) {
			XcheckN(xs, xp, p - sp);
			memcpy(xp, sp, p - sp);
			xp += p - sp;
			*xp++ = DIRSEP;
		}
		sp = p;
		XcheckN(xs, xp, namelen);
		memcpy(xp, name, namelen);
 		if (search_access(Xstring(xs, xp), mode) == 0)
#ifdef OS2
 			return Xstring(xs, xp); /* Not Xclose() - see above */
#else /* OS2 */
			return Xclose(xs, xp + namelen);
#endif /* OS2 */
		if (*sp++ == '\0')
			sp = NULL;
	}
	Xfree(xs, xp);
	return NULL;
}

static int
call_builtin(tp, wp)
	struct tbl *tp;
	char **wp;
{
	int rv;

	builtin_argv0 = wp[0];
	builtin_flag = tp->flag;
	shf_reopen(1, SHF_WR, shl_stdout);
	shl_stdout_ok = 1;
	ksh_getopt_reset(&builtin_opt, GF_ERROR);
	rv = (*tp->val.f)(wp);
	shf_flush(shl_stdout);
	shl_stdout_ok = 0;
	builtin_flag = 0;
	builtin_argv0 = (char *) 0;
	return rv;
}

/*
 * set up redirection, saving old fd's in e->savefd
 */
static int
iosetup(iop)
	register struct ioword *iop;
{
	register int u = -1;
	char *cp = iop->name;
	int iotype = iop->flag & IOTYPE;
	int do_open = 1, do_close = 0, UNINITIALIZED(flags);
	struct ioword iotmp;
	struct stat statb;

	if (iotype != IOHERE)
		cp = evalonestr(cp, DOTILDE|(Flag(FTALKING) ? DOGLOB : 0));

	/* Used for tracing and error messages to print expanded cp */
	iotmp = *iop;
	iotmp.name = (iotype == IOHERE) ? (char *) 0 : cp;
	iotmp.flag |= IONAMEXP;

	if (Flag(FXTRACE))
		shellf("%s%s\n",
			substitute(str_val(global("PS4")), 0),
			snptreef((char *) 0, 32, "%R", &iotmp));

	switch (iotype) {
	  case IOREAD:
		flags = O_RDONLY;
		break;

	  case IOCAT:
		flags = O_WRONLY | O_APPEND | O_CREAT;
		break;

	  case IOWRITE:
		flags = O_WRONLY | O_CREAT | O_TRUNC;
		if (Flag(FNOCLOBBER) && !(iop->flag & IOCLOB)
		    && (stat(cp, &statb) < 0 || S_ISREG(statb.st_mode)))
			flags |= O_EXCL;
		break;

	  case IORDWR:
		flags = O_RDWR | O_CREAT;
		break;

	  case IOHERE:
		do_open = 0;
		/* herein() returns -2 if error has been printed */
		u = herein(cp, iop->flag & IOEVAL);
		/* cp may have wrong name */
		break;

	  case IODUP:
	  {
		const char *emsg;

		do_open = 0;
		if (*cp == '-' && !cp[1]) {
			u = 1009;	 /* prevent error return below */
			do_close = 1;
		} else if ((u = check_fd(cp,
				X_OK | ((iop->flag & IORDUP) ? R_OK : W_OK),
				&emsg)) < 0)
		{
			warningf(TRUE, "%s: %s",
				snptreef((char *) 0, 32, "%R", &iotmp), emsg);
			return -1;
		}
		break;
	  }
	}
	if (do_open) {
		if (Flag(FRESTRICTED) && (flags & O_CREAT)) {
			warningf(TRUE, "%s: restricted", cp);
			return -1;
		}
		u = open(cp, flags, 0666);
#ifdef OS2
		if (u < 0 && strcmp(cp, "/dev/null") == 0)
			u = open("nul", flags, 0666);
#endif /* OS2 */
	}
	if (u < 0) {
		/* herein() may already have printed message */
		if (u == -1)
			warningf(TRUE, "cannot %s %s: %s",
			       iotype == IODUP ? "dup"
				: (iotype == IOREAD || iotype == IOHERE) ?
				    "open" : "create", cp, strerror(errno));
		return -1;
	}
	/* Do not save if it has already been redirected (i.e. "cat >x >y"). */
	if (e->savefd[iop->unit] == 0)
		/* c_exec() assumes e->savefd[fd] set for any redirections.
		 * Ask savefd() not to close iop->unit - allows error messages
		 * to be seen if iop->unit is 2; also means we can't lose
		 * the fd (eg, both dup2 below and dup2 in restfd() failing).
		 */
		e->savefd[iop->unit] = savefd(iop->unit, 1);

	if (do_close)
		close(iop->unit);
	else if (u != iop->unit) {
		if (ksh_dup2(u, iop->unit, TRUE) < 0) {
			warningf(TRUE,
				"could not finish (dup) redirection %s: %s",
				snptreef((char *) 0, 32, "%R", &iotmp),
				strerror(errno));
			if (iotype != IODUP)
				close(u);
			return -1;
		}
		if (iotype == IODUP) {
#ifdef KSH
			if (iop->flag & IORDUP)	/* possible <&p */
				/* Ensure other side of read pipe is
				 * closed so EOF can be read properly
				 */
				coproc_readw_close(u);
			else			/* possible >&p */
				/* If co-process input is duped,
				 * close shell's copy
				 */
				coproc_write_close(u);
#else /* KSH */
			;
#endif /* KSH */
		} else
			close(u);
	}
	if (u == 2) /* Clear any write errors */
		shf_reopen(2, SHF_WR, shl_out);
	return 0;
}

/*
 * open here document temp file.
 * if unquoted here, expand here temp file into second temp file.
 */
static int
herein(hname, sub)
	char *hname;
	int sub;
{
	int fd;

	/* ksh -c 'cat << EOF' can cause this... */
	if (hname == (char *) 0) {
		warningf(TRUE, "here document missing");
		return -2; /* special to iosetup(): don't print error */
	}
	if (sub) {
		char *cp;
		struct source *s, *volatile osource = source;
		struct temp *h;
		struct shf *volatile shf;
		int i;

		/* must be before newenv() 'cause shf uses ATEMP */
		shf = shf_open(hname, O_RDONLY, 0, SHF_MAPHI|SHF_CLEXEC);
		if (shf == NULL)
			return -1;
		newenv(E_ERRH);
		i = ksh_sigsetjmp(e->jbuf, 0);
		if (i) {
			if (shf)
				shf_close(shf);
			source = osource;
			quitenv(); /* after shf_close() due to alloc */
			return -2; /* special to iosetup(): don't print error */
		}
		/* set up yylex input from here file */
		s = pushs(SFILE, ATEMP);
		s->u.shf = shf;
		source = s;
		if (yylex(ONEWORD) != LWORD)
			internal_errorf(1, "herein: yylex");
		shf_close(shf);
		shf = (struct shf *) 0;
		cp = evalstr(yylval.cp, 0);

		/* write expanded input to another temp file */
		h = maketemp(ATEMP);
		h->next = e->temps; e->temps = h;
		if (!(shf = h->shf) || (fd = open(h->name, O_RDONLY, 0)) < 0)
			/* shf closeed by error handler */
			errorf("%s: %s", h->name, strerror(errno));
		shf_puts(cp, shf);
		if (shf_close(shf) == EOF) {
			close(fd);
			shf = (struct shf *) 0;
			errorf("error writing %s: %s", h->name,
				strerror(errno));
		}
		shf = (struct shf *) 0;

		quitenv();
	} else {
		fd = open(hname, O_RDONLY, 0);
		if (fd < 0)
			return -1;
	}

	return fd;
}

#ifdef KSH
/*
 *	ksh special - the select command processing section
 *	print the args in column form - assuming that we can
 */
static char *
do_selectargs(ap)
	register char **ap;
{
	static const char *const read_args[] = {
					"read", "-r", "REPLY", (char *) 0
				    };
	char *s;
	int i, UNINITIALIZED(argct);

	while (1) {
		argct = pr_menu(ap);
		shellf("%s", str_val(global("PS3")));
		if (call_builtin(findcom("read", FC_BI), (char **) read_args))
			return (char *) 0;
		s = str_val(global("REPLY"));
		while (*s && isspace(*s))
			s++;
		if (*s) {
			i = atoi(s);
			return (i >= 1 && i <= argct) ? ap[i - 1] : null;
		}
	}
}

struct select_menu_info {
	char	*const *args;
	int	arg_width;
	int	num_width;
} info;

static char *select_fmt_entry ARGS((void *arg, int i, char *buf, int buflen));

/* format a single select menu item */
static char *
select_fmt_entry(arg, i, buf, buflen)
	void *arg;
	int i;
	char *buf;
	int buflen;
{
	struct select_menu_info *smi = (struct select_menu_info *) arg;

	shf_snprintf(buf, buflen, "%*d) %s",
		smi->num_width, i + 1, smi->args[i]);
	return buf;
}

/*
 *	print a select style menu
 */
int
pr_menu(ap)
	char *const *ap;
{
	struct select_menu_info smi;
	char *const *pp;
	int nwidth, dwidth;
	int i, n;

	/* Width/column calculations were done once and saved, but this
	 * means select can't be used recursively so we re-calculate each
	 * time (could save in a structure that is returned, but its probably
	 * not worth the bother).
	 */

	/*
	 * get dimensions of the list
	 */
	for (n = 0, nwidth = 0, pp = ap; *pp; n++, pp++) {
		i = strlen(*pp);
		nwidth = (i > nwidth) ? i : nwidth;
	}
	/*
	 * we will print an index of the form
	 *	%d)
	 * in front of each entry
	 * get the max width of this
	 */
	for (i = n, dwidth = 1; i >= 10; i /= 10)
		dwidth++;

	smi.args = ap;
	smi.arg_width = nwidth;
	smi.num_width = dwidth;
	print_columns(shl_out, n, select_fmt_entry, (void *) &smi,
		dwidth + nwidth + 2);

	return n;
}
#endif /* KSH */
#ifdef KSH

/*
 *	[[ ... ]] evaluation routines
 */

extern const char *const dbtest_tokens[];
extern const char db_close[];

/* Test if the current token is a whatever.  Accepts the current token if
 * it is.  Returns 0 if it is not, non-zero if it is (in the case of
 * TM_UNOP and TM_BINOP, the returned value is a Test_op).
 */
static int
dbteste_isa(te, meta)
	Test_env *te;
	Test_meta meta;
{
	int ret = 0;
	int uqword;
	char *p;

	if (!*te->pos.wp)
		return meta == TM_END;

	/* unquoted word? */
	for (p = *te->pos.wp; *p == CHAR; p += 2)
		;
	uqword = *p == EOS;

	if (meta == TM_UNOP || meta == TM_BINOP) {
		if (uqword) {
			char buf[8];	/* longer than the longest operator */
			char *q = buf;
			for (p = *te->pos.wp; *p == CHAR
					      && q < &buf[sizeof(buf) - 1];
					      p += 2)
				*q++ = p[1];
			*q = '\0';
			ret = (int) test_isop(te, meta, buf);
		}
	} else if (meta == TM_END)
		ret = 0;
	else
		ret = uqword
			&& strcmp(*te->pos.wp, dbtest_tokens[(int) meta]) == 0;

	/* Accept the token? */
	if (ret)
		te->pos.wp++;

	return ret;
}

static const char *
dbteste_getopnd(te, op, do_eval)
	Test_env *te;
	Test_op op;
	int do_eval;
{
	char *s = *te->pos.wp;

	if (!s)
		return (char *) 0;

	te->pos.wp++;

	if (!do_eval)
		return null;

	if (op == TO_STEQL || op == TO_STNEQ)
		s = evalstr(s, DOTILDE | DOPAT);
	else
		s = evalstr(s, DOTILDE);

	return s;
}

static int
dbteste_eval(te, op, opnd1, opnd2, do_eval)
	Test_env *te;
	Test_op op;
	const char *opnd1;
	const char *opnd2;
	int do_eval;
{
	return test_eval(te, op, opnd1, opnd2, do_eval);
}

static void
dbteste_error(te, offset, msg)
	Test_env *te;
	int offset;
	const char *msg;
{
	te->flags |= TEF_ERROR;
	internal_errorf(0, "dbteste_error: %s (offset %d)", msg, offset);
}
#endif /* KSH */
