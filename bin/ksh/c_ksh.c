/*	$OpenBSD: c_ksh.c,v 1.13 2000/11/21 22:41:03 millert Exp $	*/

/*
 * built-in Korn commands: c_*
 */

#include "sh.h"
#include "ksh_stat.h"
#include <ctype.h>

#ifdef __CYGWIN__
#include <sys/cygwin.h>
#endif /* __CYGWIN__ */

int
c_cd(wp)
	char	**wp;
{
	int optc;
	int physical = Flag(FPHYSICAL);
	int cdnode;			/* was a node from cdpath added in? */
	int printpath = 0;		/* print where we cd'd? */
	int rval;
	struct tbl *pwd_s, *oldpwd_s;
	XString xs;
	char *xp;
	char *dir, *try, *pwd;
	int phys_path;
	char *cdpath;

	while ((optc = ksh_getopt(wp, &builtin_opt, "LP")) != EOF)
		switch (optc) {
		case 'L':
			physical = 0;
			break;
		case 'P':
			physical = 1;
			break;
		case '?':
			return 1;
		}
	wp += builtin_opt.optind;

	if (Flag(FRESTRICTED)) {
		bi_errorf("restricted shell - can't cd");
		return 1;
	}

	pwd_s = global("PWD");
	oldpwd_s = global("OLDPWD");

	if (!wp[0]) {
		/* No arguments - go home */
		if ((dir = str_val(global("HOME"))) == null) {
			bi_errorf("no home directory (HOME not set)");
			return 1;
		}
	} else if (!wp[1]) {
		/* One argument: - or dir */
		dir = wp[0];
		if (strcmp(dir, "-") == 0) {
			dir = str_val(oldpwd_s);
			if (dir == null) {
				bi_errorf("no OLDPWD");
				return 1;
			}
			printpath++;
		}
	} else if (!wp[2]) {
		/* Two arguments - substitute arg1 in PWD for arg2 */
		int ilen, olen, nlen, elen;
		char *cp;

		if (!current_wd[0]) {
			bi_errorf("don't know current directory");
			return 1;
		}
		/* substitue arg1 for arg2 in current path.
		 * if the first substitution fails because the cd fails
		 * we could try to find another substitution. For now
		 * we don't
		 */
		if ((cp = strstr(current_wd, wp[0])) == (char *) 0) {
			bi_errorf("bad substitution");
			return 1;
		}
		ilen = cp - current_wd;
		olen = strlen(wp[0]);
		nlen = strlen(wp[1]);
		elen = strlen(current_wd + ilen + olen) + 1;
		dir = alloc(ilen + nlen + elen, ATEMP);
		memcpy(dir, current_wd, ilen);
		memcpy(dir + ilen, wp[1], nlen);
		memcpy(dir + ilen + nlen, current_wd + ilen + olen, elen);
		printpath++;
	} else {
		bi_errorf("too many arguments");
		return 1;
	}

	Xinit(xs, xp, PATH, ATEMP);
	/* xp will have a bogus value after make_path() - set it to 0
	 * so that if it's used, it will cause a dump
	 */
	xp = (char *) 0;

	cdpath = str_val(global("CDPATH"));
	do {
		cdnode = make_path(current_wd, dir, &cdpath, &xs, &phys_path);
#ifdef S_ISLNK
		if (physical)
			rval = chdir(try = Xstring(xs, xp) + phys_path);
		else
#endif /* S_ISLNK */
		{
			simplify_path(Xstring(xs, xp));
			rval = chdir(try = Xstring(xs, xp));
		}
	} while (rval < 0 && cdpath != (char *) 0);

	if (rval < 0) {
		if (cdnode)
			bi_errorf("%s: bad directory", dir);
		else
			bi_errorf("%s - %s", try, strerror(errno));
		return 1;
	}

	/* Clear out tracked aliases with relative paths */
	flushcom(0);

	/* Set OLDPWD (note: unsetting OLDPWD does not disable this
	 * setting in at&t ksh)
	 */
	if (current_wd[0])
		/* Ignore failure (happens if readonly or integer) */
		setstr(oldpwd_s, current_wd, KSH_RETURN_ERROR);

	if (!ISABSPATH(Xstring(xs, xp))) {
#ifdef OS2
		/* simplify_path() doesn't know about os/2's drive contexts,
		 * so it can't set current_wd when changing to a:foo.
		 * Handle this by calling getcwd()...
		 */
		pwd = ksh_get_wd((char *) 0, 0);
#else /* OS2 */
		pwd = (char *) 0;
#endif /* OS2 */
	} else
#ifdef S_ISLNK
	if (!physical || !(pwd = get_phys_path(Xstring(xs, xp))))
#endif /* S_ISLNK */
		pwd = Xstring(xs, xp);

	/* Set PWD */
	if (pwd) {
#ifdef __CYGWIN__
		char ptmp[PATH];  /* larger than MAX_PATH */
		cygwin_conv_to_full_posix_path(pwd, ptmp);
#else /* __CYGWIN__ */
		char *ptmp = pwd;
#endif /* __CYGWIN__ */
		set_current_wd(ptmp);
		/* Ignore failure (happens if readonly or integer) */
		setstr(pwd_s, ptmp, KSH_RETURN_ERROR);
	} else {
		set_current_wd(null);
		pwd = Xstring(xs, xp);
		/* XXX unset $PWD? */
	}
	if (printpath || cdnode)
		shprintf("%s\n", pwd);

	return 0;
}

int
c_pwd(wp)
	char	**wp;
{
	int optc;
	int physical = Flag(FPHYSICAL);
	char *p;

	while ((optc = ksh_getopt(wp, &builtin_opt, "LP")) != EOF)
		switch (optc) {
		case 'L':
			physical = 0;
			break;
		case 'P':
			physical = 1;
			break;
		case '?':
			return 1;
		}
	wp += builtin_opt.optind;

	if (wp[0]) {
		bi_errorf("too many arguments");
		return 1;
	}
#ifdef S_ISLNK
	p = current_wd[0] ? (physical ? get_phys_path(current_wd) : current_wd)
			  : (char *) 0;
#else /* S_ISLNK */
	p = current_wd[0] ? current_wd : (char *) 0;
#endif /* S_ISLNK */
	if (p && eaccess(p, R_OK) < 0)
		p = (char *) 0;
	if (!p) {
		p = ksh_get_wd((char *) 0, 0);
		if (!p) {
			bi_errorf("can't get current directory - %s",
				strerror(errno));
			return 1;
		}
	}
	shprintf("%s\n", p);
	return 0;
}

int
c_print(wp)
	char **wp;
{
#define PO_NL		BIT(0)	/* print newline */
#define PO_EXPAND	BIT(1)	/* expand backslash sequences */
#define PO_PMINUSMINUS	BIT(2)	/* print a -- argument */
#define PO_HIST		BIT(3)	/* print to history instead of stdout */
#define PO_COPROC	BIT(4)	/* printing to coprocess: block SIGPIPE */
#define PO_FSLASH	BIT(5)  /* swap slash for backslash (for os2 ) */
	int fd = 1;
	int flags = PO_EXPAND|PO_NL;
	char *s;
	const char *emsg;
	XString xs;
	char *xp;

	if (wp[0][0] == 'e') {	/* echo command */
		int nflags = flags;

		/* A compromise between sysV and BSD echo commands:
		 * escape sequences are enabled by default, and
		 * -n, -e and -E are recognized if they appear
		 * in arguments with no illegal options (ie, echo -nq
		 * will print -nq).
		 * Different from sysV echo since options are recognized,
		 * different from BSD echo since escape sequences are enabled
		 * by default.
		 */
		wp += 1;
		while ((s = *wp) && *s == '-' && s[1]) {
			while (*++s)
				if (*s == 'n')
					nflags &= ~PO_NL;
				else if (*s == 'e')
					nflags |= PO_EXPAND;
				else if (*s == 'E')
					nflags &= ~PO_EXPAND;
				else
					/* bad option: don't use nflags, print
					 * argument
					 */
					break;
			if (*s)
				break;
			wp++;
			flags = nflags;
		}
	} else {
		int optc;
#if OS2
		const char *options = "Rnpfrsu,"; /* added f flag */
#else
		const char *options = "Rnprsu,";
#endif
		while ((optc = ksh_getopt(wp, &builtin_opt, options)) != EOF)
			switch (optc) {
			  case 'R': /* fake BSD echo command */
				flags |= PO_PMINUSMINUS;
				flags &= ~PO_EXPAND;
				options = "ne";
				break;
			  case 'e':
				flags |= PO_EXPAND;
				break;
#ifdef OS2
			  case 'f':
				flags |= PO_FSLASH;
				break;
#endif
			  case 'n':
				flags &= ~PO_NL;
				break;
#ifdef KSH
			  case 'p':
				if ((fd = coproc_getfd(W_OK, &emsg)) < 0) {
					bi_errorf("-p: %s", emsg);
					return 1;
				}
				break;
#endif /* KSH */
			  case 'r':
				flags &= ~PO_EXPAND;
				break;
			  case 's':
				flags |= PO_HIST;
				break;
			  case 'u':
				if (!*(s = builtin_opt.optarg))
					fd = 0;
				else if ((fd = check_fd(s, W_OK, &emsg)) < 0) {
					bi_errorf("-u: %s: %s", s, emsg);
					return 1;
				}
				break;
			  case '?':
				return 1;
			}
		if (!(builtin_opt.info & GI_MINUSMINUS)) {
			/* treat a lone - like -- */
			if (wp[builtin_opt.optind]
			    && strcmp(wp[builtin_opt.optind], "-") == 0)
				builtin_opt.optind++;
		} else if (flags & PO_PMINUSMINUS)
			builtin_opt.optind--;
		wp += builtin_opt.optind;
	}

	Xinit(xs, xp, 128, ATEMP);

	while (*wp != NULL) {
		register int c;
		s = *wp;
		while ((c = *s++) != '\0') {
			Xcheck(xs, xp);
#ifdef OS2
			if ((flags & PO_FSLASH) && c == '\\') 
				if (*s == '\\')
					*s++;
				else
					c = '/';
#endif /* OS2 */
			if ((flags & PO_EXPAND) && c == '\\') {
				int i;

				switch ((c = *s++)) {
				/* Oddly enough, \007 seems more portable than
				 * \a (due to HP-UX cc, Ultrix cc, old pcc's,
				 * etc.).
				 */
				case 'a': c = '\007'; break;
				case 'b': c = '\b'; break;
				case 'c': flags &= ~PO_NL;
					  continue; /* AT&T brain damage */
				case 'f': c = '\f'; break;
				case 'n': c = '\n'; break;
				case 'r': c = '\r'; break;
				case 't': c = '\t'; break;
				case 'v': c = 0x0B; break;
				case '0':
					/* Look for an octal number: can have
					 * three digits (not counting the
					 * leading 0).  Truely burnt.
					 */
					c = 0;
					for (i = 0; i < 3; i++) {
						if (*s >= '0' && *s <= '7')
							c = c*8 + *s++ - '0';
						else
							break;
					}
					break;
				case '\0': s--; c = '\\'; break;
				case '\\': break;
				default:
					Xput(xs, xp, '\\');
				}
			}
			Xput(xs, xp, c);
		}
		if (*++wp != NULL)
			Xput(xs, xp, ' ');
	}
	if (flags & PO_NL)
		Xput(xs, xp, '\n');

	if (flags & PO_HIST) {
		Xput(xs, xp, '\0');
		source->line++;
		histsave(source->line, Xstring(xs, xp), 1);
		Xfree(xs, xp);
	} else {
		int n, len = Xlength(xs, xp);
		int UNINITIALIZED(opipe);
#ifdef KSH

		/* Ensure we aren't killed by a SIGPIPE while writing to
		 * a coprocess.  at&t ksh doesn't seem to do this (seems
		 * to just check that the co-process is alive, which is
		 * not enough).
		 */
		if (coproc.write >= 0 && coproc.write == fd) {
			flags |= PO_COPROC;
			opipe = block_pipe();
		}
#endif /* KSH */
		for (s = Xstring(xs, xp); len > 0; ) {
			n = write(fd, s, len);
			if (n < 0) {
#ifdef KSH
				if (flags & PO_COPROC)
					restore_pipe(opipe);
#endif /* KSH */
				if (errno == EINTR) {
					/* allow user to ^C out */
					intrcheck();
#ifdef KSH
					if (flags & PO_COPROC)
						opipe = block_pipe();
#endif /* KSH */
					continue;
				}
#ifdef KSH
				/* This doesn't really make sense - could
				 * break scripts (print -p generates
				 * error message).
				*if (errno == EPIPE)
				*	coproc_write_close(fd);
				 */
#endif /* KSH */
				return 1;
			}
			s += n;
			len -= n;
		}
#ifdef KSH
		if (flags & PO_COPROC)
			restore_pipe(opipe);
#endif /* KSH */
	}

	return 0;
}

int
c_whence(wp)
	char **wp;
{
	struct tbl *tp;
	char *id;
	int pflag = 0, vflag = 0, Vflag = 0;
	int ret = 0;
	int optc;
	int iam_whence = wp[0][0] == 'w';
	int fcflags;
	const char *options = iam_whence ? "pv" : "pvV";

	while ((optc = ksh_getopt(wp, &builtin_opt, options)) != EOF)
		switch (optc) {
		case 'p':
			pflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case 'V':
			Vflag = 1;
			break;
		case '?':
			return 1;
		}
	wp += builtin_opt.optind;


	fcflags = FC_BI | FC_PATH | FC_FUNC;
	if (!iam_whence) {
		/* Note that -p on its own is deal with in comexec() */
		if (pflag)
			fcflags |= FC_DEFPATH;
		/* Convert command options to whence options - note that
		 * command -pV uses a different path search than whence -v
		 * or whence -pv.  This should be considered a feature.
		 */
		vflag = Vflag;
	}
	if (pflag)
		fcflags &= ~(FC_BI | FC_FUNC);

	while ((vflag || ret == 0) && (id = *wp++) != NULL) {
		tp = NULL;
		if ((iam_whence || vflag) && !pflag)
			tp = tsearch(&keywords, id, hash(id));
		if (!tp && !pflag) {
			tp = tsearch(&aliases, id, hash(id));
			if (tp && !(tp->flag & ISSET))
				tp = NULL;
		}
		if (!tp)
			tp = findcom(id, fcflags);
		if (vflag || (tp->type != CALIAS && tp->type != CEXEC
			      && tp->type != CTALIAS))
			shprintf("%s", id);
		switch (tp->type) {
		  case CKEYWD:
			if (vflag)
				shprintf(" is a reserved word");
			break;
		  case CALIAS:
			if (vflag)
				shprintf(" is an %salias for ",
					(tp->flag & EXPORT) ? "exported "
							    : null);
			if (!iam_whence && !vflag)
				shprintf("alias %s=", id);
			print_value_quoted(tp->val.s);
			break;
		  case CFUNC:
			if (vflag) {
				shprintf(" is a");
				if (tp->flag & EXPORT)
					shprintf("n exported");
				if (tp->flag & TRACE)
					shprintf(" traced");
				if (!(tp->flag & ISSET)) {
					shprintf(" undefined");
					if (tp->u.fpath)
						shprintf(" (autoload from %s)",
							tp->u.fpath);
				}
				shprintf(" function");
			}
			break;
		  case CSHELL:
			if (vflag)
				shprintf(" is a%s shell builtin",
				    (tp->flag & SPEC_BI) ? " special" : null);
			break;
		  case CTALIAS:
		  case CEXEC:
			if (tp->flag & ISSET) {
				if (vflag) {
					shprintf(" is ");
					if (tp->type == CTALIAS)
						shprintf(
						    "a tracked %salias for ",
							(tp->flag & EXPORT) ?
								"exported "
							      : null);
				}
				shprintf("%s", tp->val.s);
			} else {
				if (vflag)
					shprintf(" not found");
				ret = 1;
			}
			break;
		  default:
			shprintf("%s is *GOK*", id);
			break;
		}
		if (vflag || !ret)
			shprintf(newline);
	}
	return ret;
}

/* Deal with command -vV - command -p dealt with in comexec() */
int
c_command(wp)
	char **wp;
{
	/* Let c_whence do the work.  Note that c_command() must be
	 * a distinct function from c_whence() (tested in comexec()).
	 */
	return c_whence(wp);
}

/* typeset, export, and readonly */
int
c_typeset(wp)
	char **wp;
{
	struct block *l = e->loc;
	struct tbl *vp, **p;
	Tflag fset = 0, fclr = 0;
	int thing = 0, func = 0, local = 0;
	const char *options = "L#R#UZ#fi#lprtux";	/* see comment below */
	char *fieldstr, *basestr;
	int field, base;
	int optc;
	Tflag flag;
	int pflag = 0;

	switch (**wp) {
 	  case 'e':		/* export */
 		fset |= EXPORT;
		options = "p";
 		break;
 	  case 'r':		/* readonly */
 		fset |= RDONLY;
		options = "p";
 		break;
	  case 's':		/* set */
		/* called with 'typeset -' */
		break;
 	  case 't':		/* typeset */
 		local = 1;
 		break;
 	}
 
	fieldstr = basestr = (char *) 0;
	builtin_opt.flags |= GF_PLUSOPT;
	/* at&t ksh seems to have 0-9 as options, which are multiplied
	 * to get a number that is used with -L, -R, -Z or -i (eg, -1R2
	 * sets right justify in a field of 12).  This allows options
	 * to be grouped in an order (eg, -Lu12), but disallows -i8 -L3 and
	 * does not allow the number to be specified as a separate argument
	 * Here, the number must follow the RLZi option, but is optional
	 * (see the # kludge in ksh_getopt()).
	 */
	while ((optc = ksh_getopt(wp, &builtin_opt, options)) != EOF) {
		flag = 0;
		switch (optc) {
		  case 'L':
			flag = LJUST;
			fieldstr = builtin_opt.optarg;
			break;
		  case 'R':
			flag = RJUST;
			fieldstr = builtin_opt.optarg;
			break;
		  case 'U':
			/* at&t ksh uses u, but this conflicts with
			 * upper/lower case.  If this option is changed,
			 * need to change the -U below as well
			 */
			flag = INT_U;
			break;
		  case 'Z':
			flag = ZEROFIL;
			fieldstr = builtin_opt.optarg;
			break;
		  case 'f':
			func = 1;
			break;
		  case 'i':
			flag = INTEGER;
			basestr = builtin_opt.optarg;
			break;
		  case 'l':
			flag = LCASEV;
			break;
		  case 'p': /* posix export/readonly -p flag.
			     * typset -p is the same as typeset (in pdksh);
			     * here for compatability with ksh93.
			     */
			pflag = 1;
			break;
		  case 'r':
			flag = RDONLY;
			break;
		  case 't':
			flag = TRACE;
			break;
		  case 'u':
			flag = UCASEV_AL;	/* upper case / autoload */
			break;
		  case 'x':
			flag = EXPORT;
			break;
		  case '?':
			return 1;
		}
		if (builtin_opt.info & GI_PLUS) {
			fclr |= flag;
			fset &= ~flag;
			thing = '+';
		} else {
			fset |= flag;
			fclr &= ~flag;
			thing = '-';
		}
	}

	field = 0;
	if (fieldstr && !bi_getn(fieldstr, &field))
		return 1;
	base = 0;
	if (basestr && !bi_getn(basestr, &base))
		return 1;

	if (!(builtin_opt.info & GI_MINUSMINUS) && wp[builtin_opt.optind]
	    && (wp[builtin_opt.optind][0] == '-'
		|| wp[builtin_opt.optind][0] == '+')
	    && wp[builtin_opt.optind][1] == '\0')
	{
		thing = wp[builtin_opt.optind][0];
		builtin_opt.optind++;
	}

	if (func && ((fset|fclr) & ~(TRACE|UCASEV_AL|EXPORT))) {
		bi_errorf("only -t, -u and -x options may be used with -f");
		return 1;
	}
	if (wp[builtin_opt.optind]) {
		/* Take care of exclusions.  
		 * At this point, flags in fset are cleared in fclr and vise
		 * versa.  This property should be preserved.
		 */
		if (fset & LCASEV)	/* LCASEV has priority over UCASEV_AL */
			fset &= ~UCASEV_AL;
		if (fset & LJUST)	/* LJUST has priority over RJUST */
			fset &= ~RJUST;
		if ((fset & (ZEROFIL|LJUST)) == ZEROFIL) { /* -Z implies -ZR */
			fset |= RJUST;
			fclr &= ~RJUST;
		}
		/* Setting these attributes clears the others, unless they
		 * are also set in this command
		 */
		if (fset & (LJUST|RJUST|ZEROFIL|UCASEV_AL|LCASEV|INTEGER
			    |INT_U|INT_L))
			fclr |= ~fset &
				(LJUST|RJUST|ZEROFIL|UCASEV_AL|LCASEV|INTEGER
				 |INT_U|INT_L);
	}

	/* set variables and attributes */
	if (wp[builtin_opt.optind]) {
		int i;
		int rval = 0;
		struct tbl *f;

		if (local && !func)
			fset |= LOCAL;
		for (i = builtin_opt.optind; wp[i]; i++) {
			if (func) {
				f = findfunc(wp[i], hash(wp[i]),
					     (fset&UCASEV_AL) ? TRUE : FALSE);
				if (!f) {
					/* at&t ksh does ++rval: bogus */
					rval = 1;
					continue;
				}
				if (fset | fclr) {
					f->flag |= fset;
					f->flag &= ~fclr;
				} else
					fptreef(shl_stdout, 0,
						f->flag & FKSH ?
						    "function %s %T\n"
						    : "%s() %T\n"
						,
						wp[i], f->val.t);
			} else if (!typeset(wp[i], fset, fclr, field, base)) {
				bi_errorf("%s: not identifier", wp[i]);
				return 1;
			}
		}
		return rval;
	}

	/* list variables and attributes */
	flag = fset | fclr; /* no difference at this point.. */
	if (func) {
	    for (l = e->loc; l; l = l->next) {
		for (p = tsort(&l->funs); (vp = *p++); ) {
		    if (flag && (vp->flag & flag) == 0)
			    continue;
		    if (thing == '-')
			fptreef(shl_stdout, 0, vp->flag & FKSH ?
						    "function %s %T\n"
						    : "%s() %T\n",
				vp->name, vp->val.t);
		    else
			shprintf("%s\n", vp->name);
		}
	    }
	} else {
	    for (l = e->loc; l; l = l->next) {
		for (p = tsort(&l->vars); (vp = *p++); ) {
		    struct tbl *tvp;
		    int any_set = 0;
		    /*
		     * See if the parameter is set (for arrays, if any
		     * element is set).
		     */
		    for (tvp = vp; tvp; tvp = tvp->u.array)
			if (tvp->flag & ISSET) {
			    any_set = 1;
			    break;
			}
		    /*
		     * Check attributes - note that all array elements
		     * have (should have?) the same attributes, so checking
		     * the first is sufficient.
		     *
		     * Report an unset param only if the user has
		     * explicitly given it some attribute (like export);
		     * otherwise, after "echo $FOO", we would report FOO...
		     */
		    if (!any_set && !(vp->flag & USERATTRIB))
			continue;
		    if (flag && (vp->flag & flag) == 0)
			continue;
		    for (; vp; vp = vp->u.array) {
			/* Ignore array elements that aren't set unless there
			 * are no set elements, in which case the first is
			 * reported on
			 */
			if ((vp->flag&ARRAY) && any_set && !(vp->flag & ISSET))
			    continue;
			/* no arguments */
			if (thing == 0 && flag == 0) {
			    /* at&t ksh prints things like export, integer,
			     * leftadj, zerofill, etc., but POSIX says must
			     * be suitable for re-entry...
			     */
			    shprintf("typeset ");
			    if ((vp->flag&INTEGER))
				shprintf("-i ");
			    if ((vp->flag&EXPORT))
				shprintf("-x ");
			    if ((vp->flag&RDONLY))
				shprintf("-r ");
			    if ((vp->flag&TRACE)) 
				shprintf("-t ");
			    if ((vp->flag&LJUST)) 
				shprintf("-L%d ", vp->u2.field);
			    if ((vp->flag&RJUST)) 
				shprintf("-R%d ", vp->u2.field);
			    if ((vp->flag&ZEROFIL)) 
				shprintf("-Z ");
			    if ((vp->flag&LCASEV)) 
				shprintf("-l ");
			    if ((vp->flag&UCASEV_AL)) 
				shprintf("-u ");
			    if ((vp->flag&INT_U)) 
				shprintf("-U ");
			    shprintf("%s\n", vp->name);
			    if (vp->flag&ARRAY)
				break;
			} else {
			    if (pflag)
				shprintf("%s ",
				    (flag & EXPORT) ?  "export" : "readonly");
			    if ((vp->flag&ARRAY) && any_set)
				shprintf("%s[%d]", vp->name, vp->index);
			    else
				shprintf("%s", vp->name);
			    if (thing == '-' && (vp->flag&ISSET)) {
				char *s = str_val(vp);

				shprintf("=");
				/* at&t ksh can't have justified integers.. */
				if ((vp->flag & (INTEGER|LJUST|RJUST))
								== INTEGER)
				    shprintf("%s", s);
				else
				    print_value_quoted(s);
			    }
			    shprintf(newline);
			}
			/* Only report first `element' of an array with
			 * no set elements.
			 */
			if (!any_set)
			    break;
		    }
		}
	    }
	}
	return 0;
}
	
int
c_alias(wp)
	char **wp;
{
	struct table *t = &aliases;
	int rv = 0, rflag = 0, tflag, Uflag = 0, pflag = 0;
	int prefix = 0;
	Tflag xflag = 0;
	int optc;

	builtin_opt.flags |= GF_PLUSOPT;
	while ((optc = ksh_getopt(wp, &builtin_opt, "dprtUx")) != EOF) {
		prefix = builtin_opt.info & GI_PLUS ? '+' : '-';
		switch (optc) {
		  case 'd':
			t = &homedirs;
			break;
		  case 'p':
			pflag = 1;
			break;
		  case 'r':
			rflag = 1;
			break;
		  case 't':
			t = &taliases;
			break;
		  case 'U': /* kludge for tracked alias initialization
			     * (don't do a path search, just make an entry)
			     */
			Uflag = 1;
			break;
		  case 'x':
			xflag = EXPORT;
			break;
		  case '?':
			return 1;
		}
	}
	wp += builtin_opt.optind;

	if (!(builtin_opt.info & GI_MINUSMINUS) && *wp
	    && (wp[0][0] == '-' || wp[0][0] == '+') && wp[0][1] == '\0')
	{
		prefix = wp[0][0];
		wp++;
	}

	tflag = t == &taliases;

	/* "hash -r" means reset all the tracked aliases.. */
	if (rflag) {
		static const char *const args[] = {
			    "unalias", "-ta", (const char *) 0
			};

		if (!tflag || *wp) {
			shprintf(
	    "alias: -r flag can only be used with -t and without arguments\n");
			return 1;
		}
		ksh_getopt_reset(&builtin_opt, GF_ERROR);
		return c_unalias((char **) args);
	}

	
	if (*wp == NULL) {
		struct tbl *ap, **p;

		for (p = tsort(t); (ap = *p++) != NULL; )
			if ((ap->flag & (ISSET|xflag)) == (ISSET|xflag)) {
				if (pflag)
					shf_puts("alias ", shl_stdout);
				shf_puts(ap->name, shl_stdout);
				if (prefix != '+') {
					shf_putc('=', shl_stdout);
					print_value_quoted(ap->val.s);
				}
				shprintf(newline);
			}
	}

	for (; *wp != NULL; wp++) {
		char *alias = *wp;
		char *val = strchr(alias, '=');
		char *newval;
		struct tbl *ap;
		int h;

		if (val)
			alias = str_nsave(alias, val++ - alias, ATEMP);
		h = hash(alias);
		if (val == NULL && !tflag && !xflag) {
			ap = tsearch(t, alias, h);
			if (ap != NULL && (ap->flag&ISSET)) {
				if (pflag)
					shf_puts("alias ", shl_stdout);
				shf_puts(ap->name, shl_stdout);
				if (prefix != '+') {
					shf_putc('=', shl_stdout);
					print_value_quoted(ap->val.s);
				}
				shprintf(newline);
			} else {
				shprintf("%s alias not found\n", alias);
				rv = 1;
			}
			continue;
		}
		ap = tenter(t, alias, h);
		ap->type = tflag ? CTALIAS : CALIAS;
		/* Are we setting the value or just some flags? */
		if ((val && !tflag) || (!val && tflag && !Uflag)) {
			if (ap->flag&ALLOC) {
				ap->flag &= ~(ALLOC|ISSET);
				afree((void*)ap->val.s, APERM);
			}
			/* ignore values for -t (at&t ksh does this) */
			newval = tflag ? search(alias, path, X_OK, (int *) 0)
					: val;
			if (newval) {
				ap->val.s = str_save(newval, APERM);
				ap->flag |= ALLOC|ISSET;
			} else
				ap->flag &= ~ISSET;
		}
		ap->flag |= DEFINED;
		if (prefix == '+')
			ap->flag &= ~xflag;
		else
			ap->flag |= xflag;
		if (val)
			afree(alias, ATEMP);
	}

	return rv;
}

int
c_unalias(wp)
	char **wp;
{
	register struct table *t = &aliases;
	register struct tbl *ap;
	int rv = 0, all = 0;
	int optc;

	while ((optc = ksh_getopt(wp, &builtin_opt, "adt")) != EOF)
		switch (optc) {
		  case 'a':
			all = 1;
			break;
		  case 'd':
			t = &homedirs;
			break;
		  case 't':
			t = &taliases;
			break;
		  case '?':
			return 1;
		}
	wp += builtin_opt.optind;

	for (; *wp != NULL; wp++) {
		ap = tsearch(t, *wp, hash(*wp));
		if (ap == NULL) {
			rv = 1;	/* POSIX */
			continue;
		}
		if (ap->flag&ALLOC) {
			ap->flag &= ~(ALLOC|ISSET);
			afree((void*)ap->val.s, APERM);
		}
		ap->flag &= ~(DEFINED|ISSET|EXPORT);
	}

	if (all) {
		struct tstate ts;

		for (twalk(&ts, t); (ap = tnext(&ts)); ) {
			if (ap->flag&ALLOC) {
				ap->flag &= ~(ALLOC|ISSET);
				afree((void*)ap->val.s, APERM);
			}
			ap->flag &= ~(DEFINED|ISSET|EXPORT);
		}
	}

	return rv;
}

#ifdef KSH
int
c_let(wp)
	char **wp;
{
	int rv = 1;
	long val;

	if (wp[1] == (char *) 0) /* at&t ksh does this */
		bi_errorf("no arguments");
	else
		for (wp++; *wp; wp++)
			if (!evaluate(*wp, &val, KSH_RETURN_ERROR)) {
				rv = 2;	/* distinguish error from zero result */
				break;
			} else
				rv = val == 0;
	return rv;
}
#endif /* KSH */

int
c_jobs(wp)
	char **wp;
{
	int optc;
	int flag = 0;
	int nflag = 0;
	int rv = 0;

	while ((optc = ksh_getopt(wp, &builtin_opt, "lpnz")) != EOF)
		switch (optc) {
		  case 'l':
			flag = 1;
			break;
		  case 'p':
			flag = 2;
			break;
		  case 'n':
			nflag = 1;
			break;
		  case 'z':	/* debugging: print zombies */
			nflag = -1;
			break;
		  case '?':
			return 1;
		}
	wp += builtin_opt.optind;
	if (!*wp) {
		if (j_jobs((char *) 0, flag, nflag))
			rv = 1;
	} else {
		for (; *wp; wp++)
			if (j_jobs(*wp, flag, nflag))
				rv = 1;
	}
	return rv;
}

#ifdef JOBS
int
c_fgbg(wp)
	char **wp;
{
	int bg = strcmp(*wp, "bg") == 0;
	int UNINITIALIZED(rv);

	if (!Flag(FMONITOR)) {
		bi_errorf("job control not enabled");
		return 1;
	}
	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	wp += builtin_opt.optind;
	if (*wp)
		for (; *wp; wp++)
			rv = j_resume(*wp, bg);
	else
		rv = j_resume("%%", bg);
	/* POSIX says fg shall return 0 (unless an error occurs).
	 * at&t ksh returns the exit value of the job...
	 */
	return (bg || Flag(FPOSIX)) ? 0 : rv;
}
#endif

struct kill_info {
	int num_width;
	int name_width;
};
static char *kill_fmt_entry ARGS((void *arg, int i, char *buf, int buflen));

/* format a single kill item */
static char *
kill_fmt_entry(arg, i, buf, buflen)
	void *arg;
	int i;
	char *buf;
	int buflen;
{
	struct kill_info *ki = (struct kill_info *) arg;

	i++;
	if (sigtraps[i].name)
		shf_snprintf(buf, buflen, "%*d %*s %s",
			ki->num_width, i,
			ki->name_width, sigtraps[i].name,
			sigtraps[i].mess);
	else
		shf_snprintf(buf, buflen, "%*d %*d %s",
			ki->num_width, i,
			ki->name_width, sigtraps[i].signal,
			sigtraps[i].mess);
	return buf;
}


int
c_kill(wp)
	char **wp;
{
	Trap *t = (Trap *) 0;
	char *p;
	int lflag = 0;
	int i, n, rv, sig;

	/* assume old style options if -digits or -UPPERCASE */
	if ((p = wp[1]) && *p == '-' && (digit(p[1]) || isupper(p[1]))) {
		if (!(t = gettrap(p + 1, TRUE))) {
			bi_errorf("bad signal `%s'", p + 1);
			return 1;
		}
		i = (wp[2] && strcmp(wp[2], "--") == 0) ? 3 : 2;
	} else {
		int optc;

		while ((optc = ksh_getopt(wp, &builtin_opt, "ls:")) != EOF)
			switch (optc) {
			  case 'l':
				lflag = 1;
				break;
			  case 's':
				if (!(t = gettrap(builtin_opt.optarg, TRUE))) {
					bi_errorf("bad signal `%s'",
						builtin_opt.optarg);
					return 1;
				}
				break;
			  case '?':
				return 1;
			}
		i = builtin_opt.optind;
	}
	if ((lflag && t) || (!wp[i] && !lflag)) {
		shf_fprintf(shl_out,
"Usage: kill [ -s signame | -signum | -signame ] {pid|job}...\n\
       kill -l [exit_status]\n"
			);
		bi_errorf(null);
		return 1;
	}

	if (lflag) {
		if (wp[i]) {
			for (; wp[i]; i++) {
				if (!bi_getn(wp[i], &n))
					return 1;
				if (n > 128 && n < 128 + SIGNALS)
					n -= 128;
				if (n > 0 && n < SIGNALS && sigtraps[n].name)
					shprintf("%s\n", sigtraps[n].name);
				else
					shprintf("%d\n", n);
			}
		} else if (Flag(FPOSIX)) {
			p = null;
			for (i = 1; i < SIGNALS; i++, p = space)
				if (sigtraps[i].name)
					shprintf("%s%s", p, sigtraps[i].name);
			shprintf(newline);
		} else {
			int w, i;
			int mess_width;
			struct kill_info ki;

			for (i = SIGNALS, ki.num_width = 1; i >= 10; i /= 10)
				ki.num_width++;
			ki.name_width = mess_width = 0;
			for (i = 0; i < SIGNALS; i++) {
				w = sigtraps[i].name ? strlen(sigtraps[i].name)
						     : ki.num_width;
				if (w > ki.name_width)
					ki.name_width = w;
				w = strlen(sigtraps[i].mess);
				if (w > mess_width)
					mess_width = w;
			}

			print_columns(shl_stdout, SIGNALS - 1,
				kill_fmt_entry, (void *) &ki,
				ki.num_width + ki.name_width + mess_width + 3);
		}
		return 0;
	}
	rv = 0;
	sig = t ? t->signal : SIGTERM;
	for (; (p = wp[i]); i++) {
		if (*p == '%') {
			if (j_kill(p, sig))
				rv = 1;
		} else if (!getn(p, &n)) {
			bi_errorf("%s: arguments must be jobs or process ids",
				p);
			rv = 1;
		} else {
			/* use killpg if < -1 since -1 does special things for
			 * some non-killpg-endowed kills
			 */
			if ((n < -1 ? killpg(-n, sig) : kill(n, sig)) < 0) {
				bi_errorf("%s: %s", p, strerror(errno));
				rv = 1;
			}
		}
	}
	return rv;
}

void
getopts_reset(val)
	int val;
{
	if (val >= 1) {
		ksh_getopt_reset(&user_opt,
			GF_NONAME | (Flag(FPOSIX) ? 0 : GF_PLUSOPT));
		user_opt.optind = user_opt.uoptind = val;
	}
}

int
c_getopts(wp)
	char **wp;
{
	int	argc;
	const char *options;
	const char *var;
	int	optc;
	int	ret;
	char	buf[3];
	struct tbl *vq, *voptarg;

	if (ksh_getopt(wp, &builtin_opt, null) == '?')
		return 1;
	wp += builtin_opt.optind;

	options = *wp++;
	if (!options) {
		bi_errorf("missing options argument");
		return 1;
	}

	var = *wp++;
	if (!var) {
		bi_errorf("missing name argument");
		return 1;
	}
	if (!*var || *skip_varname(var, TRUE)) {
		bi_errorf("%s: is not an identifier", var);
		return 1;
	}

	if (e->loc->next == (struct block *) 0) {
		internal_errorf(0, "c_getopts: no argv");
		return 1;
	}
	/* Which arguments are we parsing... */
	if (*wp == (char *) 0)
		wp = e->loc->next->argv;
	else
		*--wp = e->loc->next->argv[0];

	/* Check that our saved state won't cause a core dump... */
	for (argc = 0; wp[argc]; argc++)
		;
	if (user_opt.optind > argc
	    || (user_opt.p != 0
		&& user_opt.p > strlen(wp[user_opt.optind - 1])))
	{
	      bi_errorf("arguments changed since last call");
	      return 1;
	}

	user_opt.optarg = (char *) 0;
	optc = ksh_getopt(wp, &user_opt, options);

	if (optc >= 0 && optc != '?' && (user_opt.info & GI_PLUS)) {
		buf[0] = '+';
		buf[1] = optc;
		buf[2] = '\0';
	} else {
		/* POSIX says var is set to ? at end-of-options, at&t ksh
		 * sets it to null - we go with POSIX...
		 */
		buf[0] = optc < 0 ? '?' : optc;
		buf[1] = '\0';
	}

	/* at&t ksh does not change OPTIND if it was an unknown option.
	 * Scripts counting on this are prone to break... (ie, don't count
	 * on this staying).
	 */
	if (optc != '?') {
		user_opt.uoptind = user_opt.optind;
	}

	voptarg = global("OPTARG");
	voptarg->flag &= ~RDONLY;	/* at&t ksh clears ro and int */
	/* Paranoia: ensure no bizarre results. */
	if (voptarg->flag & INTEGER)
	    typeset("OPTARG", 0, INTEGER, 0, 0);
	if (user_opt.optarg == (char *) 0)
		unset(voptarg, 0);
	else
		/* This can't fail (have cleared readonly/integer) */
		setstr(voptarg, user_opt.optarg, KSH_RETURN_ERROR);

	ret = 0;

	vq = global(var);
	/* Error message already printed (integer, readonly) */
	if (!setstr(vq, buf, KSH_RETURN_ERROR))
	    ret = 1;
	if (Flag(FEXPORT))
		typeset(var, EXPORT, 0, 0, 0);

	return optc < 0 ? 1 : ret;
}

#ifdef EMACS
int
c_bind(wp)
	char **wp;
{
	int rv = 0, macro = 0, list = 0;
	register char *cp;
	int optc;

	while ((optc = ksh_getopt(wp, &builtin_opt, "lm")) != EOF)
		switch (optc) {
		  case 'l':
			list = 1;
			break;
		  case 'm':
			macro = 1;
			break;
		  case '?':
			return 1;
		}
	wp += builtin_opt.optind;

	if (*wp == NULL)	/* list all */
		rv = x_bind((char*)NULL, (char*)NULL, 0, list);

	for (; *wp != NULL; wp++) {
		cp = strchr(*wp, '=');
		if (cp != NULL)
			*cp++ = '\0';
		if (x_bind(*wp, cp, macro, 0))
			rv = 1;
	}

	return rv;
}
#endif

/* A leading = means assignments before command are kept;
 * a leading * means a POSIX special builtin;
 * a leading + means a POSIX regular builtin
 * (* and + should not be combined).
 */
const struct builtin kshbuiltins [] = {
	{"+alias", c_alias},	/* no =: at&t manual wrong */
	{"+cd", c_cd},
	{"+command", c_command},
	{"echo", c_print},
 	{"*=export", c_typeset},
#ifdef HISTORY
	{"+fc", c_fc},
#endif /* HISTORY */
	{"+getopts", c_getopts},
	{"+jobs", c_jobs},
	{"+kill", c_kill},
#ifdef KSH
	{"let", c_let},
#endif /* KSH */
	{"print", c_print},
	{"pwd", c_pwd},
 	{"*=readonly", c_typeset},
	{"=typeset", c_typeset},
	{"+unalias", c_unalias},
	{"whence", c_whence},
#ifdef JOBS
	{"+bg", c_fgbg},
	{"+fg", c_fgbg},
#endif
#ifdef EMACS
	{"bind", c_bind},
#endif
	{NULL, NULL}
};
