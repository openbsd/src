/*	$OpenBSD: c_sh.c,v 1.21 2004/12/18 22:35:41 millert Exp $	*/

/*
 * built-in Bourne commands
 */

#include "sh.h"
#include <sys/stat.h>	/* umask() */
#include <sys/times.h>

static	char *clocktos(clock_t t);


/* :, false and true */
int
c_label(wp)
	char **wp;
{
	return wp[0][0] == 'f' ? 1 : 0;
}

int
c_shift(wp)
	char **wp;
{
	struct block *l = e->loc;
	int n;
	long val;
	char *arg;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	arg = wp[builtin_opt.optind];

	if (arg) {
		evaluate(arg, &val, KSH_UNWIND_ERROR);
		n = val;
	} else
		n = 1;
	if (n < 0) {
		bi_errorf("%s: bad number", arg);
		return (1);
	}
	if (l->argc < n) {
		bi_errorf("nothing to shift");
		return (1);
	}
	l->argv[n] = l->argv[0];
	l->argv += n;
	l->argc -= n;
	return 0;
}

int
c_umask(wp)
	char **wp;
{
	int i;
	char *cp;
	int symbolic = 0;
	int old_umask;
	int optc;

	while ((optc = ksh_getopt(wp, &builtin_opt, "S")) != EOF)
		switch (optc) {
		  case 'S':
			symbolic = 1;
			break;
		  case '?':
			return 1;
		}
	cp = wp[builtin_opt.optind];
	if (cp == NULL) {
		old_umask = umask(0);
		umask(old_umask);
		if (symbolic) {
			char buf[18];
			int j;

			old_umask = ~old_umask;
			cp = buf;
			for (i = 0; i < 3; i++) {
				*cp++ = "ugo"[i];
				*cp++ = '=';
				for (j = 0; j < 3; j++)
					if (old_umask & (1 << (8 - (3*i + j))))
						*cp++ = "rwx"[j];
				*cp++ = ',';
			}
			cp[-1] = '\0';
			shprintf("%s\n", buf);
		} else
			shprintf("%#3.3o\n", old_umask);
	} else {
		int new_umask;

		if (digit(*cp)) {
			for (new_umask = 0; *cp >= '0' && *cp <= '7'; cp++)
				new_umask = new_umask * 8 + (*cp - '0');
			if (*cp) {
				bi_errorf("bad number");
				return 1;
			}
		} else {
			/* symbolic format */
			int positions, new_val;
			char op;

			old_umask = umask(0);
			umask(old_umask); /* in case of error */
			old_umask = ~old_umask;
			new_umask = old_umask;
			positions = 0;
			while (*cp) {
				while (*cp && strchr("augo", *cp))
					switch (*cp++) {
					case 'a': positions |= 0111; break;
					case 'u': positions |= 0100; break;
					case 'g': positions |= 0010; break;
					case 'o': positions |= 0001; break;
					}
				if (!positions)
					positions = 0111; /* default is a */
				if (!strchr("=+-", op = *cp))
					break;
				cp++;
				new_val = 0;
				while (*cp && strchr("rwxugoXs", *cp))
					switch (*cp++) {
					case 'r': new_val |= 04; break;
					case 'w': new_val |= 02; break;
					case 'x': new_val |= 01; break;
					case 'u': new_val |= old_umask >> 6;
						  break;
					case 'g': new_val |= old_umask >> 3;
						  break;
					case 'o': new_val |= old_umask >> 0;
						  break;
					case 'X': if (old_umask & 0111)
							new_val |= 01;
						  break;
					case 's': /* ignored */
						  break;
					}
				new_val = (new_val & 07) * positions;
				switch (op) {
				case '-':
					new_umask &= ~new_val;
					break;
				case '=':
					new_umask = new_val
					    | (new_umask & ~(positions * 07));
					break;
				case '+':
					new_umask |= new_val;
				}
				if (*cp == ',') {
					positions = 0;
					cp++;
				} else if (!strchr("=+-", *cp))
					break;
			}
			if (*cp) {
				bi_errorf("bad mask");
				return 1;
			}
			new_umask = ~new_umask;
		}
		umask(new_umask);
	}
	return 0;
}

int
c_dot(wp)
	char **wp;
{
	char *file, *cp;
	char **argv;
	int argc;
	int i;
	int err;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;

	if ((cp = wp[builtin_opt.optind]) == NULL)
		return 0;
	file = search(cp, path, R_OK, &err);
	if (file == NULL) {
		bi_errorf("%s: %s", cp, err ? strerror(err) : "not found");
		return 1;
	}

	/* Set positional parameters? */
	if (wp[builtin_opt.optind + 1]) {
		argv = wp + builtin_opt.optind;
		argv[0] = e->loc->argv[0]; /* preserve $0 */
		for (argc = 0; argv[argc + 1]; argc++)
			;
	} else {
		argc = 0;
		argv = (char **) 0;
	}
	i = include(file, argc, argv, 0);
	if (i < 0) { /* should not happen */
		bi_errorf("%s: %s", cp, strerror(errno));
		return 1;
	}
	return i;
}

int
c_wait(wp)
	char **wp;
{
	int rv = 0;
	int sig;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	wp += builtin_opt.optind;
	if (*wp == (char *) 0) {
		while (waitfor((char *) 0, &sig) >= 0)
			;
		rv = sig;
	} else {
		for (; *wp; wp++)
			rv = waitfor(*wp, &sig);
		if (rv < 0)
			rv = sig ? sig : 127; /* magic exit code: bad job-id */
	}
	return rv;
}

int
c_read(wp)
	char **wp;
{
	int c = 0;
	int expand = 1, history = 0;
	int expanding;
	int ecode = 0;
	char *cp;
	int fd = 0;
	struct shf *shf;
	int optc;
	const char *emsg;
	XString cs, xs;
	struct tbl *vp;
	char *xp = NULL;

	while ((optc = ksh_getopt(wp, &builtin_opt, "prsu,")) != EOF)
		switch (optc) {
		  case 'p':
			if ((fd = coproc_getfd(R_OK, &emsg)) < 0) {
				bi_errorf("-p: %s", emsg);
				return 1;
			}
			break;
		  case 'r':
			expand = 0;
			break;
		  case 's':
			history = 1;
			break;
		  case 'u':
			if (!*(cp = builtin_opt.optarg))
				fd = 0;
			else if ((fd = check_fd(cp, R_OK, &emsg)) < 0) {
				bi_errorf("-u: %s: %s", cp, emsg);
				return 1;
			}
			break;
		  case '?':
			return 1;
		}
	wp += builtin_opt.optind;

	if (*wp == NULL)
		*--wp = "REPLY";

	/* Since we can't necessarily seek backwards on non-regular files,
	 * don't buffer them so we can't read too much.
	 */
	shf = shf_reopen(fd, SHF_RD | SHF_INTERRUPT | can_seek(fd), shl_spare);

	if ((cp = strchr(*wp, '?')) != NULL) {
		*cp = 0;
		if (isatty(fd)) {
			/* at&t ksh says it prints prompt on fd if it's open
			 * for writing and is a tty, but it doesn't do it
			 * (it also doesn't check the interactive flag,
			 * as is indicated in the Kornshell book).
			 */
			shellf("%s", cp+1);
		}
	}

	/* If we are reading from the co-process for the first time,
	 * make sure the other side of the pipe is closed first.  This allows
	 * the detection of eof.
	 *
	 * This is not compatible with at&t ksh... the fd is kept so another
	 * coproc can be started with same output, however, this means eof
	 * can't be detected...  This is why it is closed here.
	 * If this call is removed, remove the eof check below, too.
	 * coproc_readw_close(fd);
	 */

	if (history)
		Xinit(xs, xp, 128, ATEMP);
	expanding = 0;
	Xinit(cs, cp, 128, ATEMP);
	for (; *wp != NULL; wp++) {
		for (cp = Xstring(cs, cp); ; ) {
			if (c == '\n' || c == EOF)
				break;
			while (1) {
				c = shf_getc(shf);
				if (c == '\0')
					continue;
				if (c == EOF && shf_error(shf)
				    && shf_errno(shf) == EINTR)
				{
					/* Was the offending signal one that
					 * would normally kill a process?
					 * If so, pretend the read was killed.
					 */
					ecode = fatal_trap_check();

					/* non fatal (eg, CHLD), carry on */
					if (!ecode) {
						shf_clearerr(shf);
						continue;
					}
				}
				break;
			}
			if (history) {
				Xcheck(xs, xp);
				Xput(xs, xp, c);
			}
			Xcheck(cs, cp);
			if (expanding) {
				expanding = 0;
				if (c == '\n') {
					c = 0;
					if (Flag(FTALKING_I) && isatty(fd)) {
						/* set prompt in case this is
						 * called from .profile or $ENV
						 */
						set_prompt(PS2, (Source *) 0);
						pprompt(prompt, 0);
					}
				} else if (c != EOF)
					Xput(cs, cp, c);
				continue;
			}
			if (expand && c == '\\') {
				expanding = 1;
				continue;
			}
			if (c == '\n' || c == EOF)
				break;
			if (ctype(c, C_IFS)) {
				if (Xlength(cs, cp) == 0 && ctype(c, C_IFSWS))
					continue;
				if (wp[1])
					break;
			}
			Xput(cs, cp, c);
		}
		/* strip trailing IFS white space from last variable */
		if (!wp[1])
			while (Xlength(cs, cp) && ctype(cp[-1], C_IFS)
			       && ctype(cp[-1], C_IFSWS))
				cp--;
		Xput(cs, cp, '\0');
		vp = global(*wp);
		/* Must be done before setting export. */
		if (vp->flag & RDONLY) {
			shf_flush(shf);
			bi_errorf("%s is read only", *wp);
			return 1;
		}
		if (Flag(FEXPORT))
			typeset(*wp, EXPORT, 0, 0, 0);
		if (!setstr(vp, Xstring(cs, cp), KSH_RETURN_ERROR)) {
		    shf_flush(shf);
		    return 1;
		}
	}

	shf_flush(shf);
	if (history) {
		Xput(xs, xp, '\0');
		source->line++;
		histsave(source->line, Xstring(xs, xp), 1);
		Xfree(xs, xp);
	}
	/* if this is the co-process fd, close the file descriptor
	 * (can get eof if and only if all processes are have died, ie,
	 * coproc.njobs is 0 and the pipe is closed).
	 */
	if (c == EOF && !ecode)
		coproc_read_close(fd);

	return ecode ? ecode : c == EOF;
}

int
c_eval(wp)
	char **wp;
{
	struct source *s;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	s = pushs(SWORDS, ATEMP);
	s->u.strv = wp + builtin_opt.optind;
	if (!Flag(FPOSIX)) {
		/*
		 * Handle case where the command is empty due to failed
		 * command substitution, eg, eval "$(false)".
		 * In this case, shell() will not set/change exstat (because
		 * compiled tree is empty), so will use this value.
		 * subst_exstat is cleared in execute(), so should be 0 if
		 * there were no substitutions.
		 *
		 * A strict reading of POSIX says we don't do this (though
		 * it is traditionally done). [from 1003.2-1992]
		 *    3.9.1: Simple Commands
		 *	... If there is a command name, execution shall
		 *	continue as described in 3.9.1.1.  If there
		 *	is no command name, but the command contained a command
		 *	substitution, the command shall complete with the exit
		 *	status of the last command substitution
		 *    3.9.1.1: Command Search and Execution
		 *	...(1)...(a) If the command name matches the name of
		 *	a special built-in utility, that special built-in
		 *	utility shall be invoked.
		 * 3.14.5: Eval
		 *	... If there are no arguments, or only null arguments,
		 *	eval shall return an exit status of zero.
		 */
		exstat = subst_exstat;
	}

	return shell(s, FALSE);
}

int
c_trap(wp)
	char **wp;
{
	int i;
	char *s;
	Trap *p;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	wp += builtin_opt.optind;

	if (*wp == NULL) {
		int anydfl = 0;

		for (p = sigtraps, i = NSIG+1; --i >= 0; p++) {
			if (p->trap == NULL)
				anydfl = 1;
			else {
				shprintf("trap -- ");
				print_value_quoted(p->trap);
				shprintf(" %s\n", p->name);
			}
		}
#if 0 /* this is ugly and not clear POSIX needs it */
		/* POSIX may need this so output of trap can be saved and
		 * used to restore trap conditions
		 */
		if (anydfl) {
			shprintf("trap -- -");
			for (p = sigtraps, i = NSIG+1; --i >= 0; p++)
				if (p->trap == NULL && p->name)
					shprintf(" %s", p->name);
			shprintf(newline);
		}
#endif
		return 0;
	}

	/*
	 * Use case sensitive lookup for first arg so the
	 * command 'exit' isn't confused with the pseudo-signal
	 * 'EXIT'.
	 */
	s = (gettrap(*wp, FALSE) == NULL) ? *wp++ : NULL; /* get command */
	if (s != NULL && s[0] == '-' && s[1] == '\0')
		s = NULL;

	/* set/clear traps */
	while (*wp != NULL) {
		p = gettrap(*wp++, TRUE);
		if (p == NULL) {
			bi_errorf("bad signal %s", wp[-1]);
			return 1;
		}
		settrap(p, s);
	}
	return 0;
}

int
c_exitreturn(wp)
	char **wp;
{
	int how = LEXIT;
	int n;
	char *arg;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	arg = wp[builtin_opt.optind];

	if (arg) {
	    if (!getn(arg, &n)) {
		    exstat = 1;
		    warningf(TRUE, "%s: bad number", arg);
	    } else
		    exstat = n;
	}
	if (wp[0][0] == 'r') { /* return */
		struct env *ep;

		/* need to tell if this is exit or return so trap exit will
		 * work right (POSIX)
		 */
		for (ep = e; ep; ep = ep->oenv)
			if (STOP_RETURN(ep->type)) {
				how = LRETURN;
				break;
			}
	}

	if (how == LEXIT && !really_exit && j_stopped_running()) {
		really_exit = 1;
		how = LSHELL;
	}

	quitenv();	/* get rid of any i/o redirections */
	unwind(how);
	/*NOTREACHED*/
	return 0;
}

int
c_brkcont(wp)
	char **wp;
{
	int n, quit;
	struct env *ep, *last_ep = (struct env *) 0;
	char *arg;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	arg = wp[builtin_opt.optind];

	if (!arg)
		n = 1;
	else if (!bi_getn(arg, &n))
		return 1;
	quit = n;
	if (quit <= 0) {
		/* at&t ksh does this for non-interactive shells only - weird */
		bi_errorf("%s: bad value", arg);
		return 1;
	}

	/* Stop at E_NONE, E_PARSE, E_FUNC, or E_INCL */
	for (ep = e; ep && !STOP_BRKCONT(ep->type); ep = ep->oenv)
		if (ep->type == E_LOOP) {
			if (--quit == 0)
				break;
			ep->flags |= EF_BRKCONT_PASS;
			last_ep = ep;
		}

	if (quit) {
		/* at&t ksh doesn't print a message - just does what it
		 * can.  We print a message 'cause it helps in debugging
		 * scripts, but don't generate an error (ie, keep going).
		 */
		if (n == quit) {
			warningf(TRUE, "%s: cannot %s", wp[0], wp[0]);
			return 0;
		}
		/* POSIX says if n is too big, the last enclosing loop
		 * shall be used.  Doesn't say to print an error but we
		 * do anyway 'cause the user messed up.
		 */
		last_ep->flags &= ~EF_BRKCONT_PASS;
		warningf(TRUE, "%s: can only %s %d level(s)",
			wp[0], wp[0], n - quit);
	}

	unwind(*wp[0] == 'b' ? LBREAK : LCONTIN);
	/*NOTREACHED*/
}

int
c_set(wp)
	char **wp;
{
	int argi, setargs;
	struct block *l = e->loc;
	char **owp = wp;

	if (wp[1] == NULL) {
		static const char *const args [] = { "set", "-", NULL };
		return c_typeset((char **) args);
	}

	argi = parse_args(wp, OF_SET, &setargs);
	if (argi < 0)
		return 1;
	/* set $# and $* */
	if (setargs) {
		owp = wp += argi - 1;
		wp[0] = l->argv[0]; /* save $0 */
		while (*++wp != NULL)
			*wp = str_save(*wp, &l->area);
		l->argc = wp - owp - 1;
		l->argv = (char **) alloc(sizeofN(char *, l->argc+2), &l->area);
		for (wp = l->argv; (*wp++ = *owp++) != NULL; )
			;
	}
	/* POSIX says set exit status is 0, but old scripts that use
	 * getopt(1), use the construct: set -- `getopt ab:c "$@"`
	 * which assumes the exit value set will be that of the ``
	 * (subst_exstat is cleared in execute() so that it will be 0
	 * if there are no command substitutions).
	 */
	return Flag(FPOSIX) ? 0 : subst_exstat;
}

int
c_unset(wp)
	char **wp;
{
	char *id;
	int optc, unset_var = 1;
	int ret = 0;

	while ((optc = ksh_getopt(wp, &builtin_opt, "fv")) != EOF)
		switch (optc) {
		  case 'f':
			unset_var = 0;
			break;
		  case 'v':
			unset_var = 1;
			break;
		  case '?':
			return 1;
		}
	wp += builtin_opt.optind;
	for (; (id = *wp) != NULL; wp++)
		if (unset_var) {	/* unset variable */
			struct tbl *vp = global(id);

			if (!(vp->flag & ISSET))
			    ret = 1;
			if ((vp->flag&RDONLY)) {
				bi_errorf("%s is read only", vp->name);
				return 1;
			}
			unset(vp, strchr(id, '[') ? 1 : 0);
		} else {		/* unset function */
			if (define(id, (struct op *) NULL))
				ret = 1;
		}
	return ret;
}

int
c_times(wp)
	char **wp;
{
	struct tms all;

	(void) times(&all);
	shprintf("Shell: %8ss user ", clocktos(all.tms_utime));
	shprintf("%8ss system\n", clocktos(all.tms_stime));
	shprintf("Kids:  %8ss user ", clocktos(all.tms_cutime));
	shprintf("%8ss system\n", clocktos(all.tms_cstime));

	return 0;
}

/*
 * time pipeline (really a statement, not a built-in command)
 */
int
timex(t, f)
	struct op *t;
	int f;
{
#define TF_NOARGS	BIT(0)
#define TF_NOREAL	BIT(1)		/* don't report real time */
#define TF_POSIX	BIT(2)		/* report in posix format */
	int rv = 0;
	struct tms t0, t1, tms;
	clock_t t0t, t1t = 0;
	int tf = 0;
	extern clock_t j_usrtime, j_systime; /* computed by j_wait */
	char opts[1];

	t0t = times(&t0);
	if (t->left) {
		/*
		 * Two ways of getting cpu usage of a command: just use t0
		 * and t1 (which will get cpu usage from other jobs that
		 * finish while we are executing t->left), or get the
		 * cpu usage of t->left. at&t ksh does the former, while
		 * pdksh tries to do the later (the j_usrtime hack doesn't
		 * really work as it only counts the last job).
		 */
		j_usrtime = j_systime = 0;
		if (t->left->type == TCOM)
			t->left->str = opts;
		opts[0] = 0;
		rv = execute(t->left, f | XTIME);
		tf |= opts[0];
		t1t = times(&t1);
	} else
		tf = TF_NOARGS;

	if (tf & TF_NOARGS) { /* ksh93 - report shell times (shell+kids) */
		tf |= TF_NOREAL;
		tms.tms_utime = t0.tms_utime + t0.tms_cutime;
		tms.tms_stime = t0.tms_stime + t0.tms_cstime;
	} else {
		tms.tms_utime = t1.tms_utime - t0.tms_utime + j_usrtime;
		tms.tms_stime = t1.tms_stime - t0.tms_stime + j_systime;
	}

	if (!(tf & TF_NOREAL))
		shf_fprintf(shl_out,
			tf & TF_POSIX ? "real %8s\n" : "%8ss real ",
			clocktos(t1t - t0t));
	shf_fprintf(shl_out, tf & TF_POSIX ? "user %8s\n" : "%8ss user ",
		clocktos(tms.tms_utime));
	shf_fprintf(shl_out, tf & TF_POSIX ? "sys  %8s\n" : "%8ss system\n",
		clocktos(tms.tms_stime));
	shf_flush(shl_out);

	return rv;
}

void
timex_hook(t, app)
	struct op *t;
	char ** volatile *app;
{
	char **wp = *app;
	int optc;
	int i, j;
	Getopt opt;

	ksh_getopt_reset(&opt, 0);
	opt.optind = 0;	/* start at the start */
	while ((optc = ksh_getopt(wp, &opt, ":p")) != EOF)
		switch (optc) {
		  case 'p':
			t->str[0] |= TF_POSIX;
			break;
		  case '?':
			errorf("time: -%s unknown option", opt.optarg);
		  case ':':
			errorf("time: -%s requires an argument",
				opt.optarg);
		}
	/* Copy command words down over options. */
	if (opt.optind != 0) {
		for (i = 0; i < opt.optind; i++)
			afree(wp[i], ATEMP);
		for (i = 0, j = opt.optind; (wp[i] = wp[j]); i++, j++)
			;
	}
	if (!wp[0])
		t->str[0] |= TF_NOARGS;
	*app = wp;
}

static char *
clocktos(t)
	clock_t t;
{
	static char temp[22]; /* enough for 64 bit clock_t */
	int i;
	char *cp = temp + sizeof(temp);

	/* note: posix says must use max precision, ie, if clk_tck is
	 * 1000, must print 3 places after decimal (if non-zero, else 1).
	 */
	if (CLK_TCK != 100)	/* convert to 1/100'ths */
	    t = (t < 1000000000/CLK_TCK) ?
		    (t * 100) / CLK_TCK : (t / CLK_TCK) * 100;

	*--cp = '\0';
	for (i = -2; i <= 0 || t > 0; i++) {
		if (i == 0)
			*--cp = '.';
		*--cp = '0' + (char)(t%10);
		t /= 10;
	}
	return cp;
}

/* exec with no args - args case is taken care of in comexec() */
int
c_exec(wp)
	char ** wp;
{
	int i;

	/* make sure redirects stay in place */
	if (e->savefd != NULL) {
		for (i = 0; i < NUFILE; i++) {
			if (e->savefd[i] > 0)
				close(e->savefd[i]);
			/*
			 * For ksh keep anything > 2 private,
			 * for sh, let them be (POSIX says what
			 * happens is unspecified and the bourne shell
			 * keeps them open).
			 */
			if (!Flag(FSH) && i > 2 && e->savefd[i])
				fcntl(i, F_SETFD, FD_CLOEXEC);
		}
		e->savefd = NULL;
	}
	return 0;
}

/* dummy function, special case in comexec() */
int
c_builtin(wp)
	char ** wp;
{
	return 0;
}

extern	int c_test(char **wp);			/* in c_test.c */
extern	int c_ulimit(char **wp);		/* in c_ulimit.c */

/* A leading = means assignments before command are kept;
 * a leading * means a POSIX special builtin;
 * a leading + means a POSIX regular builtin
 * (* and + should not be combined).
 */
const struct builtin shbuiltins [] = {
	{"*=.", c_dot},
	{"*=:", c_label},
	{"[", c_test},
	{"*=break", c_brkcont},
	{"=builtin", c_builtin},
	{"*=continue", c_brkcont},
	{"*=eval", c_eval},
	{"*=exec", c_exec},
	{"*=exit", c_exitreturn},
	{"+false", c_label},
	{"*=return", c_exitreturn},
	{"*=set", c_set},
	{"*=shift", c_shift},
	{"=times", c_times},
	{"*=trap", c_trap},
	{"+=wait", c_wait},
	{"+read", c_read},
	{"test", c_test},
	{"+true", c_label},
	{"ulimit", c_ulimit},
	{"+umask", c_umask},
	{"*=unset", c_unset},
	{NULL, NULL}
};
