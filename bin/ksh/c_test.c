/*	$OpenBSD: c_test.c,v 1.4 1997/06/19 13:58:38 kstailey Exp $	*/

/*
 * test(1); version 7-like  --  author Erik Baalbergen
 * modified by Eric Gisin to be used as built-in.
 * modified by Arnold Robbins to add SVR3 compatibility
 * (-x -c -b -p -u -g -k) plus Korn's -L -nt -ot -ef and new -S (socket).
 * modified by Michael Rendell to add Korn's [[ .. ]] expressions.
 * modified by J.T. Conklin to add POSIX compatibility.
 */

#include "sh.h"
#include "ksh_stat.h"
#include "c_test.h"

/* test(1) accepts the following grammar:
	oexpr	::= aexpr | aexpr "-o" oexpr ;
	aexpr	::= nexpr | nexpr "-a" aexpr ;
	nexpr	::= primary | "!" nexpr ;
	primary	::= unary-operator operand
		| operand binary-operator operand
		| operand
		| "(" oexpr ")"
		;

	unary-operator ::= "-a"|"-r"|"-w"|"-x"|"-e"|"-f"|"-d"|"-c"|"-b"|"-p"|
			   "-u"|"-g"|"-k"|"-s"|"-t"|"-z"|"-n"|"-o"|"-O"|"-G"|
			   "-L"|"-h"|"-S"|"-H";

	binary-operator ::= "="|"=="|"!="|"-eq"|"-ne"|"-ge"|"-gt"|"-le"|"-lt"|
			    "-nt"|"-ot"|"-ef"|
			    "<"|">"	# rules used for [[ .. ]] expressions
			    ;
	operand ::= <any thing>
*/

#define T_ERR_EXIT	2	/* POSIX says > 1 for errors */

struct t_op {
	char	op_text[4];
	Test_op	op_num;
};
static const struct t_op u_ops [] = {
	{"-a",	TO_FILAXST },
	{"-b",	TO_FILBDEV },
	{"-c",	TO_FILCDEV },
	{"-d",	TO_FILID },
	{"-e",	TO_FILEXST },
	{"-f",	TO_FILREG },
	{"-G",	TO_FILGID },
	{"-g",	TO_FILSETG },
	{"-h",	TO_FILSYM },
	{"-H",	TO_FILCDF },
	{"-k",	TO_FILSTCK },
	{"-L",	TO_FILSYM },
	{"-n",	TO_STNZE },
	{"-O",	TO_FILUID },
	{"-o",	TO_OPTION },
	{"-p",	TO_FILFIFO },
	{"-r",	TO_FILRD },
	{"-s",	TO_FILGZ },
	{"-S",	TO_FILSOCK },
	{"-t",	TO_FILTT },
	{"-u",	TO_FILSETU },
	{"-w",	TO_FILWR },
	{"-x",	TO_FILEX },
	{"-z",	TO_STZER },
	{"",	TO_NONOP }
    };
static const struct t_op b_ops [] = {
	{"=",	TO_STEQL },
#ifdef KSH
	{"==",	TO_STEQL },
#endif /* KSH */
	{"!=",	TO_STNEQ },
	{"<",	TO_STLT },
	{">",	TO_STGT },
	{"-eq",	TO_INTEQ },
	{"-ne",	TO_INTNE },
	{"-gt",	TO_INTGT },
	{"-ge",	TO_INTGE },
	{"-lt",	TO_INTLT },
	{"-le",	TO_INTLE },
	{"-ef",	TO_FILEQ },
	{"-nt",	TO_FILNT },
	{"-ot",	TO_FILOT },
	{"",	TO_NONOP }
    };

static int	test_stat ARGS((const char *path, struct stat *statb));
static int	test_eaccess ARGS((const char *path, int mode));
static int	test_oexpr ARGS((Test_env *te, int do_eval));
static int	test_aexpr ARGS((Test_env *te, int do_eval));
static int	test_nexpr ARGS((Test_env *te, int do_eval));
static int	test_primary ARGS((Test_env *te, int do_eval));
static int	ptest_isa ARGS((Test_env *te, Test_meta meta));
static const char *ptest_getopnd ARGS((Test_env *te, Test_op op, int do_eval));
static int	ptest_eval ARGS((Test_env *te, Test_op op, const char *opnd1,
				const char *opnd2, int do_eval));
static void	ptest_error ARGS((Test_env *te, int offset, const char *msg));

int
c_test(wp)
	char **wp;
{
	int argc;
	int res;
	Test_env te;

	te.flags = 0;
	te.isa = ptest_isa;
	te.getopnd = ptest_getopnd;
	te.eval = ptest_eval;
	te.error = ptest_error;

	for (argc = 0; wp[argc]; argc++)
		;

	if (strcmp(wp[0], "[") == 0) {
		if (strcmp(wp[--argc], "]") != 0) {
			bi_errorf("missing ]");
			return T_ERR_EXIT;
		}
	}

	te.pos.wp = wp + 1;
	te.wp_end = wp + argc;

	/* 
	 * Handle the special cases from POSIX.2, section 4.62.4.
	 * Implementation of all the rules isn't necessary since 
	 * our parser does the right thing for the ommited steps.
	 */
	if (argc <= 5) {
		char **owp = wp;
		int invert = 0;
		Test_op	op;
		const char *opnd1, *opnd2;

		while (--argc >= 0) {
			if ((*te.isa)(&te, TM_END))
				return !0;
			if (argc == 3) {
				opnd1 = (*te.getopnd)(&te, TO_NONOP, 1);
				if ((op = (Test_op) (*te.isa)(&te, TM_BINOP))) {
					opnd2 = (*te.getopnd)(&te, op, 1);
					res = (*te.eval)(&te, op, opnd1, opnd2,
							1);
					if (te.flags & TEF_ERROR)
						return T_ERR_EXIT;
					if (invert & 1)
						res = !res;
					return !res;
				}
				/* back up to opnd1 */
				te.pos.wp--;
			}
			if (argc == 1) {
				opnd1 = (*te.getopnd)(&te, TO_NONOP, 1);
				res = (*te.eval)(&te, TO_STNZE, opnd1,
						(char *) 0, 1);
				if (invert & 1)
					res = !res;
				return !res;
			}
			if ((*te.isa)(&te, TM_NOT)) {
				invert++;
			} else
				break;
		}
		te.pos.wp = owp + 1;
	}

	return test_parse(&te);
}

/*
 * Generic test routines.
 */

Test_op
test_isop(te, meta, s)
	Test_env *te;
	Test_meta meta;
	const char *s;
{
	char sc1;
	const struct t_op *otab;

	otab = meta == TM_UNOP ? u_ops : b_ops;
	if (*s) {
		sc1 = s[1];
		for (; otab->op_text[0]; otab++)
			if (sc1 == otab->op_text[1]
			    && strcmp(s, otab->op_text) == 0
			    && ((te->flags & TEF_DBRACKET)
				|| (otab->op_num != TO_STLT
				    && otab->op_num != TO_STGT)))
				return otab->op_num;
	}
	return TO_NONOP;
}

int
test_eval(te, op, opnd1, opnd2, do_eval)
	Test_env *te;
	Test_op op;
	const char *opnd1;
	const char *opnd2;
	int do_eval;
{
	int res;
	int not;
	struct stat b1, b2;

	if (!do_eval)
		return 0;

	switch ((int) op) {
	/*
	 * Unary Operators
	 */
	  case TO_STNZE: /* -n */
		return *opnd1 != '\0';
	  case TO_STZER: /* -z */
		return *opnd1 == '\0';
	  case TO_OPTION: /* -o */
		if ((not = *opnd1 == '!'))
			opnd1++;
		if ((res = option(opnd1)) < 0)
			res = 0;
		else {
			res = Flag(res);
			if (not)
				res = !res;
		}
		return res; 
	  case TO_FILRD: /* -r */
		return test_eaccess(opnd1, R_OK) == 0;
	  case TO_FILWR: /* -w */
		return test_eaccess(opnd1, W_OK) == 0;
	  case TO_FILEX: /* -x */
		return test_eaccess(opnd1, X_OK) == 0;
	  case TO_FILAXST: /* -a */
		return test_stat(opnd1, &b1) == 0;
	  case TO_FILEXST: /* -e */
		/* at&t ksh does not appear to do the /dev/fd/ thing for
		 * this (unless the os itself handles it)
		 */
		return stat(opnd1, &b1) == 0;
	  case TO_FILREG: /* -r */
		return test_stat(opnd1, &b1) == 0 && S_ISREG(b1.st_mode);
	  case TO_FILID: /* -d */
		return test_stat(opnd1, &b1) == 0 && S_ISDIR(b1.st_mode);
	  case TO_FILCDEV: /* -c */
#ifdef S_ISCHR
		return test_stat(opnd1, &b1) == 0 && S_ISCHR(b1.st_mode);
#else
		return 0;
#endif
	  case TO_FILBDEV: /* -b */
#ifdef S_ISBLK
		return test_stat(opnd1, &b1) == 0 && S_ISBLK(b1.st_mode);
#else
		return 0;
#endif
	  case TO_FILFIFO: /* -p */
#ifdef S_ISFIFO
		return test_stat(opnd1, &b1) == 0 && S_ISFIFO(b1.st_mode);
#else
		return 0;
#endif
	  case TO_FILSYM: /* -h -L */
#ifdef S_ISLNK
		return lstat(opnd1, &b1) == 0 && S_ISLNK(b1.st_mode);
#else
		return 0;
#endif
	  case TO_FILSOCK: /* -S */
#ifdef S_ISSOCK
		return test_stat(opnd1, &b1) == 0 && S_ISSOCK(b1.st_mode);
#else
		return 0;
#endif
	  case TO_FILCDF:/* -H HP context dependent files (directories) */
#ifdef S_ISCDF
	  {
		/* Append a + to filename and check to see if result is a
		 * setuid directory.  CDF stuff in general is hookey, since
		 * it breaks for the following sequence: echo hi > foo+;
		 * mkdir foo; echo bye > foo/default; chmod u+s foo
		 * (foo+ refers to the file with hi in it, there is no way
		 * to get at the file with bye in it - please correct me if
		 * I'm wrong about this).
		 */
		int len = strlen(opnd1);
		char *p = str_nsave(opnd1, len + 1, ATEMP);

		p[len++] = '+';
		p[len] = '\0';
		return stat(p, &b1) == 0 && S_ISCDF(b1.st_mode);
	  }
#else
		return 0;
#endif
	  case TO_FILSETU: /* -u */
#ifdef S_ISUID
		return test_stat(opnd1, &b1) == 0
			&& (b1.st_mode & S_ISUID) == S_ISUID;
#else
		return 0;
#endif
	  case TO_FILSETG: /* -g */
#ifdef S_ISGID
		return test_stat(opnd1, &b1) == 0
			&& (b1.st_mode & S_ISGID) == S_ISGID;
#else
		return 0;
#endif
	  case TO_FILSTCK: /* -k */
		return test_stat(opnd1, &b1) == 0
			&& (b1.st_mode & S_ISVTX) == S_ISVTX;
	  case TO_FILGZ: /* -s */
		return test_stat(opnd1, &b1) == 0 && b1.st_size > 0L;
	  case TO_FILTT: /* -t */
		if (opnd1 && !bi_getn(opnd1, &res)) {
			te->flags |= TEF_ERROR;
			res = 0;
		} else
			res = isatty(opnd1 ? res : 0);
		return res;
	  case TO_FILUID: /* -O */
		return test_stat(opnd1, &b1) == 0 && b1.st_uid == geteuid();
	  case TO_FILGID: /* -G */
		return test_stat(opnd1, &b1) == 0 && b1.st_gid == getegid();
	/*
	 * Binary Operators
	 */
	  case TO_STEQL: /* = */
		if (te->flags & TEF_DBRACKET)
			return gmatch(opnd1, opnd2, FALSE);
		return strcmp(opnd1, opnd2) == 0;
	  case TO_STNEQ: /* != */
		if (te->flags & TEF_DBRACKET)
			return !gmatch(opnd1, opnd2, FALSE);
		return strcmp(opnd1, opnd2) != 0;
	  case TO_STLT: /* < */
		return strcmp(opnd1, opnd2) < 0;
	  case TO_STGT: /* > */
		return strcmp(opnd1, opnd2) > 0;
	  case TO_INTEQ: /* -eq */
	  case TO_INTNE: /* -ne */
	  case TO_INTGE: /* -ge */
	  case TO_INTGT: /* -gt */
	  case TO_INTLE: /* -le */
	  case TO_INTLT: /* -lt */
		{
			long v1, v2;

			if (!evaluate(opnd1, &v1, TRUE)
			    || !evaluate(opnd2, &v2, TRUE))
			{
				/* error already printed.. */
				te->flags |= TEF_ERROR;
				return 1;
			}
			switch ((int) op) {
			  case TO_INTEQ:
				return v1 == v2;
			  case TO_INTNE:
				return v1 != v2;
			  case TO_INTGE:
				return v1 >= v2;
			  case TO_INTGT:
				return v1 > v2;
			  case TO_INTLE:
				return v1 <= v2;
			  case TO_INTLT:
				return v1 < v2;
			}
		}
	  case TO_FILNT: /* -nt */
		return stat (opnd1, &b1) == 0 && stat (opnd2, &b2) == 0
		       && b1.st_mtime > b2.st_mtime;
	  case TO_FILOT: /* -ot */
		return stat (opnd1, &b1) == 0 && stat (opnd2, &b2) == 0
		       && b1.st_mtime < b2.st_mtime;
	  case TO_FILEQ: /* -ef */
		return stat (opnd1, &b1) == 0 && stat (opnd2, &b2) == 0
		       && b1.st_dev == b2.st_dev
		       && b1.st_ino == b2.st_ino;
	}
	(*te->error)(te, 0, "internal error: unknown op");
	return 1;
}

/* Nasty kludge to handle Korn's bizarre /dev/fd hack */
static int
test_stat(path, statb)
	const char *path;
	struct stat *statb;
{
#if !defined(HAVE_DEV_FD)
	int fd;

	if (strncmp(path, "/dev/fd/", 8) == 0 && getn(path + 8, &fd))
		return fstat(fd, statb);
#endif /* !HAVE_DEV_FD */

	return stat(path, statb);
}

/* Another nasty kludge to handle Korn's bizarre /dev/fd hack */
static int
test_eaccess(path, mode)
	const char *path;
	int mode;
{
#if !defined(HAVE_DEV_FD)
	int fd;

	if (strncmp(path, "/dev/fd/", 8) == 0 && getn(path + 8, &fd)) {
		int flags;

		if ((flags = fcntl(fd, F_GETFL, 0)) < 0
		    || (mode & X_OK)
		    || ((mode & W_OK) && (flags & O_ACCMODE) == O_RDONLY)
		    || ((mode & R_OK) && (flags & O_ACCMODE) == O_WRONLY))
			return -1;
		return 0;
	}
#endif /* !HAVE_DEV_FD */

	return eaccess(path, mode);
}

int
test_parse(te)
	Test_env *te;
{
	int res;

	res = test_oexpr(te, 1);

	if (!(te->flags & TEF_ERROR) && !(*te->isa)(te, TM_END))
		(*te->error)(te, 0, "unexpected operator/operand");

	return (te->flags & TEF_ERROR) ? T_ERR_EXIT : !res;
}

static int
test_oexpr(te, do_eval)
	Test_env *te;
	int do_eval;
{
	int res;

	res = test_aexpr(te, do_eval);
	if (res)
		do_eval = 0;
	if (!(te->flags & TEF_ERROR) && (*te->isa)(te, TM_OR))
		return test_oexpr(te, do_eval) || res;
	return res;
}

static int
test_aexpr(te, do_eval)
	Test_env *te;
	int do_eval;
{
	int res;

	res = test_nexpr(te, do_eval);
	if (!res)
		do_eval = 0;
	if (!(te->flags & TEF_ERROR) && (*te->isa)(te, TM_AND))
		return test_aexpr(te, do_eval) && res;
	return res;
}

static int
test_nexpr(te, do_eval)
	Test_env *te;
	int do_eval;
{
	if (!(te->flags & TEF_ERROR) && (*te->isa)(te, TM_NOT))
		return !test_nexpr(te, do_eval);
	return test_primary(te, do_eval);
}

static int
test_primary(te, do_eval)
	Test_env *te;
	int do_eval;
{
	const char *opnd1, *opnd2;
	int res;
	Test_op op;

	if (te->flags & TEF_ERROR)
		return 0;
	if ((*te->isa)(te, TM_OPAREN)) {
		res = test_oexpr(te, do_eval);
		if (te->flags & TEF_ERROR)
			return 0;
		if (!(*te->isa)(te, TM_CPAREN)) {
			(*te->error)(te, 0, "missing closing paren");
			return 0;
		}
		return res;
	}
	if ((op = (Test_op) (*te->isa)(te, TM_UNOP))) {
		/* unary expression */
		opnd1 = (*te->getopnd)(te, op, do_eval);
		if (!opnd1) {
			(*te->error)(te, -1, "missing argument");
			return 0;
		}

		return (*te->eval)(te, op, opnd1, (const char *) 0, do_eval);
	}
	opnd1 = (*te->getopnd)(te, TO_NONOP, do_eval);
	if (!opnd1) {
		(*te->error)(te, 0, "expression expected");
		return 0;
	}
	if ((op = (Test_op) (*te->isa)(te, TM_BINOP))) {
		/* binary expression */
		opnd2 = (*te->getopnd)(te, op, do_eval);
		if (!opnd2) {
			(*te->error)(te, -1, "missing second argument");
			return 0;
		}

		return (*te->eval)(te, op, opnd1, opnd2, do_eval);
	}
	if (te->flags & TEF_DBRACKET) {
		(*te->error)(te, -1, "missing expression operator");
		return 0;
	}
	return (*te->eval)(te, TO_STNZE, opnd1, (const char *) 0, do_eval);
}

/*
 * Plain test (test and [ .. ]) specific routines.
 */

/* Test if the current token is a whatever.  Accepts the current token if
 * it is.  Returns 0 if it is not, non-zero if it is (in the case of
 * TM_UNOP and TM_BINOP, the returned value is a Test_op).
 */
static int
ptest_isa(te, meta)
	Test_env *te;
	Test_meta meta;
{
	/* Order important - indexed by Test_meta values */
	static const char *const tokens[] = {
				"-o", "-a", "!", "(", ")"
			};
	int ret;

	if (te->pos.wp >= te->wp_end)
		return meta == TM_END;

	if (meta == TM_UNOP || meta == TM_BINOP)
		ret = (int) test_isop(te, meta, *te->pos.wp);
	else if (meta == TM_END)
		ret = 0;
	else
		ret = strcmp(*te->pos.wp, tokens[(int) meta]) == 0;

	/* Accept the token? */
	if (ret)
		te->pos.wp++;

	return ret;
}

static const char *
ptest_getopnd(te, op, do_eval)
	Test_env *te;
	Test_op op;
	int do_eval;
{
	if (te->pos.wp >= te->wp_end)
		return op == TO_FILTT ? "1" : (const char *) 0;
	return *te->pos.wp++;
}

static int
ptest_eval(te, op, opnd1, opnd2, do_eval)
	Test_env *te;
	Test_op op;
	const char *opnd1;
	const char *opnd2;
	int do_eval;
{
	return test_eval(te, op, opnd1, opnd2, do_eval);
}

static void
ptest_error(te, offset, msg)
	Test_env *te;
	int offset;
	const char *msg;
{
	const char *op = te->pos.wp + offset >= te->wp_end ?
				(const char *) 0 : te->pos.wp[offset];

	te->flags |= TEF_ERROR;
	if (op)
		bi_errorf("%s: %s", op, msg);
	else
		bi_errorf("%s", msg);
}
