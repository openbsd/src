/*    cop.h
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

struct cop {
    BASEOP
    char *	cop_label;	/* label for this construct */
    HV *	cop_stash;	/* package line was compiled in */
    GV *	cop_filegv;	/* file the following line # is from */
    U32		cop_seq;	/* parse sequence number */
    I32		cop_arybase;	/* array base this line was compiled with */
    line_t      cop_line;       /* line # of this command */
};

#define Nullcop Null(COP*)

/*
 * Here we have some enormously heavy (or at least ponderous) wizardry.
 */

/* subroutine context */
struct block_sub {
    CV *	cv;
    GV *	gv;
    GV *	dfoutgv;
    AV *	savearray;
    AV *	argarray;
    U16		olddepth;
    U8		hasargs;
};

#define PUSHSUB(cx)							\
	cx->blk_sub.cv = cv;						\
	cx->blk_sub.olddepth = CvDEPTH(cv);				\
	cx->blk_sub.hasargs = hasargs;

#define PUSHFORMAT(cx)							\
	cx->blk_sub.cv = cv;						\
	cx->blk_sub.gv = gv;						\
	cx->blk_sub.hasargs = 0;					\
	cx->blk_sub.dfoutgv = defoutgv;					\
	(void)SvREFCNT_inc(cx->blk_sub.dfoutgv)

#define POPSUB(cx)							\
	if (cx->blk_sub.hasargs) {   /* put back old @_ */		\
	    GvAV(defgv) = cx->blk_sub.savearray;			\
	}								\
	if (cx->blk_sub.cv) {						\
	    if (!(CvDEPTH(cx->blk_sub.cv) = cx->blk_sub.olddepth)) {	\
		SvREFCNT_dec((SV*)cx->blk_sub.cv);			\
	    }								\
	}

#define POPFORMAT(cx)							\
	setdefout(cx->blk_sub.dfoutgv);					\
	SvREFCNT_dec(cx->blk_sub.dfoutgv);

/* eval context */
struct block_eval {
    I32		old_in_eval;
    I32		old_op_type;
    char *	old_name;
    OP *	old_eval_root;
    SV *	cur_text;
};

#define PUSHEVAL(cx,n,fgv)						\
	cx->blk_eval.old_in_eval = in_eval;				\
	cx->blk_eval.old_op_type = op->op_type;				\
	cx->blk_eval.old_name = n;					\
	cx->blk_eval.old_eval_root = eval_root;				\
	cx->blk_eval.cur_text = linestr;

#define POPEVAL(cx)							\
	in_eval = cx->blk_eval.old_in_eval;				\
	optype = cx->blk_eval.old_op_type;				\
	eval_root = cx->blk_eval.old_eval_root;

/* loop context */
struct block_loop {
    char *	label;
    I32		resetsp;
    OP *	redo_op;
    OP *	next_op;
    OP *	last_op;
    SV **	itervar;
    SV *	itersave;
    AV *	iterary;
    I32		iterix;
};

#define PUSHLOOP(cx, ivar, s)						\
	cx->blk_loop.label = curcop->cop_label;				\
	cx->blk_loop.resetsp = s - stack_base;				\
	cx->blk_loop.redo_op = cLOOP->op_redoop;			\
	cx->blk_loop.next_op = cLOOP->op_nextop;			\
	cx->blk_loop.last_op = cLOOP->op_lastop;			\
	cx->blk_loop.itervar = ivar;					\
	if (ivar)							\
	    cx->blk_loop.itersave = *cx->blk_loop.itervar;

#define POPLOOP(cx)							\
	newsp		= stack_base + cx->blk_loop.resetsp;

/* context common to subroutines, evals and loops */
struct block {
    I32		blku_oldsp;	/* stack pointer to copy stuff down to */
    COP *	blku_oldcop;	/* old curcop pointer */
    I32		blku_oldretsp;	/* return stack index */
    I32		blku_oldmarksp;	/* mark stack index */
    I32		blku_oldscopesp;	/* scope stack index */
    PMOP *	blku_oldpm;	/* values of pattern match vars */
    U8		blku_gimme;	/* is this block running in list context? */

    union {
	struct block_sub	blku_sub;
	struct block_eval	blku_eval;
	struct block_loop	blku_loop;
    } blk_u;
};
#define blk_oldsp	cx_u.cx_blk.blku_oldsp
#define blk_oldcop	cx_u.cx_blk.blku_oldcop
#define blk_oldretsp	cx_u.cx_blk.blku_oldretsp
#define blk_oldmarksp	cx_u.cx_blk.blku_oldmarksp
#define blk_oldscopesp	cx_u.cx_blk.blku_oldscopesp
#define blk_oldpm	cx_u.cx_blk.blku_oldpm
#define blk_gimme	cx_u.cx_blk.blku_gimme
#define blk_sub		cx_u.cx_blk.blk_u.blku_sub
#define blk_eval	cx_u.cx_blk.blk_u.blku_eval
#define blk_loop	cx_u.cx_blk.blk_u.blku_loop

/* Enter a block. */
#define PUSHBLOCK(cx,t,sp) CXINC, cx = &cxstack[cxstack_ix],		\
	cx->cx_type		= t,					\
	cx->blk_oldsp		= sp - stack_base,			\
	cx->blk_oldcop		= curcop,				\
	cx->blk_oldmarksp	= markstack_ptr - markstack,		\
	cx->blk_oldscopesp	= scopestack_ix,			\
	cx->blk_oldretsp	= retstack_ix,				\
	cx->blk_oldpm		= curpm,				\
	cx->blk_gimme		= gimme;				\
	DEBUG_l( fprintf(stderr,"Entering block %ld, type %s\n",	\
		    (long)cxstack_ix, block_type[t]); )

/* Exit a block (RETURN and LAST). */
#define POPBLOCK(cx,pm) cx = &cxstack[cxstack_ix--],			\
	newsp		= stack_base + cx->blk_oldsp,			\
	curcop		= cx->blk_oldcop,				\
	markstack_ptr	= markstack + cx->blk_oldmarksp,		\
	scopestack_ix	= cx->blk_oldscopesp,				\
	retstack_ix	= cx->blk_oldretsp,				\
	pm		= cx->blk_oldpm,				\
	gimme		= cx->blk_gimme;				\
	DEBUG_l( fprintf(stderr,"Leaving block %ld, type %s\n",		\
		    (long)cxstack_ix+1,block_type[cx->cx_type]); )

/* Continue a block elsewhere (NEXT and REDO). */
#define TOPBLOCK(cx) cx = &cxstack[cxstack_ix],				\
	stack_sp	= stack_base + cx->blk_oldsp,			\
	markstack_ptr	= markstack + cx->blk_oldmarksp,		\
	scopestack_ix	= cx->blk_oldscopesp,				\
	retstack_ix	= cx->blk_oldretsp

/* substitution context */
struct subst {
    I32		sbu_iters;
    I32		sbu_maxiters;
    I32		sbu_safebase;
    I32		sbu_once;
    I32		sbu_oldsave;
    char *	sbu_orig;
    SV *	sbu_dstr;
    SV *	sbu_targ;
    char *	sbu_s;
    char *	sbu_m;
    char *	sbu_strend;
    char *	sbu_subbase;
    REGEXP *	sbu_rx;
};
#define sb_iters	cx_u.cx_subst.sbu_iters
#define sb_maxiters	cx_u.cx_subst.sbu_maxiters
#define sb_safebase	cx_u.cx_subst.sbu_safebase
#define sb_once		cx_u.cx_subst.sbu_once
#define sb_oldsave	cx_u.cx_subst.sbu_oldsave
#define sb_orig		cx_u.cx_subst.sbu_orig
#define sb_dstr		cx_u.cx_subst.sbu_dstr
#define sb_targ		cx_u.cx_subst.sbu_targ
#define sb_s		cx_u.cx_subst.sbu_s
#define sb_m		cx_u.cx_subst.sbu_m
#define sb_strend	cx_u.cx_subst.sbu_strend
#define sb_subbase	cx_u.cx_subst.sbu_subbase
#define sb_rx		cx_u.cx_subst.sbu_rx

#define PUSHSUBST(cx) CXINC, cx = &cxstack[cxstack_ix],			\
	cx->sb_iters		= iters,				\
	cx->sb_maxiters		= maxiters,				\
	cx->sb_safebase		= safebase,				\
	cx->sb_once		= once,					\
	cx->sb_oldsave		= oldsave,				\
	cx->sb_orig		= orig,					\
	cx->sb_dstr		= dstr,					\
	cx->sb_targ		= targ,					\
	cx->sb_s		= s,					\
	cx->sb_m		= m,					\
	cx->sb_strend		= strend,				\
	cx->sb_rx		= rx,					\
	cx->cx_type		= CXt_SUBST

#define POPSUBST(cx) cxstack_ix--

struct context {
    I32		cx_type;	/* what kind of context this is */
    union {
	struct block	cx_blk;
	struct subst	cx_subst;
    } cx_u;
};
#define CXt_NULL	0
#define CXt_SUB		1
#define CXt_EVAL	2
#define CXt_LOOP	3
#define CXt_SUBST	4
#define CXt_BLOCK	5

#define CXINC (cxstack_ix < cxstack_max ? ++cxstack_ix : (cxstack_ix = cxinc()))

/* "gimme" values */
#define G_SCALAR	0
#define G_ARRAY		1

/* extra flags for perl_call_* routines */
#define G_DISCARD	2	/* Call FREETMPS. */
#define G_EVAL		4	/* Assume eval {} around subroutine call. */
#define G_NOARGS	8	/* Don't construct a @_ array. */
#define G_KEEPERR      16	/* Append errors to $@ rather than overwriting it */
