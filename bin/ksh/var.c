/*	$OpenBSD: var.c,v 1.8 1999/01/08 20:25:02 millert Exp $	*/

#include "sh.h"
#include "ksh_time.h"
#include "ksh_limval.h"
#include "ksh_stat.h"
#include <ctype.h>

/*
 * Variables
 *
 * WARNING: unreadable code, needs a rewrite
 *
 * if (flag&INTEGER), val.i contains integer value, and type contains base.
 * otherwise, (val.s + type) contains string value.
 * if (flag&EXPORT), val.s contains "name=value" for E-Z exporting.
 */
static	struct tbl vtemp;
static	struct table specials;
static char	*formatstr	ARGS((struct tbl *vp, const char *s));
static void	export		ARGS((struct tbl *vp, const char *val));
static int	special		ARGS((const char *name));
static void	getspec		ARGS((struct tbl *vp));
static void	setspec		ARGS((struct tbl *vp));
static void	unsetspec	ARGS((struct tbl *vp));
static struct tbl *arraysearch  ARGS((struct tbl *, int));

/*
 * create a new block for function calls and simple commands
 * assume caller has allocated and set up e->loc
 */
void
newblock()
{
	register struct block *l;
	static char *const empty[] = {null};

	l = (struct block *) alloc(sizeof(struct block), ATEMP);
	l->flags = 0;
	ainit(&l->area);
	if (!e->loc) {
		l->argc = 0;
		l->argv = (char **) empty;
	} else {
		l->argc = e->loc->argc;
		l->argv = e->loc->argv;
	}
	l->exit = l->error = NULL;
	tinit(&l->vars, &l->area, 0);
	tinit(&l->funs, &l->area, 0);
	l->next = e->loc;
	e->loc = l;
}

/*
 * pop a block handling special variables
 */
void
popblock()
{
	register struct block *l = e->loc;
	register struct tbl *vp, **vpp = l->vars.tbls, *vq;
	register int i;

	e->loc = l->next;	/* pop block */
	for (i = l->vars.size; --i >= 0; )
		if ((vp = *vpp++) != NULL && (vp->flag&SPECIAL))
			if ((vq = global(vp->name))->flag & ISSET)
				setspec(vq);
			else
				unsetspec(vq);
	if (l->flags & BF_DOGETOPTS)
		user_opt = l->getopts_state;
	afreeall(&l->area);
	afree(l, ATEMP);
}

/* called by main() to initialize variable data structures */
void
initvar()
{
	static const struct {
		const char *name;
		int v;
	} names[] = {
			{ "COLUMNS",		V_COLUMNS },
			{ "IFS",		V_IFS },
			{ "OPTIND",		V_OPTIND },
			{ "PATH",		V_PATH },
			{ "POSIXLY_CORRECT",	V_POSIXLY_CORRECT },
			{ "TMPDIR",		V_TMPDIR },
#ifdef HISTORY
			{ "HISTFILE",		V_HISTFILE },
			{ "HISTSIZE",		V_HISTSIZE },
#endif /* HISTORY */
#ifdef EDIT
			{ "EDITOR",		V_EDITOR },
			{ "VISUAL",		V_VISUAL },
#endif /* EDIT */
#ifdef KSH
			{ "MAIL",		V_MAIL },
			{ "MAILCHECK",		V_MAILCHECK },
			{ "MAILPATH",		V_MAILPATH },
			{ "RANDOM",		V_RANDOM },
			{ "SECONDS",		V_SECONDS },
			{ "TMOUT",		V_TMOUT },
#endif /* KSH */
			{ (char *) 0,	0 }
		};
	int i;
	struct tbl *tp;

	tinit(&specials, APERM, 32); /* must be 2^n (currently 16 speciasl) */
	for (i = 0; names[i].name; i++) {
		tp = tenter(&specials, names[i].name, hash(names[i].name));
		tp->flag = DEFINED|ISSET;
		tp->type = names[i].v;
	}
}

/* Used to calculate an array index for global()/local().  Sets *arrayp to
 * non-zero if this is an array, sets *valp to the array index, returns
 * the basename of the array.
 */
const char *
array_index_calc(n, arrayp, valp)
	const char *n;
	bool_t *arrayp;
	int *valp;
{
	const char *p;
	int len;

	*arrayp = FALSE;
	p = skip_varname(n, FALSE);
	if (p != n && *p == '[' && (len = array_ref_len(p))) {
		char *sub, *tmp;
		long rval;

		/* Calculate the value of the subscript */
		*arrayp = TRUE;
		tmp = str_nsave(p+1, len-2, ATEMP);
		sub = substitute(tmp, 0);
		afree(tmp, ATEMP);
		n = str_nsave(n, p - n, ATEMP);
		evaluate(sub, &rval, FALSE);
		if (rval < 0 || rval > ARRAYMAX)
			errorf("%s: subscript out of range", n);
		*valp = rval;
		afree(sub, ATEMP);
	}
	return n;
}

/*
 * Search for variable, if not found create globally.
 */
struct tbl *
global(n)
	register const char *n;
{
	register struct block *l = e->loc;
	register struct tbl *vp;
	register int c;
	unsigned h;
	bool_t	 array;
	int	 val;

	/* Check to see if this is an array */
	n = array_index_calc(n, &array, &val);
	h = hash(n);
	c = n[0];
	if (!letter(c)) {
		if (array)
			errorf("bad substitution");
		vp = &vtemp;
		vp->flag = (DEFINED|RDONLY);
		vp->type = 0;
		vp->areap = ATEMP;
		*vp->name = c;
		if (digit(c)) {
			for (c = 0; digit(*n); n++)
				c = c*10 + *n-'0';
			if (c <= l->argc)
				setstr(vp, l->argv[c]);
			return vp;
		}
		if (n[1] != '\0')
			return vp;
		vp->flag |= ISSET|INTEGER;
		switch (c) {
		  case '$':
			vp->val.i = kshpid;
			break;
		  case '!':
			/* If no job, expand to nothing */
			if ((vp->val.i = j_async()) == 0)
				vp->flag &= ~(ISSET|INTEGER);
			break;
		  case '?':
			vp->val.i = exstat;
			break;
		  case '#':
			vp->val.i = l->argc;
			break;
		  case '-':
			vp->flag &= ~INTEGER;
			vp->val.s = getoptions();
			break;
		  default:
			vp->flag &= ~(ISSET|INTEGER);
		}
		return vp;
	}
	for (l = e->loc; ; l = l->next) {
		vp = tsearch(&l->vars, n, h);
		if (vp != NULL)
			if (array)
				return arraysearch(vp, val);
			else
				return vp;
		if (l->next == NULL)
			break;
	}
	vp = tenter(&l->vars, n, h);
	if (array)
		vp = arraysearch(vp, val);
	vp->flag |= DEFINED;
	if (special(n))
		vp->flag |= SPECIAL;
	return vp;
}

/*
 * Search for local variable, if not found create locally.
 */
struct tbl *
local(n, copy)
	register const char *n;
	bool_t copy;
{
	register struct block *l = e->loc;
	register struct tbl *vp;
	unsigned h;
	bool_t	 array;
	int	 val;

	/* Check to see if this is an array */
	n = array_index_calc(n, &array, &val);
	h = hash(n);
	if (!letter(*n)) {
		vp = &vtemp;
		vp->flag = DEFINED|RDONLY;
		vp->type = 0;
		vp->areap = ATEMP;
		return vp;
	}
	vp = tenter(&l->vars, n, h);
	if (copy && !(vp->flag & DEFINED)) {
		struct block *ll = l;
		struct tbl *vq = (struct tbl *) 0;

		while ((ll = ll->next) && !(vq = tsearch(&ll->vars, n, h)))
			;
		if (vq) {
			vp->flag |= vq->flag & (EXPORT|INTEGER|RDONLY
						|LJUST|RJUST|ZEROFIL
						|LCASEV|UCASEV_AL|INT_U|INT_L);
			if (vq->flag & INTEGER)
				vp->type = vq->type;
			vp->u2.field = vq->u2.field;
		}
	}
	if (array)
		vp = arraysearch(vp, val);
	vp->flag |= DEFINED;
	if (special(n))
		vp->flag |= SPECIAL;
	return vp;
}

/* get variable string value */
char *
str_val(vp)
	register struct tbl *vp;
{
	char *s;

	if ((vp->flag&SPECIAL))
		getspec(vp);
	if (!(vp->flag&ISSET))
		s = null;		/* special to dollar() */
	else if (!(vp->flag&INTEGER))	/* string source */
		s = vp->val.s + vp->type;
	else {				/* integer source */
		/* worst case number length is when base=2, so use BITS(long) */
			     /* minus base #     number    null */
		static char strbuf[1 + 2 + 1 + BITS(long) + 1];
		const char *digits = (vp->flag & UCASEV_AL) ?
				  "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				: "0123456789abcdefghijklmnopqrstuvwxyz";
		register unsigned long n;
		register int base;

		s = strbuf + sizeof(strbuf);
		if (vp->flag & INT_U)
			n = (unsigned long) vp->val.i;
		else
			n = (vp->val.i < 0) ? -vp->val.i : vp->val.i;
		base = (vp->type == 0) ? 10 : vp->type;

		*--s = '\0';
		do {
			*--s = digits[n % base];
			n /= base;
		} while (n != 0);
		if (base != 10) {
			*--s = '#';
			*--s = digits[base % 10];
			if (base >= 10)
				*--s = digits[base / 10];
		}
		if (!(vp->flag & INT_U) && vp->val.i < 0)
			*--s = '-';
		if (vp->flag & (RJUST|LJUST)) /* case already dealt with */
			s = formatstr(vp, s);
	}
	return s;
}

/* get variable integer value, with error checking */
long
intval(vp)
	register struct tbl *vp;
{
	long num;
	int base;

	base = getint(vp, &num);
	if (base == -1)
		/* XXX check calls - is error here ok by POSIX? */
		errorf("%s: bad number", str_val(vp));
	return num;
}

/* set variable to string value */
void
setstr(vq, s)
	register struct tbl *vq;
	const char *s;
{
	if (!(vq->flag&INTEGER)) { /* string dest */
		if ((vq->flag&ALLOC)) {
			/* debugging */
			if (s >= vq->val.s
			    && s <= vq->val.s + strlen(vq->val.s))
				internal_errorf(TRUE,
				    "setstr: %s=%s: assigning to self",
				    vq->name, s);
			afree((void*)vq->val.s, vq->areap);
		}
		vq->flag &= ~(ISSET|ALLOC);
		vq->type = 0;
		if (s && (vq->flag & (UCASEV_AL|LCASEV|LJUST|RJUST)))
			s = formatstr(vq, s);
		if ((vq->flag&EXPORT))
			export(vq, s);
		else {
			vq->val.s = str_save(s, vq->areap);
			if (vq->val.s)		/* <sjg> don't lie */
				vq->flag |= ALLOC;
		}
	} else			/* integer dest */
		/* XXX is this correct? */
		v_evaluate(vq, s, FALSE);
	vq->flag |= ISSET;
	if ((vq->flag&SPECIAL))
		setspec(vq);
}

/* set variable to integer */
void
setint(vq, n)
	register struct tbl *vq;
	long n;
{
	if (!(vq->flag&INTEGER)) {
		register struct tbl *vp = &vtemp;
		vp->flag = (ISSET|INTEGER);
		vp->type = 0;
		vp->areap = ATEMP;
		vp->val.i = n;
		setstr(vq, str_val(vp));
	} else
		vq->val.i = n;
	vq->flag |= ISSET;
	if ((vq->flag&SPECIAL))
		setspec(vq);
}

int
getint(vp, nump)
	struct tbl *vp;
	long *nump;
{
	register char *s;
	register int c;
	int base, neg;
	int have_base = 0;
	long num;

	if (vp->flag&SPECIAL)
		getspec(vp);
	/* XXX is it possible for ISSET to be set and val.s to be 0? */
	if (!(vp->flag&ISSET) || (!(vp->flag&INTEGER) && vp->val.s == NULL))
		return -1;
	if (vp->flag&INTEGER) {
		*nump = vp->val.i;
		return vp->type;
	}
	s = vp->val.s + vp->type;
	if (s == NULL)	/* redundent given initial test */
		s = null;
	base = 10;
	num = 0;
	neg = 0;
	for (c = *s++; c ; c = *s++) {
		if (c == '-') {
			neg++;
		} else if (c == '#') {
			base = (int) num;
			if (have_base || base < 2 || base > 36)
				return -1;
			num = 0;
			have_base = 1;
		} else if (letnum(c)) {
			if (isdigit(c))
				c -= '0';
			else if (islower(c))
				c -= 'a' - 10; /* todo: assumes ascii */
			else if (isupper(c))
				c -= 'A' - 10; /* todo: assumes ascii */
			else
				c = -1; /* _: force error */
			if (c < 0 || c >= base)
				return -1;
			num = num * base + c;
		} else
			return -1;
	}
	if (neg)
		num = -num;
	*nump = num;
	return base;
}

/* convert variable vq to integer variable, setting its value from vp
 * (vq and vp may be the same)
 */
struct tbl *
setint_v(vq, vp)
	register struct tbl *vq, *vp;
{
	int base;
	long num;

	if ((base = getint(vp, &num)) == -1)
		return NULL;
	if (!(vq->flag & INTEGER) && (vq->flag & ALLOC)) {
		vq->flag &= ~ALLOC;
		afree(vq->val.s, vq->areap);
	}
	vq->val.i = num;
	if (vq->type == 0) /* default base */
		vq->type = base;
	vq->flag |= ISSET|INTEGER;
	if (vq->flag&SPECIAL)
		setspec(vq);
	return vq;
}

static char *
formatstr(vp, s)
	struct tbl *vp;
	const char *s;
{
	int olen, nlen;
	char *p, *q;

	olen = strlen(s);

	if (vp->flag & (RJUST|LJUST)) {
		if (!vp->u2.field)	/* default field width */
			vp->u2.field = olen;
		nlen = vp->u2.field;
	} else
		nlen = olen;

	p = (char *) alloc(nlen + 1, ATEMP);
	if (vp->flag & (RJUST|LJUST)) {
		int slen;

		if (vp->flag & RJUST) {
			const char *q = s + olen;
			/* strip trailing spaces (at&t ksh uses q[-1] == ' ') */
			while (q > s && isspace(q[-1]))
				--q;
			slen = q - s;
			if (slen > vp->u2.field) {
				s += slen - vp->u2.field;
				slen = vp->u2.field;
			}
			shf_snprintf(p, nlen + 1,
				((vp->flag & ZEROFIL) && digit(*s)) ?
					  "%0*s%.*s" : "%*s%.*s",
				vp->u2.field - slen, null, slen, s);
		} else {
			/* strip leading spaces/zeros */
			while (isspace(*s))
				s++;
			if (vp->flag & ZEROFIL)
				while (*s == '0')
					s++;
			shf_snprintf(p, nlen + 1, "%-*.*s",
				vp->u2.field, vp->u2.field, s);
		}
	} else
		memcpy(p, s, olen + 1);

	if (vp->flag & UCASEV_AL) {
		for (q = p; *q; q++)
			if (islower(*q))
				*q = toupper(*q);
	} else if (vp->flag & LCASEV) {
		for (q = p; *q; q++)
			if (isupper(*q))
				*q = tolower(*q);
	}

	return p;
}

/*
 * make vp->val.s be "name=value" for quick exporting.
 */
static void
export(vp, val)
	register struct tbl *vp;
	const char *val;
{
	register char *xp;
	char *op = (vp->flag&ALLOC) ? vp->val.s : NULL;
	int namelen = strlen(vp->name);
	int vallen = strlen(val) + 1;

	vp->flag |= ALLOC;
	xp = (char*)alloc(namelen + 1 + vallen, vp->areap);
	memcpy(vp->val.s = xp, vp->name, namelen);
	xp += namelen;
	*xp++ = '=';
	vp->type = xp - vp->val.s; /* offset to value */
	memcpy(xp, val, vallen);
	if (op != NULL)
		afree((void*)op, vp->areap);
}

/*
 * lookup variable (according to (set&LOCAL)),
 * set its attributes (INTEGER, RDONLY, EXPORT, TRACE, LJUST, RJUST, ZEROFIL,
 * LCASEV, UCASEV_AL), and optionally set its value if an assignment.
 */
struct tbl *
typeset(var, set, clr, field, base)
	register const char *var;
	Tflag clr, set;
	int field, base;
{
	register struct tbl *vp;
	struct tbl *vpbase, *t;
	char *tvar;
	const char *val;

	/* check for valid variable name, search for value */
	val = skip_varname(var, FALSE);
	if (val == var)
		return NULL;
	if (*val == '[') {
		int len;

		len = array_ref_len(val);
		if (len == 0)
			return NULL;
		/* IMPORT is only used when the shell starts up and is
		 * setting up its environment.  Allow only simple array
		 * references at this time since parameter/command substitution
		 * is preformed on the [expression], which would be a major
		 * security hole.
		 */
		if (set & IMPORT) {
			int i;
			for (i = 1; i < len - 1; i++)
				if (!digit(val[i]))
					return NULL;
		}
		val += len;
	}
	if (*val == '=')
		tvar = str_nsave(var, val++ - var, ATEMP);
	else {
		/* Importing from original envirnment: must have an = */
		if (set & IMPORT)
			return NULL;
		tvar = (char *) var;
		val = NULL;
	}

	/* Prevent typeset from creating a local PATH/ENV/SHELL */
	if (Flag(FRESTRICTED) && (strcmp(tvar, "PATH") == 0
				  || strcmp(tvar, "ENV") == 0
				  || strcmp(tvar, "SHELL") == 0))
		errorf("%s: restricted", tvar);

	vp = (set&LOCAL) ? local(tvar, (set & LOCAL_COPY) ? TRUE : FALSE)
		: global(tvar);
	set &= ~(LOCAL|LOCAL_COPY);

	vpbase = (vp->flag & ARRAY) ? global(arrayname(var)) : vp;

	/* only allow export flag to be set.  at&t ksh allows any attribute to
	 * be changed, which means it can be truncated or modified
	 * (-L/-R/-Z/-i).
	 */
	if ((vpbase->flag&RDONLY)
	    && (val || clr || (set & ~EXPORT)))
		/* XXX check calls - is error here ok by POSIX? */
		errorf("%s: is read only", tvar);
	if (val)
		afree(tvar, ATEMP);

	/* most calls are with set/clr == 0 */
	if (set | clr) {
		/* XXX if x[0] isn't set, there will be problems: need to have
		 * one copy of attributes for arrays...
		 */
		for (t = vpbase; t; t = t->u.array) {
			int fake_assign;
			char UNINITIALIZED(*s);
			char UNINITIALIZED(*free_me);

			fake_assign = (t->flag & ISSET) && (!val || t != vp)
				      && ((set & (UCASEV_AL|LCASEV|LJUST|RJUST|ZEROFIL))
					  || ((t->flag & INTEGER) && (clr & INTEGER))
					  || (!(t->flag & INTEGER) && (set & INTEGER)));
			if (fake_assign) {
				if (t->flag & INTEGER) {
					s = str_val(t);
					free_me = (char *) 0;
				} else {
					s = t->val.s + t->type;
					free_me = (t->flag & ALLOC) ? t->val.s
								  : (char *) 0;
				}
				t->flag &= ~ALLOC;
			}
			if (!(t->flag & INTEGER) && (set & INTEGER)) {
				t->type = 0;
				t->flag &= ~ALLOC;
			}
			t->flag = (t->flag | set) & ~clr;
			/* Don't change base if assignment is to be done,
			 * in case assignment fails.
			 */
			if ((set & INTEGER) && base > 0 && (!val || t != vp))
				t->type = base;
			if (set & (LJUST|RJUST|ZEROFIL))
				t->u2.field = field;
			if (fake_assign) {
				setstr(t, s);
				if (free_me)
					afree((void *) free_me, t->areap);
			}
		}
	}

	if (val != NULL) {
		if (vp->flag&INTEGER) {
			/* do not zero base before assignment */
			setstr(vp, val);
			/* Done after assignment to override default */
			if (base > 0)
				vp->type = base;
		} else
			setstr(vp, val);
	}

	/* only x[0] is ever exported, so use vpbase */
	if ((vpbase->flag&EXPORT) && !(vpbase->flag&INTEGER)
	    && vpbase->type == 0)
		export(vpbase, (vpbase->flag&ISSET) ? vpbase->val.s : null);

	return vp;
}

/* Unset a variable.  array_ref is set if there was an array reference in
 * the name lookup (eg, x[2]).
 */
void
unset(vp, array_ref)
	register struct tbl *vp;
	int array_ref;
{
	if (vp->flag & ALLOC)
		afree((void*)vp->val.s, vp->areap);
	if ((vp->flag & ARRAY) && !array_ref) {
		struct tbl *a, *tmp;

		/* Free up entire array */
		for (a = vp->u.array; a; ) {
			tmp = a;
			a = a->u.array;
			if (tmp->flag & ALLOC)
				afree((void *) tmp->val.s, tmp->areap);
			afree(tmp, tmp->areap);
		}
		vp->u.array = (struct tbl *) 0;
	}
	/* If foo[0] is being unset, the remainder of the array is kept... */
	vp->flag &= SPECIAL | (array_ref ? ARRAY|DEFINED : 0);
	if (vp->flag & SPECIAL)
		unsetspec(vp);	/* responsible for `unspecial'ing var */
}

/* return a pointer to the first char past a legal variable name (returns the
 * argument if there is no legal name, returns * a pointer to the terminating
 * null if whole string is legal).
 */
char *
skip_varname(s, aok)
	const char *s;
	int aok;
{
	int alen;

	if (s && letter(*s)) {
		while (*++s && letnum(*s))
			;
		if (aok && *s == '[' && (alen = array_ref_len(s)))
			s += alen;
	}
	return (char *) s;
}

/* Return a pointer to the first character past any legal variable name.  */
char *
skip_wdvarname(s, aok)
	const char *s;
	int aok;	/* skip array de-reference? */
{
	if (s[0] == CHAR && letter(s[1])) {
		do
			s += 2;
		while (s[0] == CHAR && letnum(s[1]));
		if (aok && s[0] == CHAR && s[1] == '[') {
			/* skip possible array de-reference */
			const char *p = s;
			char c;
			int depth = 0;

			while (1) {
				if (p[0] != CHAR)
					break;
				c = p[1];
				p += 2;
				if (c == '[')
					depth++;
				else if (c == ']' && --depth == 0) {
					s = p;
					break;
				}
			}
		}
	}
	return (char *) s;
}

/* Check if coded string s is a variable name */
int
is_wdvarname(s, aok)
	const char *s;
	int aok;
{
	char *p = skip_wdvarname(s, aok);

	return p != s && p[0] == EOS;
}

/* Check if coded string s is a variable assignment */
int
is_wdvarassign(s)
	const char *s;
{
	char *p = skip_wdvarname(s, TRUE);

	return p != s && p[0] == CHAR && p[1] == '=';
}

/*
 * Make the exported environment from the exported names in the dictionary.
 */
char **
makenv()
{
	struct block *l = e->loc;
	XPtrV env;
	register struct tbl *vp, **vpp;
	register int i;

	XPinit(env, 64);
	for (l = e->loc; l != NULL; l = l->next)
		for (vpp = l->vars.tbls, i = l->vars.size; --i >= 0; )
			if ((vp = *vpp++) != NULL
			    && (vp->flag&(ISSET|EXPORT)) == (ISSET|EXPORT)) {
				register struct block *l2;
				register struct tbl *vp2;
				unsigned h = hash(vp->name);

				/* unexport any redefined instances */
				for (l2 = l->next; l2 != NULL; l2 = l2->next) {
					vp2 = tsearch(&l2->vars, vp->name, h);
					if (vp2 != NULL)
						vp2->flag &= ~EXPORT;
				}
				if ((vp->flag&INTEGER)) {
					/* integer to string */
					char *val;
					val = str_val(vp);
					vp->flag &= ~INTEGER;
					setstr(vp, val);
				}
				XPput(env, vp->val.s);
			}
	XPput(env, NULL);
	return (char **) XPclose(env);
}

/*
 * Called after a fork in parent to bump the random number generator.
 * Done to ensure children will not get the same random number sequence
 * if the parent doesn't use $RANDOM.
 */
void
change_random()
{
    rand();
}

/*
 * handle special variables with side effects - PATH, SECONDS.
 */

/* Test if name is a special parameter */
static int
special(name)
	register const char * name;
{
	register struct tbl *tp;

	tp = tsearch(&specials, name, hash(name));
	return tp ? tp->type : V_NONE;
}

#ifdef KSH
static	time_t	seconds;		/* time SECONDS last set */
#endif /* KSH */

static void
getspec(vp)
	register struct tbl *vp;
{
	switch (special(vp->name)) {
#ifdef KSH
	  case V_SECONDS:
		vp->flag &= ~SPECIAL;
		setint(vp, (long) (time((time_t *)0) - seconds));
		vp->flag |= SPECIAL;
		break;
	  case V_RANDOM:
		vp->flag &= ~SPECIAL;
		setint(vp, (long) (rand() & 0x7fff));
		vp->flag |= SPECIAL;
		break;
#endif /* KSH */
#ifdef HISTORY
	  case V_HISTSIZE:
		vp->flag &= ~SPECIAL;
		setint(vp, (long) histsize);
		vp->flag |= SPECIAL;
		break;
#endif /* HISTORY */
	  case V_OPTIND:
		vp->flag &= ~SPECIAL;
		setint(vp, (long) user_opt.uoptind);
		vp->flag |= SPECIAL;
		break;
	}
}

static void
setspec(vp)
	register struct tbl *vp;
{
	char *s;

	switch (special(vp->name)) {
	  case V_PATH:
		if (path)
			afree(path, APERM);
		path = str_save(str_val(vp), APERM);
		flushcom(1);	/* clear tracked aliases */
		break;
	  case V_IFS:
		setctypes(s = str_val(vp), C_IFS);
		ifs0 = *s;
		break;
	  case V_OPTIND:
		vp->flag &= ~SPECIAL;
		getopts_reset((int) intval(vp));
		vp->flag |= SPECIAL;
		break;
	  case V_POSIXLY_CORRECT:
		change_flag(FPOSIX, OF_SPECIAL, 1);
		break;
	  case V_TMPDIR:
		if (tmpdir) {
			afree(tmpdir, APERM);
			tmpdir = (char *) 0;
		}
		/* Use tmpdir iff it is an absolute path, is writable and
		 * searchable and is a directory...
		 */
		{
			struct stat statb;
			s = str_val(vp);
			if (ISABSPATH(s) && eaccess(s, W_OK|X_OK) == 0
			    && stat(s, &statb) == 0 && S_ISDIR(statb.st_mode))
				tmpdir = str_save(s, APERM);
		}
		break;
#ifdef HISTORY
	  case V_HISTSIZE:
		vp->flag &= ~SPECIAL;
		sethistsize((int) intval(vp));
		vp->flag |= SPECIAL;
		break;
	  case V_HISTFILE:
		sethistfile(str_val(vp));
		break;
#endif /* HISTORY */
#ifdef EDIT
	  case V_VISUAL:
		set_editmode(str_val(vp));
		break;
	  case V_EDITOR:
		if (!(global("VISUAL")->flag & ISSET))
			set_editmode(str_val(vp));
		break;
	  case V_COLUMNS:
		if ((x_cols = intval(vp)) <= MIN_COLS)
			x_cols = MIN_COLS;
		break;
#endif /* EDIT */
#ifdef KSH
	  case V_MAIL:
		mbset(str_val(vp));
		break;
	  case V_MAILPATH:
		mpset(str_val(vp));
		break;
	  case V_MAILCHECK:
		/* mail_check_set(intval(vp)); */
		break;
	  case V_RANDOM:
		vp->flag &= ~SPECIAL;
		srand((unsigned int)intval(vp));
		vp->flag |= SPECIAL;
		break;
	  case V_SECONDS:
		vp->flag &= ~SPECIAL;
		seconds = time((time_t*) 0) - intval(vp);
		vp->flag |= SPECIAL;
		break;
	  case V_TMOUT:
		/* at&t ksh seems to do this (only listen if integer) */
		if (vp->flag & INTEGER)
			ksh_tmout = vp->val.i >= 0 ? vp->val.i : 0;
		break;
#endif /* KSH */
	}
}

static void
unsetspec(vp)
	register struct tbl *vp;
{
	switch (special(vp->name)) {
	  case V_PATH:
		if (path)
			afree(path, APERM);
		path = str_save(def_path, APERM);
		flushcom(1);	/* clear tracked aliases */
		break;
	  case V_IFS:
		setctypes(" \t\n", C_IFS);
		ifs0 = ' ';
		break;
	  case V_TMPDIR:
		/* should not become unspecial */
		if (tmpdir) {
			afree(tmpdir, APERM);
			tmpdir = (char *) 0;
		}
		break;
#ifdef KSH
	  case V_MAIL:
		mbset((char *) 0);
		break;
	  case V_MAILPATH:
		mpset((char *) 0);
		break;
	  case V_TMOUT:
		/* at&t ksh doesn't do this. TMOUT becomes unspecial so
		 * future assignments don't have effect.  Could be
		 * useful (eg, after "TMOUT=60; unset TMOUT", user
		 * can't get rid of the timeout...).  Should be handled
		 * by generic unset code...
		 */
		ksh_tmout = 0;
		break;
#endif /* KSH */
	  /* todo: generic action for specials (at&t says variables
	   * lose their special meaning when unset but global() checks
	   * the name of new vars to see if they are special)
	   * 	lose meaning: _, ERRNO, LINENO, MAILCHECK,
	   *		OPTARG, OPTIND, RANDOM, SECONDS, TMOUT.
	   *	unknown: MAIL, MAILPATH, HISTSIZE, HISTFILE,
	   *    no effect: IFS, COLUMNS, PATH, TMPDIR,
	   *		VISUAL, EDITOR,
	   *		POSIXLY_CORRECT (use set +o posix instead)
	   */
	}
}

/*
 * Search for (and possibly create) a table entry starting with
 * vp, indexed by val.
 */
static struct tbl *
arraysearch(vp, val)
	struct tbl *vp;
	int val;
{
	struct tbl *prev, *curr, *new;

	vp->flag |= ARRAY|DEFINED;

	/* The table entry is always [0] */
	if (val == 0) {
		vp->index = 0;
		return vp;
	}
	prev = vp;
	curr = vp->u.array;
	while (curr && curr->index < val) {
		prev = curr;
		curr = curr->u.array;
	}
	if (curr && curr->index == val) {
		if (curr->flag&ISSET)
			return curr;
		else
			new = curr;
	} else
		new = (struct tbl *)alloc(sizeof(struct tbl)+strlen(vp->name)+1, vp->areap);
	strcpy(new->name, vp->name);
	new->flag = vp->flag & ~(ALLOC|DEFINED|ISSET|SPECIAL);
	new->type = vp->type;
	new->areap = vp->areap;
	new->u2.field = vp->u2.field;
	new->index = val;
	if (curr != new) {		/* not reusing old array entry */
		prev->u.array = new;
		new->u.array = curr;
	}
	return new;
}

/* Return the length of an array reference (eg, [1+2]) - cp is assumed
 * to point to the open bracket.  Returns 0 if there is no matching closing
 * bracket.
 */
int
array_ref_len(cp)
	const char *cp;
{
	const char *s = cp;
	int c;
	int depth = 0;

	while ((c = *s++) && (c != ']' || --depth))
		if (c == '[')
			depth++;
	if (!c)
		return 0;
	return s - cp;
}

/*
 * Make a copy of the base of an array name
 */
char *
arrayname(str)
	const char *str;
{
	const char *p;

	if ((p = strchr(str, '[')) == 0)
		/* Shouldn't happen, but why worry? */
		return (char *) str;

	return str_nsave(str, p - str, ATEMP);
}

/* Set (or overwrite, if !reset) the array variable var to the values in vals.
 */
void
set_array(var, reset, vals)
	const char *var;
	int reset;
	char **vals;
{
	struct tbl *vp, *vq;
	int i;

	/* to get local array, use "typeset foo; set -A foo" */
	vp = global(var);

	/* Note: at&t ksh allows set -A but not set +A of a read-only var */
	if ((vp->flag&RDONLY))
		errorf("%s: is read only", var);
	/* This code is quite non-optimal */
	if (reset > 0)
		/* trash existing values and attributes */
		unset(vp, 0);
	/* todo: would be nice for assignment to completely succeed or
	 * completely fail.  Only really effects integer arrays:
	 * evaluation of some of vals[] may fail...
	 */
	for (i = 0; vals[i]; i++) {
		vq = arraysearch(vp, i);
		setstr(vq, vals[i]);
	}
}
