/*    op.h
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * The fields of BASEOP are:
 *	op_next		Pointer to next ppcode to execute after this one.
 *			(Top level pre-grafted op points to first op,
 *			but this is replaced when op is grafted in, when
 *			this op will point to the real next op, and the new
 *			parent takes over role of remembering starting op.)
 *	op_ppaddr	Pointer to current ppcode's function.
 *	op_type		The type of the operation.
 *	op_flags	Flags common to all operations.  See OPf_* below.
 *	op_private	Flags peculiar to a particular operation (BUT,
 *			by default, set to the number of children until
 *			the operation is privatized by a check routine,
 *			which may or may not check number of children).
 */

typedef U32 PADOFFSET;

#ifdef DEBUGGING_OPS
#define OPCODE opcode
#else
#define OPCODE U16
#endif

#define BASEOP				\
    OP*		op_next;		\
    OP*		op_sibling;		\
    OP*		(*op_ppaddr)();		\
    PADOFFSET	op_targ;		\
    OPCODE	op_type;		\
    U16		op_seq;			\
    U8		op_flags;		\
    U8		op_private;

#define GIMME (op->op_flags & OPf_KNOW ? op->op_flags & OPf_LIST : dowantarray())

/* Public flags */
#define OPf_LIST	1	/* Do operator in list context. */
#define OPf_KNOW	2	/* Context is known. */
#define OPf_KIDS	4	/* There is a firstborn child. */
#define OPf_PARENS	8	/* This operator was parenthesized. */
				/*  (Or block needs explicit scope entry.) */
#define OPf_REF		16	/* Certified reference. */
				/*  (Return container, not containee). */
#define OPf_MOD		32	/* Will modify (lvalue). */
#define OPf_STACKED	64	/* Some arg is arriving on the stack. */
#define OPf_SPECIAL	128	/* Do something weird for this op: */
				/*  On local LVAL, don't init local value. */
				/*  On OP_SORT, subroutine is inlined. */
				/*  On OP_NOT, inversion was implicit. */
				/*  On OP_LEAVE, don't restore curpm. */
				/*  On truncate, we truncate filehandle */
				/*  On control verbs, we saw no label */
				/*  On flipflop, we saw ... instead of .. */
				/*  On UNOPs, saw bare parens, e.g. eof(). */
				/*  On OP_ENTERSUB || OP_NULL, saw a "do". */

/* Private for lvalues */
#define OPpLVAL_INTRO	128	/* Lvalue must be localized */

/* Private for OP_AASSIGN */
#define OPpASSIGN_COMMON	64	/* Left & right have syms in common. */

/* Private for OP_SASSIGN */
#define OPpASSIGN_BACKWARDS	64	/* Left & right switched. */

/* Private for OP_TRANS */
#define OPpTRANS_SQUASH		16
#define OPpTRANS_DELETE		32
#define OPpTRANS_COMPLEMENT	64

/* Private for OP_REPEAT */
#define OPpREPEAT_DOLIST	64	/* List replication. */

/* Private for OP_ENTERSUB, OP_RV2?V, OP_?ELEM */
  /* (lower bits carry hints) */
#define OPpENTERSUB_AMPER	8	/* Used & form to call. */
#define OPpENTERSUB_DB		16	/* Debug subroutine. */
#define OPpDEREF_AV		32	/* Want ref to AV. */
#define OPpDEREF_HV		64	/* Want ref to HV. */

/* Private for OP_CONST */
#define OPpCONST_ENTERED	16	/* Has been entered as symbol. */
#define OPpCONST_ARYBASE	32	/* Was a $[ translated to constant. */
#define OPpCONST_BARE		64	/* Was a bare word (filehandle?). */

/* Private for OP_FLIP/FLOP */
#define OPpFLIP_LINENUM		64	/* Range arg potentially a line num. */

/* Private for OP_LIST */
#define OPpLIST_GUESSED		64	/* Guessed that pushmark was needed. */

/* Private for OP_LEAVE and friends */
#define OPpLEAVE_VOID		64	/* No need to copy out values. */

struct op {
    BASEOP
};

struct unop {
    BASEOP
    OP *	op_first;
};

struct binop {
    BASEOP
    OP *	op_first;
    OP *	op_last;
};

struct logop {
    BASEOP
    OP *	op_first;
    OP *	op_other;
};

struct condop {
    BASEOP
    OP *	op_first;
    OP *	op_true;
    OP *	op_false;
};

struct listop {
    BASEOP
    OP *	op_first;
    OP *	op_last;
    U32		op_children;
};

struct pmop {
    BASEOP
    OP *	op_first;
    OP *	op_last;
    U32		op_children;
    OP *	op_pmreplroot;
    OP *	op_pmreplstart;
    PMOP *	op_pmnext;		/* list of all scanpats */
    REGEXP *	op_pmregexp;		/* compiled expression */
    SV *	op_pmshort;		/* for a fast bypass of execute() */
    U16		op_pmflags;
    U16		op_pmpermflags;
    char	op_pmslen;
};

#define PMf_USED	0x0001		/* pm has been used once already */
#define PMf_ONCE	0x0002		/* use pattern only once per reset */
#define PMf_SCANFIRST	0x0004		/* initial constant not anchored */
#define PMf_ALL		0x0008		/* initial constant is whole pat */
#define PMf_SKIPWHITE	0x0010		/* skip leading whitespace for split */
#define PMf_FOLD	0x0020		/* case insensitivity */
#define PMf_CONST	0x0040		/* subst replacement is constant */
#define PMf_KEEP	0x0080		/* keep 1st runtime pattern forever */
#define PMf_GLOBAL	0x0100		/* pattern had a g modifier */
#define PMf_RUNTIME	0x0200		/* pattern coming in on the stack */
#define PMf_EVAL	0x0400		/* evaluating replacement as expr */
#define PMf_WHITE	0x0800		/* pattern is \s+ */
#define PMf_MULTILINE	0x1000		/* assume multiple lines */
#define PMf_SINGLELINE	0x2000		/* assume single line */
#define PMf_UNUSED	0x4000		/* (unused) */
#define PMf_EXTENDED	0x8000		/* chuck embedded whitespace */

struct svop {
    BASEOP
    SV *	op_sv;
};

struct gvop {
    BASEOP
    GV *	op_gv;
};

struct pvop {
    BASEOP
    char *	op_pv;
};

struct loop {
    BASEOP
    OP *	op_first;
    OP *	op_last;
    U32		op_children;
    OP *	op_redoop;
    OP *	op_nextop;
    OP *	op_lastop;
};

#define cUNOP ((UNOP*)op)
#define cBINOP ((BINOP*)op)
#define cLISTOP ((LISTOP*)op)
#define cLOGOP ((LOGOP*)op)
#define cCONDOP ((CONDOP*)op)
#define cPMOP ((PMOP*)op)
#define cSVOP ((SVOP*)op)
#define cGVOP ((GVOP*)op)
#define cPVOP ((PVOP*)op)
#define cCOP ((COP*)op)
#define cLOOP ((LOOP*)op)

#define kUNOP ((UNOP*)kid)
#define kBINOP ((BINOP*)kid)
#define kLISTOP ((LISTOP*)kid)
#define kLOGOP ((LOGOP*)kid)
#define kCONDOP ((CONDOP*)kid)
#define kPMOP ((PMOP*)kid)
#define kSVOP ((SVOP*)kid)
#define kGVOP ((GVOP*)kid)
#define kPVOP ((PVOP*)kid)
#define kCOP ((COP*)kid)
#define kLOOP ((LOOP*)kid)

#define Nullop Null(OP*)

/* Lowest byte of opargs */
#define OA_MARK 1
#define OA_FOLDCONST 2
#define OA_RETSCALAR 4
#define OA_TARGET 8
#define OA_RETINTEGER 16
#define OA_OTHERINT 32
#define OA_DANGEROUS 64
#define OA_DEFGV 128

#define OASHIFT 8

/* Remaining nybbles of opargs */
#define OA_SCALAR 1
#define OA_LIST 2
#define OA_AVREF 3
#define OA_HVREF 4
#define OA_CVREF 5
#define OA_FILEREF 6
#define OA_SCALARREF 7
#define OA_OPTIONAL 8

