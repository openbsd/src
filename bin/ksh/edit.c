/*	$OpenBSD: edit.c,v 1.1.1.1 1996/08/14 06:19:10 downsj Exp $	*/

/*
 * Command line editing - common code
 *
 */

#include "config.h"
#ifdef EDIT

#include "sh.h"
#include "tty.h"
#define EXTERN
#include "edit.h"
#undef EXTERN
#ifdef OS_SCO	/* SCO Unix 3.2v4.1 */
# include <sys/stream.h>	/* needed for <sys/ptem.h> */
# include <sys/ptem.h>		/* needed for struct winsize */
#endif /* OS_SCO */
#include <ctype.h>
#include "ksh_stat.h"

static char	vdisable_c;


/* Called from main */
void
x_init()
{
	/* set to -1 to force initial binding */
	edchars.erase = edchars.kill = edchars.intr = edchars.quit
		= edchars.eof = -1;
	/* default value for deficient systems */
	edchars.werase = 027;	/* ^W */
#ifdef TIOCGWINSZ
	{
		struct winsize ws;

		if (ioctl(tty_fd, TIOCGWINSZ, &ws) >= 0) {
			struct tbl *vp;

			if (ws.ws_col) {
				x_cols = ws.ws_col < MIN_COLS ? MIN_COLS
						: ws.ws_col;
				
				if ((vp = typeset("COLUMNS", EXPORT, 0, 0, 0)))
					setint(vp, (long) ws.ws_col);
			}
			if (ws.ws_row
			    && (vp = typeset("LINES", EXPORT, 0, 0, 0)))
				setint(vp, (long) ws.ws_row);
		}
	}
#endif /* TIOCGWINSZ */
#ifdef EMACS
	x_init_emacs();
#endif /* EMACS */

	/* Bizarreness to figure out how to disable
	 * a struct termios.c_cc[] char
	 */
#ifdef _POSIX_VDISABLE
	if (_POSIX_VDISABLE >= 0)
		vdisable_c = (char) _POSIX_VDISABLE;
	else
		/* `feature not available' */
		vdisable_c = (char) 0377;
#else
# if defined(HAVE_PATHCONF) && defined(_PC_VDISABLE)
	vdisable_c = fpathconf(tty_fd, _PC_VDISABLE);
# else
	vdisable_c = (char) 0377;	/* default to old BSD value */
# endif
#endif /* _POSIX_VDISABLE */
}

/*
 * read an edited command line
 */
int
x_read(buf, len)
	char *buf;
	size_t len;
{
	int	i;

	x_mode(TRUE);
#ifdef EMACS
	if (Flag(FEMACS) || Flag(FGMACS))
		i = x_emacs(buf, len);
	else
#endif
#ifdef VI
	if (Flag(FVI))
		i = x_vi(buf, len);
	else
#endif
		i = -1;		/* internal error */
	x_mode(FALSE);
	return i;
}

/* tty I/O */

int
x_getc()
{
#ifdef OS2
	unsigned char c = _read_kbd(0, 1, 0);
	return c == 0 ? 0xE0 : c;
#else /* OS2 */
	char c;
	int n;

	while ((n = blocking_read(0, &c, 1)) < 0 && errno == EINTR)
		if (trap) {
			x_mode(FALSE);
			runtraps(0);
			x_mode(TRUE);
		}
	if (n != 1)
		return -1;
	return (int) (unsigned char) c;
#endif /* OS2 */
}

void
x_flush()
{
	shf_flush(shl_out);
}

void
x_putc(c)
	int c;
{
	shf_putc(c, shl_out);
}

void
x_puts(s)
	const char *s;
{
	while (*s != 0)
		shf_putc(*s++, shl_out);
}

bool_t
x_mode(onoff)
	bool_t	onoff;
{
	static bool_t	x_cur_mode;
	bool_t		prev;

	if (x_cur_mode == onoff)
		return x_cur_mode;
	prev = x_cur_mode;
	x_cur_mode = onoff;

	if (onoff) {
		TTY_state	cb;
		X_chars		oldchars;
		
		oldchars = edchars;
		cb = tty_state;

#if defined(HAVE_TERMIOS_H) || defined(HAVE_TERMIO_H)
		edchars.erase = cb.c_cc[VERASE];
		edchars.kill = cb.c_cc[VKILL];
		edchars.intr = cb.c_cc[VINTR];
		edchars.quit = cb.c_cc[VQUIT];
		edchars.eof = cb.c_cc[VEOF];
# ifdef VWERASE
		edchars.werase = cb.c_cc[VWERASE];
# endif
# ifdef _CRAY2		/* brain-damaged terminal handler */
		cb.c_lflag &= ~(ICANON|ECHO);
		/* rely on print routine to map '\n' to CR,LF */
# else
		cb.c_iflag &= ~(INLCR|ICRNL);
#  ifdef _BSD_SYSV	/* need to force CBREAK instead of RAW (need CRMOD on output) */
		cb.c_lflag &= ~(ICANON|ECHO);
#  else
#   ifdef SWTCH	/* need CBREAK to handle swtch char */
		cb.c_lflag &= ~(ICANON|ECHO);
		cb.c_lflag |= ISIG;
		cb.c_cc[VINTR] = vdisable_c;
		cb.c_cc[VQUIT] = vdisable_c;
#   else
		cb.c_lflag &= ~(ISIG|ICANON|ECHO);
#   endif
#  endif
#  ifdef VLNEXT
		/* osf/1 processes lnext when ~icanon */
		cb.c_cc[VLNEXT] = vdisable_c;
#  endif /* VLNEXT */
#  ifdef VDISCARD
		/* sunos 4.1.x & osf/1 processes discard(flush) when ~icanon */
		cb.c_cc[VDISCARD] = vdisable_c;
#  endif /* VDISCARD */
		cb.c_cc[VTIME] = 0;
		cb.c_cc[VMIN] = 1;
# endif	/* _CRAY2 */
#else
	/* Assume BSD tty stuff. */
		edchars.erase = cb.sgttyb.sg_erase;
		edchars.kill = cb.sgttyb.sg_kill;
		cb.sgttyb.sg_flags &= ~ECHO;
		cb.sgttyb.sg_flags |= CBREAK;
#  ifdef TIOCGATC
		edchars.intr = cb.lchars.tc_intrc;
		edchars.quit = cb.lchars.tc_quitc;
		edchars.eof = cb.lchars.tc_eofc;
		edchars.werase = cb.lchars.tc_werasc;
		cb.lchars.tc_suspc = -1;
		cb.lchars.tc_dsuspc = -1;
		cb.lchars.tc_lnextc = -1;
		cb.lchars.tc_statc = -1;
		cb.lchars.tc_intrc = -1;
		cb.lchars.tc_quitc = -1;
		cb.lchars.tc_rprntc = -1;
#  else
		edchars.intr = cb.tchars.t_intrc;
		edchars.quit = cb.tchars.t_quitc;
		edchars.eof = cb.tchars.t_eofc;
		cb.tchars.t_intrc = -1;
		cb.tchars.t_quitc = -1;
#   ifdef TIOCGLTC
		edchars.werase = cb.ltchars.t_werasc;
		cb.ltchars.t_suspc = -1;
		cb.ltchars.t_dsuspc = -1;
		cb.ltchars.t_lnextc = -1;
		cb.ltchars.t_rprntc = -1;
#   endif
#  endif /* TIOCGATC */
#endif /* HAVE_TERMIOS_H || HAVE_TERMIO_H */

		set_tty(tty_fd, &cb, TF_WAIT);

		if (memcmp(&edchars, &oldchars, sizeof(edchars)) != 0) {
#ifdef EMACS
			x_emacs_keys(&edchars);
#endif
		}
	} else
		/* TF_WAIT doesn't seem to be necessary when leaving xmode */
		set_tty(tty_fd, &tty_state, TF_NONE);

	return prev;
}

/* NAME:
 *      promptlen - calculate the length of PS1 etc.
 *
 * DESCRIPTION:
 *      This function is based on a fix from guy@demon.co.uk
 *      It fixes a bug in that if PS1 contains '!', the length 
 *      given by strlen() is probably wrong.
 *
 * RETURN VALUE:
 *      length
 */
 
int
promptlen(cp, spp)
    const char  *cp;
    const char **spp;
{
    int count = 0;
    const char *sp = cp;

    while (*cp) {
	if (*cp == '\n' || *cp == '\r') {
	    count = 0;
	    cp++;
	    sp = cp;
	} else if (*cp == '\t') {
	    count = (count | 7) + 1;
	    cp++;
	} else if (*cp == '\b') {
	    if (count > 0)
		count--;
	    cp++;
	}
#if 1
	else
	  cp++, count++;
#else
	else if (*cp++ != '!')
	  count++;
	else if (*cp == '!') {
	    cp++;
	    count++;
	} else {
	    register int i = source->line + 1;

	    do
		count++;
	    while ((i /= 10) > 0);
	}
#endif /* 1 */
    }
    if (spp)
	*spp = sp;
    return count;
}

void
set_editmode(ed)
	const char *ed;
{
	static const enum sh_flag edit_flags[] = {
#ifdef EMACS
			FEMACS, FGMACS,
#endif
#ifdef VI
			FVI,
#endif
		    };
	char *rcp;
	int i;
  
	if ((rcp = ksh_strrchr_dirsep(ed)))
		ed = ++rcp;
	for (i = 0; i < NELEM(edit_flags); i++)
		if (strstr(ed, options[(int) edit_flags[i]].name)) {
			change_flag(edit_flags[i], OF_SPECIAL, 1);
			return;
		}
}

/* ------------------------------------------------------------------------- */
/*           Common file/command completion code for vi/emacs	             */


static char	*add_glob ARGS((const char *str, int slen));
static void	glob_table ARGS((const char *pat, XPtrV *wp, struct table *tp));
static void	glob_path ARGS((int flags, const char *pat, XPtrV *wp,
				const char *path));

/* XXX not used... */
int
x_complete_word(str, slen, is_command, nwordsp, ret)
	const char *str;
	int slen;
	int is_command;
	int *nwordsp;
	char **ret;
{
	int nwords;
	int prefix_len;
	char **words;

	nwords = (is_command ? x_command_glob : x_file_glob)(XCF_FULLPATH,
				str, slen, &words);
	*nwordsp = nwords;
	if (nwords == 0) {
		*ret = (char *) 0;
		return -1;
	}

	prefix_len = x_longest_prefix(nwords, words);
	*ret = str_nsave(words[0], prefix_len, ATEMP);
	x_free_words(nwords, words);
	return prefix_len;
}

void
x_print_expansions(nwords, words, is_command)
	int nwords;
	char *const *words;
	int is_command;
{
	int use_copy = 0;
	int prefix_len;
	XPtrV l;

	/* Check if all matches are in the same directory (in this
	 * case, we want to omitt the directory name)
	 */
	if (!is_command
	    && (prefix_len = x_longest_prefix(nwords, words)) > 0)
	{
		int i;

		/* Special case for 1 match (prefix is whole word) */
		if (nwords == 1)
			prefix_len = x_basename(words[0], (char *) 0);
		/* Any (non-trailing) slashes in non-common word suffixes? */
		for (i = 0; i < nwords; i++)
			if (x_basename(words[i] + prefix_len, (char *) 0)
							> prefix_len)
				break;
		/* All in same directory? */
		if (i == nwords) {
			while (prefix_len > 0
			       && !ISDIRSEP(words[0][prefix_len - 1]))
				prefix_len--;
			use_copy = 1;
			XPinit(l, nwords + 1);
			for (i = 0; i < nwords; i++)
				XPput(l, words[i] + prefix_len);
			XPput(l, (char *) 0);
		}
	}

	/*
	 * Enumerate expansions
	 */
	x_putc('\r');
	x_putc('\n');
	pr_menu(use_copy ? (char **) XPptrv(l) : words);

	if (use_copy)
		XPfree(l); /* not x_free_words() */
}

/*
 *  Do file globbing:
 *	- appends * to (copy of) str if no globbing chars found
 *	- does expansion, checks for no match, etc.
 *	- sets *wordsp to array of matching strings
 *	- returns number of matching strings
 */
/* XXX static? */
int
x_file_glob(flags, str, slen, wordsp)
	int flags;
	const char *str;
	int slen;
	char ***wordsp;
{
	char *toglob;
	char **words;
	int nwords;
	XPtrV w;
	struct source *s, *sold;

	if (slen <= 0)
		return 0;

	toglob = add_glob(str, slen);

	/*
	 * Convert "foo*" (toglob) to an array of strings (words)
	 */
	sold = source;
	s = pushs(SWSTR, ATEMP);
	s->start = s->str = toglob;
	source = s;
	if (yylex(ONEWORD) != LWORD) {
		source = sold;
		internal_errorf(0, "fileglob: substitute error");
		return 0;
	}
	source = sold;
	XPinit(w, 32);
	expand(yylval.cp, &w, DOGLOB|DOTILDE|DOMARKDIRS);
	XPput(w, NULL);
	words = (char **) XPclose(w);

	for (nwords = 0; words[nwords]; nwords++)
		;
	if (nwords == 1) {
		struct stat statb;

		/* Check if globbing failed (returned glob pattern),
		 * but be careful (E.g. toglob == "ab*" when the file
		 * "ab*" exists is not an error).
		 * Also, check for empty result - happens if we tried
		 * to glob something which evaluated to an empty
		 * string (e.g., "$FOO" when there is no FOO, etc).
		 */
		if ((strcmp(words[0], toglob) == 0
		     && stat(words[0], &statb) < 0)
		    || words[0][0] == '\0')
		{
			x_free_words(nwords, words);
			nwords = 0;
		}
	}
	afree(toglob, ATEMP);

	*wordsp = nwords ? words : (char **) 0;

	return nwords;
}

/* Data structure used in x_command_glob() */
struct path_order_info {
	char *word;
	int base;
	int path_order;
};

/* Compare routine used in x_command_glob() */
static int
path_order_cmp(aa, bb)
	const void *aa;
	const void *bb;
{
	const struct path_order_info *a = (const struct path_order_info *) aa;
	const struct path_order_info *b = (const struct path_order_info *) bb;
	int t;

	t = FILECMP(a->word + a->base, b->word + b->base);
	return t ? t : a->path_order - b->path_order;
}

/* XXX static? */
int
x_command_glob(flags, str, slen, wordsp)
	int flags;
	const char *str;
	int slen;
	char ***wordsp;
{
	char *toglob;
	char *pat;
	char *fpath;
	int nwords;
	XPtrV w;
	struct block *l;

	if (slen <= 0)
		return 0;

	toglob = add_glob(str, slen);

	/* Convert "foo*" (toglob) to a pattern for future use */
	pat = evalstr(toglob, DOPAT|DOTILDE);
	afree(toglob, ATEMP);

	XPinit(w, 32);

	glob_table(pat, &w, &keywords);
	glob_table(pat, &w, &aliases);
	glob_table(pat, &w, &builtins);
	for (l = e->loc; l; l = l->next)
		glob_table(pat, &w, &l->funs);

	glob_path(flags, pat, &w, path);
	if ((fpath = str_val(global("FPATH"))) != null)
		glob_path(flags, pat, &w, fpath);

	nwords = XPsize(w);

	if (!nwords) {
		*wordsp = (char **) 0;
		XPfree(w);
		return 0;
	}

	/* Sort entries */
	if (flags & XCF_FULLPATH) {
		/* Sort by basename, then path order */
		struct path_order_info *info;
		struct path_order_info *last_info = 0;
		char **words = (char **) XPptrv(w);
		int path_order = 0;
		int i;

		info = (struct path_order_info *)
			alloc(sizeof(struct path_order_info) * nwords, ATEMP);
		for (i = 0; i < nwords; i++) {
			info[i].word = words[i];
			info[i].base = x_basename(words[i], (char *) 0);
			if (!last_info || info[i].base != last_info->base
			    || FILENCMP(words[i],
					last_info->word, info[i].base) != 0)
			{
				last_info = &info[i];
				path_order++;
			}
			info[i].path_order = path_order;
		}
		qsort(info, nwords, sizeof(struct path_order_info),
			path_order_cmp);
		for (i = 0; i < nwords; i++)
			words[i] = info[i].word;
		afree((void *) info, ATEMP);
	} else {
		/* Sort and remove duplicate entries */
		char **words = (char **) XPptrv(w);
		int i, j;

		qsortp(XPptrv(w), (size_t) nwords, xstrcmp);

		for (i = j = 0; i < nwords - 1; i++) {
			if (strcmp(words[i], words[i + 1]))
				words[j++] = words[i];
			else
				afree(words[i], ATEMP);
		}
		words[j++] = words[i];
		nwords = j;
		w.cur = (void **) &words[j];
	}

	XPput(w, NULL);
	*wordsp = (char **) XPclose(w);

	return nwords;
}

#define IS_WORDC(c)	!isspace(c)

/* XXX static? */
int
x_locate_word(buf, buflen, pos, startp, is_commandp)
	const char *buf;
	int buflen;
	int pos;
	int *startp;
	int *is_commandp;
{
	int p;
	int start, end;

	if (pos == buflen)
		pos--;
	if (pos < 0 || pos >= buflen) {
		*startp = pos;
		*is_commandp = 0;
		return 0;
	}

	/* Go backwards 'til we are in a word */
	for (start = pos; start >= 0 && !IS_WORDC(buf[start]); start--)
		;
	/* No word found? */
	if (start < 0)
		return 0;
	/* Keep going backwards to start of word */
	for (; start >= 0 && IS_WORDC(buf[start]); start--)
		;
	start++;

	/* Go forwards to end of word */
	for (end = start; end < buflen && IS_WORDC(buf[end]); end++)
		;

	if (is_commandp) {
		int iscmd;

		/* Figure out if this is a command */
		for (p = start - 1; p >= 0 && isspace(buf[p]); p--)
			;
		iscmd = p < 0 || strchr(";|&()", buf[p]);
		if (iscmd) {
			/* If command has a /, path, etc. is not searched;
			 * only current directory is searched, which is just
			 * like file globbing.
			 */
			for (p = start; p < end; p++)
				if (ISDIRSEP(buf[p]))
					break;
			iscmd = p == end;
		}
		*is_commandp = iscmd;
	}

	*startp = start;

	return end - start;
}

int
x_cf_glob(flags, buf, buflen, pos, startp, endp, wordsp, is_commandp)
	int flags;
	const char *buf;
	int buflen;
	int pos;
	int *startp;
	int *endp;
	char ***wordsp;
	int *is_commandp;
{
	int len;
	int nwords;
	char **words;
	int is_command;

	len = x_locate_word(buf, buflen, pos, startp, &is_command);
	if (len == 0)
		return 0;

	if (!(flags & XCF_COMMAND))
		is_command = 0;
	nwords = (is_command ? x_command_glob : x_file_glob)(flags,
				    buf + *startp, len, &words);
	if (nwords == 0) {
		*wordsp = (char **) 0;
		return 0;
	}

	if (is_commandp)
		*is_commandp = is_command;
	*wordsp = words;
	*endp = *startp + len;

	return nwords;
}

/* Given a string, copy it and possibly add a '*' to the end.  The
 * new string is returned.
 */
static char *
add_glob(str, slen)
	const char *str;
	int slen;
{
	char *toglob;
	char *s;

	if (slen <= 0)
		return (char *) 0;

	toglob = str_nsave(str, slen + 1, ATEMP); /* + 1 for "*" */
	toglob[slen] = '\0';

	/*
	 * If the pathname contains a wildcard (an unquoted '*',
	 * '?', or '[') or parameter expansion ('$'), then it is globbed
	 * based on that value (i.e., without the appended '*').
	 */
	for (s = toglob; *s; s++) {
		if (*s == '\\' && s[1])
			s++;
		else if (*s == '*' || *s == '[' || *s == '?' || *s == '$'
			 || (s[1] == '(' /*)*/ && strchr("*+?@!", *s)))
			break;
	}
	if (!*s) {
		toglob[slen] = '*';
		toglob[slen + 1] = '\0';
	}

	return toglob;
}

/*
 * Find longest common prefix
 */
int
x_longest_prefix(nwords, words)
	int nwords;
	char *const *words;
{
	int i, j;
	int prefix_len;
	char *p;

	if (nwords <= 0)
		return 0;

	prefix_len = strlen(words[0]);
	for (i = 1; i < nwords; i++)
		for (j = 0, p = words[i]; j < prefix_len; j++)
			if (FILECHCONV(p[j]) != FILECHCONV(words[0][j])) {
				prefix_len = j;
				break;
			}
	return prefix_len;
}

void
x_free_words(nwords, words)
	int nwords;
	char **words;
{
	int i;

	for (i = 0; i < nwords; i++)
		if (words[i])
			afree(words[i], ATEMP);
	afree(words, ATEMP);
}

/* Return the offset of the basename of string s (which ends at se - need not
 * be null terminated).  Trailing slashes are ignored.  If s is just a slash,
 * then the offset is 0 (actually, length - 1).
 *	s		Return
 *	/etc		1
 *	/etc/		1
 *	/etc//		1
 *	/etc/fo		5
 *	foo		0
 *	///		2
 *			0
 */
int
x_basename(s, se)
	const char *s;
	const char *se;
{
	const char *p;

	if (se == (char *) 0)
		se = s + strlen(s);
	if (s == se)
		return 0;

	/* Skip trailing slashes */
	for (p = se - 1; p > s && ISDIRSEP(*p); p--)
		;
	for (; p > s && !ISDIRSEP(*p); p--)
		;
	if (ISDIRSEP(*p) && p + 1 < se)
		p++;

	return p - s;
}

/*
 *  Apply pattern matching to a table: all table entries that match a pattern
 * are added to wp.
 */
static void
glob_table(pat, wp, tp)
	const char *pat;
	XPtrV *wp;
	struct table *tp;
{
	struct tstate ts;
	struct tbl *te;

	for (twalk(&ts, tp); (te = tnext(&ts)); ) {
		if (gmatch(te->name, pat, FALSE))
			XPput(*wp, str_save(te->name, ATEMP));
	}
}

static void
glob_path(flags, pat, wp, path)
	int flags;
	const char *pat;
	XPtrV *wp;
	const char *path;
{
	const char *sp, *p;
	char *xp;
	int pathlen;
	int patlen;
	int oldsize, newsize, i, j;
	char **words;
	XString xs;

	patlen = strlen(pat) + 1;
	sp = path;
	Xinit(xs, xp, patlen + 128, ATEMP);
	while (sp) {
		xp = Xstring(xs, xp);
		if (!(p = strchr(sp, PATHSEP)))
			p = sp + strlen(sp);
		pathlen = p - sp;
		if (pathlen) {
			/* Copy sp into xp, stuffing any MAGIC characters
			 * on the way
			 */
			const char *s = sp;

			XcheckN(xs, xp, pathlen * 2);
			while (s < p) {
				if (ISMAGIC(*s))
					*xp++ = MAGIC;
				*xp++ = *s++;
			}
			*xp++ = DIRSEP;
			pathlen++;
		}
		sp = p;
		XcheckN(xs, xp, patlen);
		memcpy(xp, pat, patlen);

		oldsize = XPsize(*wp);
		glob_str(Xstring(xs, xp), wp, 0);
		newsize = XPsize(*wp);

		/* Check that each match is executable... */
		words = (char **) XPptrv(*wp);
		for (i = j = oldsize; i < newsize; i++) {
			if (search_access(words[i], X_OK) >= 0) {
				words[j] = words[i];
				if (!(flags & XCF_FULLPATH))
					memmove(words[j], words[j] + pathlen,
						strlen(words[j] + pathlen) + 1);
				j++;
			} else
				afree(words[i], ATEMP);
		}
		wp->cur = (void **) &words[j];

		if (!*sp++)
			break;
	}
	Xfree(xs, xp);
}

#endif /* EDIT */
