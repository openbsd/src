#define SAVEt_ITEM	0
#define SAVEt_SV	1
#define SAVEt_AV	2
#define SAVEt_HV	3
#define SAVEt_INT	4
#define SAVEt_LONG	5
#define SAVEt_I32	6
#define SAVEt_IV	7
#define SAVEt_SPTR	8
#define SAVEt_APTR	9
#define SAVEt_HPTR	10
#define SAVEt_PPTR	11
#define SAVEt_NSTAB	12
#define SAVEt_SVREF	13
#define SAVEt_GP	14
#define SAVEt_FREESV	15
#define SAVEt_FREEOP	16
#define SAVEt_FREEPV	17
#define SAVEt_CLEARSV	18
#define SAVEt_DELETE	19
#define SAVEt_DESTRUCTOR 20
#define SAVEt_REGCONTEXT 21
#define SAVEt_STACK_POS  22
#define SAVEt_I16	23

#define SSCHECK(need) if (savestack_ix + need > savestack_max) savestack_grow()
#define SSPUSHINT(i) (savestack[savestack_ix++].any_i32 = (I32)(i))
#define SSPUSHLONG(i) (savestack[savestack_ix++].any_long = (long)(i))
#define SSPUSHIV(i) (savestack[savestack_ix++].any_iv = (IV)(i))
#define SSPUSHPTR(p) (savestack[savestack_ix++].any_ptr = (void*)(p))
#define SSPUSHDPTR(p) (savestack[savestack_ix++].any_dptr = (p))
#define SSPOPINT (savestack[--savestack_ix].any_i32)
#define SSPOPLONG (savestack[--savestack_ix].any_long)
#define SSPOPIV (savestack[--savestack_ix].any_iv)
#define SSPOPPTR (savestack[--savestack_ix].any_ptr)
#define SSPOPDPTR (savestack[--savestack_ix].any_dptr)

#define SAVETMPS save_int((int*)&tmps_floor), tmps_floor = tmps_ix
#define FREETMPS if (tmps_ix > tmps_floor) free_tmps()
#ifdef DEPRECATED
#define FREE_TMPS() FREETMPS
#endif

#define ENTER push_scope()
#define LEAVE pop_scope()
#define LEAVE_SCOPE(old) if (savestack_ix > old) leave_scope(old)

/*
 * Not using SOFT_CAST on SAVEFREESV and SAVEFREESV
 * because these are used for several kinds of pointer values
 */
#define SAVEI16(i)	save_I16(SOFT_CAST(I16*)&(i))
#define SAVEI32(i)	save_I32(SOFT_CAST(I32*)&(i))
#define SAVEINT(i)	save_int(SOFT_CAST(int*)&(i))
#define SAVEIV(i)	save_iv(SOFT_CAST(IV*)&(i))
#define SAVELONG(l)	save_long(SOFT_CAST(long*)&(l))
#define SAVESPTR(s)	save_sptr((SV**)&(s))
#define SAVEPPTR(s)	save_pptr(SOFT_CAST(char**)&(s))
#define SAVEFREESV(s)	save_freesv((SV*)(s))
#define SAVEFREEOP(o)	save_freeop(SOFT_CAST(OP*)(o))
#define SAVEFREEPV(p)	save_freepv(SOFT_CAST(char*)(p))
#define SAVECLEARSV(sv)	save_clearsv(SOFT_CAST(SV**)&(sv))
#define SAVEDELETE(h,k,l) \
	  save_delete(SOFT_CAST(HV*)(h), SOFT_CAST(char*)(k), (I32)(l))
#define SAVEDESTRUCTOR(f,p) \
	  save_destructor(SOFT_CAST(void(*)_((void*)))(f),SOFT_CAST(void*)(p))
#define SAVESTACK_POS() STMT_START {	\
    SSCHECK(2);				\
    SSPUSHINT(stack_sp - stack_base);	\
    SSPUSHINT(SAVEt_STACK_POS);		\
 } STMT_END


/* A jmpenv packages the state required to perform a proper non-local jump.
 * Note that there is a start_env initialized when perl starts, and top_env
 * points to this initially, so top_env should always be non-null.
 *
 * Existence of a non-null top_env->je_prev implies it is valid to call
 * longjmp() at that runlevel (we make sure start_env.je_prev is always
 * null to ensure this).
 *
 * je_mustcatch, when set at any runlevel to TRUE, means eval ops must
 * establish a local jmpenv to handle exception traps.  Care must be taken
 * to restore the previous value of je_mustcatch before exiting the
 * stack frame iff JMPENV_PUSH was not called in that stack frame.
 * GSAR 97-03-27
 */

struct jmpenv {
    struct jmpenv *	je_prev;
    Sigjmp_buf		je_buf;		
    int			je_ret;		/* return value of last setjmp() */
    bool		je_mustcatch;	/* longjmp()s must be caught locally */
};

typedef struct jmpenv JMPENV;

#define dJMPENV		JMPENV cur_env
#define JMPENV_PUSH(v) \
    STMT_START {					\
	cur_env.je_prev = top_env;			\
	cur_env.je_ret = Sigsetjmp(cur_env.je_buf, 1);	\
	top_env = &cur_env;				\
	cur_env.je_mustcatch = FALSE;			\
	(v) = cur_env.je_ret;				\
    } STMT_END
#define JMPENV_POP \
    STMT_START { top_env = cur_env.je_prev; } STMT_END
#define JMPENV_JUMP(v) \
    STMT_START {						\
	if (top_env->je_prev)					\
	    Siglongjmp(top_env->je_buf, (v));			\
	if ((v) == 2)						\
	    exit(STATUS_NATIVE_EXPORT);				\
	PerlIO_printf(PerlIO_stderr(), "panic: top_env\n");	\
	exit(1);						\
    } STMT_END
   
#define CATCH_GET	(top_env->je_mustcatch)
#define CATCH_SET(v)	(top_env->je_mustcatch = (v))
   
