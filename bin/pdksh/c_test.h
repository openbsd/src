/*	$OpenBSD: c_test.h,v 1.1.1.1 1996/08/14 06:19:10 downsj Exp $	*/

/* Various types of operations.  Keeping things grouped nicely
 * (unary,binary) makes switch() statements more efficeint.
 */
enum Test_op {
	TO_NONOP = 0,	/* non-operator */
	/* unary operators */
	TO_STNZE, TO_STZER, TO_OPTION,
	TO_FILAXST,
	TO_FILEXST,
	TO_FILREG, TO_FILBDEV, TO_FILCDEV, TO_FILSYM, TO_FILFIFO, TO_FILSOCK,
	TO_FILCDF, TO_FILID, TO_FILGID, TO_FILSETG, TO_FILSTCK, TO_FILUID,
	TO_FILRD, TO_FILGZ, TO_FILTT, TO_FILSETU, TO_FILWR, TO_FILEX,
	/* binary operators */
	TO_STEQL, TO_STNEQ, TO_STLT, TO_STGT, TO_INTEQ, TO_INTNE, TO_INTGT,
	TO_INTGE, TO_INTLT, TO_INTLE, TO_FILEQ, TO_FILNT, TO_FILOT
};
typedef enum Test_op Test_op;

/* Used by Test_env.isa() (order important - used to index *_tokens[] arrays) */
enum Test_meta {
	TM_OR,		/* -o or || */
	TM_AND,		/* -a or && */
	TM_NOT,		/* ! */
	TM_OPAREN,	/* ( */
	TM_CPAREN,	/* ) */
	TM_UNOP,	/* unary operator */
	TM_BINOP,	/* binary operator */
	TM_END		/* end of input */
};
typedef enum Test_meta Test_meta;

#define TEF_ERROR	BIT(0)		/* set if we've hit an error */
#define TEF_DBRACKET	BIT(1)		/* set if [[ .. ]] test */

typedef struct test_env Test_env;
struct test_env {
	int	flags;		/* TEF_* */
	union {
		char	**wp;		/* used by ptest_* */
		XPtrV	*av;		/* used by dbtestp_* */
	} pos;
	char **wp_end;			/* used by ptest_* */
	int	(*isa) ARGS((Test_env *te, Test_meta meta));
	const char *(*getopnd) ARGS((Test_env *te, Test_op op, int do_eval));
	int	(*eval) ARGS((Test_env *te, Test_op op, const char *opnd1,
				 const char *opnd2, int do_eval));
	void	(*error) ARGS((Test_env *te, int offset, const char *msg));
};

Test_op	test_isop ARGS((Test_env *te, Test_meta meta, const char *s));
int     test_eval ARGS((Test_env *te, Test_op op, const char *opnd1,
			const char *opnd2, int do_eval));
int	test_parse ARGS((Test_env *te));
