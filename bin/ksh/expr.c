/*	$OpenBSD: expr.c,v 1.7 1999/06/15 01:18:34 millert Exp $	*/

/*
 * Korn expression evaluation
 */
/*
 * todo: better error handling: if in builtin, should be builtin error, etc.
 */

#include "sh.h"
#include <ctype.h>


/* The order of these enums is constrained by the order of opinfo[] */
enum token {
	/* some (long) unary operators */
	O_PLUSPLUS = 0, O_MINUSMINUS,
	/* binary operators */
	O_EQ, O_NE,
	/* assignments are assumed to be in range O_ASN .. O_BORASN */
	O_ASN, O_TIMESASN, O_DIVASN, O_MODASN, O_PLUSASN, O_MINUSASN,
	       O_LSHIFTASN, O_RSHIFTASN, O_BANDASN, O_BXORASN, O_BORASN,
	O_LSHIFT, O_RSHIFT,
	O_LE, O_GE, O_LT, O_GT,
	O_LAND,
	O_LOR,
	O_TIMES, O_DIV, O_MOD,
	O_PLUS, O_MINUS,
	O_BAND,
	O_BXOR,
	O_BOR,
	O_TERN,
	O_COMMA,
	/* things after this aren't used as binary operators */
	/* unary that are not also binaries */
	O_BNOT, O_LNOT,
	/* misc */
	OPEN_PAREN, CLOSE_PAREN, CTERN,
	/* things that don't appear in the opinfo[] table */
	VAR, LIT, END, BAD
    };
#define IS_BINOP(op) (((int)op) >= (int)O_EQ && ((int)op) <= (int)O_COMMA)
#define IS_ASSIGNOP(op)	((int)(op) >= (int)O_ASN && (int)(op) <= (int)O_BORASN)

enum prec {
	P_PRIMARY = 0,		/* VAR, LIT, (), ~ ! - + */
	P_MULT,			/* * / % */
	P_ADD,			/* + - */
	P_SHIFT,		/* << >> */
	P_RELATION,		/* < <= > >= */
	P_EQUALITY,		/* == != */
	P_BAND,			/* & */
	P_BXOR,			/* ^ */
	P_BOR,			/* | */
	P_LAND,			/* && */
	P_LOR,			/* || */
	P_TERN,			/* ?: */
	P_ASSIGN,		/* = *= /= %= += -= <<= >>= &= ^= |= */
	P_COMMA			/* , */
    };
#define MAX_PREC	P_COMMA

struct opinfo {
	char		name[4];
	int		len;	/* name length */
	enum prec	prec;	/* precidence: lower is higher */
};

/* Tokens in this table must be ordered so the longest are first
 * (eg, += before +).  If you change something, change the order
 * of enum token too.
 */
static const struct opinfo opinfo[] = {
		{ "++",	 2, P_PRIMARY },	/* before + */
		{ "--",	 2, P_PRIMARY },	/* before - */
		{ "==",	 2, P_EQUALITY },	/* before = */
		{ "!=",	 2, P_EQUALITY },	/* before ! */
		{ "=",	 1, P_ASSIGN },		/* keep assigns in a block */
		{ "*=",	 2, P_ASSIGN },
		{ "/=",	 2, P_ASSIGN },
		{ "%=",	 2, P_ASSIGN },
		{ "+=",	 2, P_ASSIGN },
		{ "-=",	 2, P_ASSIGN },
		{ "<<=", 3, P_ASSIGN },
		{ ">>=", 3, P_ASSIGN },
		{ "&=",	 2, P_ASSIGN },
		{ "^=",	 2, P_ASSIGN },
		{ "|=",	 2, P_ASSIGN },
		{ "<<",	 2, P_SHIFT },
		{ ">>",	 2, P_SHIFT },
		{ "<=",	 2, P_RELATION },
		{ ">=",	 2, P_RELATION },
		{ "<",	 1, P_RELATION },
		{ ">",	 1, P_RELATION },
		{ "&&",	 2, P_LAND },
		{ "||",	 2, P_LOR },
		{ "*",	 1, P_MULT },
		{ "/",	 1, P_MULT },
		{ "%",	 1, P_MULT },
		{ "+",	 1, P_ADD },
		{ "-",	 1, P_ADD },
		{ "&",	 1, P_BAND },
		{ "^",	 1, P_BXOR },
		{ "|",	 1, P_BOR },
		{ "?",	 1, P_TERN },
		{ ",",	 1, P_COMMA },
		{ "~",	 1, P_PRIMARY },
		{ "!",	 1, P_PRIMARY },
		{ "(",	 1, P_PRIMARY },
		{ ")",	 1, P_PRIMARY },
		{ ":",	 1, P_PRIMARY },
		{ "",	 0, P_PRIMARY } /* end of table */
	    };


typedef struct expr_state Expr_state;
struct expr_state {
	const char *expression;		/* expression being evaluated */
	const char *tokp;		/* lexical position */
	enum token  tok;		/* token from token() */
	int	    noassign;		/* don't do assigns (for ?:,&&,||) */
	struct tbl *val;		/* value from token() */
	struct tbl *evaling;		/* variable that is being recursively
					 * expanded (EXPRINEVAL flag set)
					 */
};

enum error_type { ET_UNEXPECTED, ET_BADLIT, ET_RECURSIVE,
		  ET_LVALUE, ET_RDONLY, ET_STR };

static void        evalerr  ARGS((Expr_state *es, enum error_type type,
				  const char *str)) GCC_FUNC_ATTR(noreturn);
static struct tbl *evalexpr ARGS((Expr_state *es, enum prec prec));
static void        token    ARGS((Expr_state *es));
static struct tbl *do_ppmm  ARGS((Expr_state *es, enum token op,
				  struct tbl *vasn, bool_t is_prefix));
static void	   assign_check ARGS((Expr_state *es, enum token op,
				      struct tbl *vasn));
static struct tbl *tempvar  ARGS((void));
static struct tbl *intvar   ARGS((Expr_state *es, struct tbl *vp));

/*
 * parse and evalute expression
 */
int
evaluate(expr, rval, error_ok)
	const char *expr;
	long *rval;
	int error_ok;
{
	struct tbl v;
	int ret;

	v.flag = DEFINED|INTEGER;
	v.type = 0;
	ret = v_evaluate(&v, expr, error_ok);
	*rval = v.val.i;
	return ret;
}

/*
 * parse and evalute expression, storing result in vp.
 */
int
v_evaluate(vp, expr, error_ok)
	struct tbl *vp;
	const char *expr;
	volatile int error_ok;
{
	struct tbl *v;
	Expr_state curstate;
	Expr_state * const es = &curstate;
	int i;

	/* save state to allow recursive calls */
	curstate.expression = curstate.tokp = expr;
	curstate.noassign = 0;
	curstate.evaling = (struct tbl *) 0;

	newenv(E_ERRH);
	i = ksh_sigsetjmp(e->jbuf, 0);
	if (i) {
		/* Clear EXPRINEVAL in of any variables we were playing with */
		if (curstate.evaling)
			curstate.evaling->flag &= ~EXPRINEVAL;
		quitenv();
		if (i == LAEXPR) {
			if (error_ok == KSH_RETURN_ERROR)
				return 0;
			errorf(null);
		}
		unwind(i);
		/*NOTREACHED*/
	}

	token(es);
#if 1 /* ifdef-out to disallow empty expressions to be treated as 0 */
	if (es->tok == END) {
		es->tok = LIT;
		es->val = tempvar();
	}
#endif /* 0 */
	v = intvar(es, evalexpr(es, MAX_PREC));

	if (es->tok != END)
		evalerr(es, ET_UNEXPECTED, (char *) 0);

	if (vp->flag & INTEGER)
		setint_v(vp, v);
	else
		/* can fail if readony */
		setstr(vp, str_val(v), error_ok);

	quitenv();

	return 1;
}

static void
evalerr(es, type, str)
	Expr_state *es;
	enum error_type type;
	const char *str;
{
	char tbuf[2];
	const char *s;

	switch (type) {
	case ET_UNEXPECTED:
		switch (es->tok) {
		case VAR:
			s = es->val->name;
			break;
		case LIT:
			s = str_val(es->val);
			break;
		case END:
			s = "end of expression";
			break;
		case BAD:
			tbuf[0] = *es->tokp;
			tbuf[1] = '\0';
			s = tbuf;
			break;
		default:
			s = opinfo[(int)es->tok].name;
		}
		warningf(TRUE, "%s: unexpected `%s'", es->expression, s);
		break;

	case ET_BADLIT:
		warningf(TRUE, "%s: bad number `%s'", es->expression, str);
		break;

	case ET_RECURSIVE:
		warningf(TRUE, "%s: expression recurses on parameter `%s'",
			es->expression, str);
		break;

	case ET_LVALUE:
		warningf(TRUE, "%s: %s requires lvalue",
			es->expression, str);
		break;

	case ET_RDONLY:
		warningf(TRUE, "%s: %s applied to read only variable",
			es->expression, str);
		break;

	default: /* keep gcc happy */
	case ET_STR:
		warningf(TRUE, "%s: %s", es->expression, str);
		break;
	}
	unwind(LAEXPR);
}

static struct tbl *
evalexpr(es, prec)
	Expr_state *es;
	enum prec prec;
{
	struct tbl *vl, UNINITIALIZED(*vr), *vasn;
	enum token op;
	long UNINITIALIZED(res);

	if (prec == P_PRIMARY) {
		op = es->tok;
		if (op == O_BNOT || op == O_LNOT || op == O_MINUS
		    || op == O_PLUS)
		{
			token(es);
			vl = intvar(es, evalexpr(es, P_PRIMARY));
			if (op == O_BNOT)
				vl->val.i = ~vl->val.i;
			else if (op == O_LNOT)
				vl->val.i = !vl->val.i;
			else if (op == O_MINUS)
				vl->val.i = -vl->val.i;
			/* op == O_PLUS is a no-op */
		} else if (op == OPEN_PAREN) {
			token(es);
			vl = evalexpr(es, MAX_PREC);
			if (es->tok != CLOSE_PAREN)
				evalerr(es, ET_STR, "missing )");
			token(es);
		} else if (op == O_PLUSPLUS || op == O_MINUSMINUS) {
			token(es);
			vl = do_ppmm(es, op, es->val, TRUE);
			token(es);
		} else if (op == VAR || op == LIT) {
			vl = es->val;
			token(es);
		} else {
			evalerr(es, ET_UNEXPECTED, (char *) 0);
			/*NOTREACHED*/
		}
		if (es->tok == O_PLUSPLUS || es->tok == O_MINUSMINUS) {
			vl = do_ppmm(es, es->tok, vl, FALSE);
			token(es);
		}
		return vl;
	}
	vl = evalexpr(es, ((int) prec) - 1);
	for (op = es->tok; IS_BINOP(op) && opinfo[(int) op].prec == prec;
		op = es->tok)
	{
		token(es);
		vasn = vl;
		if (op != O_ASN) /* vl may not have a value yet */
			vl = intvar(es, vl);
		if (IS_ASSIGNOP(op)) {
			assign_check(es, op, vasn);
			vr = intvar(es, evalexpr(es, P_ASSIGN));
		} else if (op != O_TERN && op != O_LAND && op != O_LOR)
			vr = intvar(es, evalexpr(es, ((int) prec) - 1));
		if ((op == O_DIV || op == O_MOD || op == O_DIVASN
		     || op == O_MODASN) && vr->val.i == 0)
		{
			if (es->noassign)
				vr->val.i = 1;
			else
				evalerr(es, ET_STR, "zero divisor");
		}
		switch ((int) op) {
		case O_TIMES:
		case O_TIMESASN:
			res = vl->val.i * vr->val.i;
			break;
		case O_DIV:
		case O_DIVASN:
			res = vl->val.i / vr->val.i;
			break;
		case O_MOD:
		case O_MODASN:
			res = vl->val.i % vr->val.i;
			break;
		case O_PLUS:
		case O_PLUSASN:
			res = vl->val.i + vr->val.i;
			break;
		case O_MINUS:
		case O_MINUSASN:
			res = vl->val.i - vr->val.i;
			break;
		case O_LSHIFT:
		case O_LSHIFTASN:
			res = vl->val.i << vr->val.i;
			break;
		case O_RSHIFT:
		case O_RSHIFTASN:
			res = vl->val.i >> vr->val.i;
			break;
		case O_LT:
			res = vl->val.i < vr->val.i;
			break;
		case O_LE:
			res = vl->val.i <= vr->val.i;
			break;
		case O_GT:
			res = vl->val.i > vr->val.i;
			break;
		case O_GE:
			res = vl->val.i >= vr->val.i;
			break;
		case O_EQ:
			res = vl->val.i == vr->val.i;
			break;
		case O_NE:
			res = vl->val.i != vr->val.i;
			break;
		case O_BAND:
		case O_BANDASN:
			res = vl->val.i & vr->val.i;
			break;
		case O_BXOR:
		case O_BXORASN:
			res = vl->val.i ^ vr->val.i;
			break;
		case O_BOR:
		case O_BORASN:
			res = vl->val.i | vr->val.i;
			break;
		case O_LAND:
			if (!vl->val.i)
				es->noassign++;
			vr = intvar(es, evalexpr(es, ((int) prec) - 1));
			res = vl->val.i && vr->val.i;
			if (!vl->val.i)
				es->noassign--;
			break;
		case O_LOR:
			if (vl->val.i)
				es->noassign++;
			vr = intvar(es, evalexpr(es, ((int) prec) - 1));
			res = vl->val.i || vr->val.i;
			if (vl->val.i)
				es->noassign--;
			break;
		case O_TERN:
			{
				int e = vl->val.i != 0;
				if (!e)
					es->noassign++;
				vl = evalexpr(es, MAX_PREC);
				if (!e)
					es->noassign--;
				if (es->tok != CTERN)
					evalerr(es, ET_STR, "missing :");
				token(es);
				if (e)
					es->noassign++;
				vr = evalexpr(es, P_TERN);
				if (e)
					es->noassign--;
				vl = e ? vl : vr;
			}
			break;
		case O_ASN:
			res = vr->val.i;
			break;
		case O_COMMA:
			res = vr->val.i;
			break;
		}
		if (IS_ASSIGNOP(op)) {
			vr->val.i = res;
			if (vasn->flag & INTEGER)
				setint_v(vasn, vr);
			else
				setint(vasn, res);
			vl = vr;
		} else if (op != O_TERN)
			vl->val.i = res;
	}
	return vl;
}

static void
token(es)
	Expr_state *es;
{
	const char *cp;
	int c;
	char *tvar;

	/* skip white space */
	for (cp = es->tokp; (c = *cp), isspace(c); cp++)
		;
	es->tokp = cp;

	if (c == '\0')
		es->tok = END;
	else if (letter(c)) {
		for (; letnum(c); c = *cp)
			cp++;
		if (c == '[') {
			int len;

			len = array_ref_len(cp);
			if (len == 0)
				evalerr(es, ET_STR, "missing ]");
			cp += len;
		}
#ifdef KSH
		else if (c == '(' /*)*/ ) {
		    /* todo: add math functions (all take single argument):
		     * abs acos asin atan cos cosh exp int log sin sinh sqrt
		     * tan tanh
		     */
		    ;
		}
#endif /* KSH */
		if (es->noassign) {
			es->val = tempvar();
			es->val->flag |= EXPRLVALUE;
		} else {
			tvar = str_nsave(es->tokp, cp - es->tokp, ATEMP);
			es->val = global(tvar);
			afree(tvar, ATEMP);
		}
		es->tok = VAR;
	} else if (digit(c)) {
		for (; c != '_' && (letnum(c) || c == '#'); c = *cp++)
			;
		tvar = str_nsave(es->tokp, --cp - es->tokp, ATEMP);
		es->val = tempvar();
		es->val->flag &= ~INTEGER;
		es->val->type = 0;
		es->val->val.s = tvar;
		if (setint_v(es->val, es->val) == NULL)
			evalerr(es, ET_BADLIT, tvar);
		afree(tvar, ATEMP);
		es->tok = LIT;
	} else {
		int i, n0;

		for (i = 0; (n0 = opinfo[i].name[0]); i++)
			if (c == n0
			    && strncmp(cp, opinfo[i].name, opinfo[i].len) == 0)
			{
				es->tok = (enum token) i;
				cp += opinfo[i].len;
				break;
			}
		if (!n0)
			es->tok = BAD;
	}
	es->tokp = cp;
}

/* Do a ++ or -- operation */
static struct tbl *
do_ppmm(es, op, vasn, is_prefix)
	Expr_state *es;
	enum token op;
	struct tbl *vasn;
	bool_t is_prefix;
{
	struct tbl *vl;
	int oval;

	assign_check(es, op, vasn);

	vl = intvar(es, vasn);
	oval = op == O_PLUSPLUS ? vl->val.i++ : vl->val.i--;
	if (vasn->flag & INTEGER)
		setint_v(vasn, vl);
	else
		setint(vasn, vl->val.i);
	if (!is_prefix)		/* undo the inc/dec */
		vl->val.i = oval;

	return vl;
}

static void
assign_check(es, op, vasn)
	Expr_state *es;
	enum token op;
	struct tbl *vasn;
{
	if (vasn->name[0] == '\0' && !(vasn->flag & EXPRLVALUE))
		evalerr(es, ET_LVALUE, opinfo[(int) op].name);
	else if (vasn->flag & RDONLY)
		evalerr(es, ET_RDONLY, opinfo[(int) op].name);
}

static struct tbl *
tempvar()
{
	register struct tbl *vp;

	vp = (struct tbl*) alloc(sizeof(struct tbl), ATEMP);
	vp->flag = ISSET|INTEGER;
	vp->type = 0;
	vp->areap = ATEMP;
	vp->val.i = 0;
	vp->name[0] = '\0';
	return vp;
}

/* cast (string) variable to temporary integer variable */
static struct tbl *
intvar(es, vp)
	Expr_state *es;
	struct tbl *vp;
{
	struct tbl *vq;

	/* try to avoid replacing a temp var with another temp var */
	if (vp->name[0] == '\0'
	    && (vp->flag & (ISSET|INTEGER|EXPRLVALUE)) == (ISSET|INTEGER))
		return vp;

	vq = tempvar();
	if (setint_v(vq, vp) == NULL) {
		if (vp->flag & EXPRINEVAL)
			evalerr(es, ET_RECURSIVE, vp->name);
		es->evaling = vp;
		vp->flag |= EXPRINEVAL;
		v_evaluate(vq, str_val(vp), KSH_UNWIND_ERROR);
		vp->flag &= ~EXPRINEVAL;
		es->evaling = (struct tbl *) 0;
	}
	return vq;
}
