/*	$OpenBSD: sh.h,v 1.28 2005/10/04 20:35:11 otto Exp $	*/

/*
 * Public Domain Bourne/Korn shell
 */

/* $From: sh.h,v 1.2 1994/05/19 18:32:40 michael Exp michael $ */

#include "config.h"	/* system and option configuration info */

/* Start of common headers */

#include <stdio.h>
#include <sys/types.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>

#include <signal.h>

#include <paths.h>

/* Find a integer type that is at least 32 bits (or die) - SIZEOF_* defined
 * by autoconf (assumes an 8 bit byte, but I'm not concerned).
 * NOTE: INT32 may end up being more than 32 bits.
 */
# define INT32	int

/* end of common headers */

/* some useful #defines */
#ifdef EXTERN
# define I__(i) = i
#else
# define I__(i)
# define EXTERN extern
# define EXTERN_DEFINED
#endif

#define EXECSHELL	_PATH_BSHELL
#define EXECSHELL_STR	"EXECSHELL"

#define	NELEM(a) (sizeof(a) / sizeof((a)[0]))
#define	sizeofN(type, n) (sizeof(type) * (n))
#define	BIT(i)	(1<<(i))	/* define bit in flag */

/* Table flag type - needs > 16 and < 32 bits */
typedef INT32 Tflag;

#define	NUFILE	32		/* Number of user-accessible files */
#define	FDBASE	10		/* First file usable by Shell */

/* Make MAGIC a char that might be printed to make bugs more obvious, but
 * not a char that is used often.  Also, can't use the high bit as it causes
 * portability problems (calling strchr(x, 0x80|'x') is error prone).
 */
#define	MAGIC		(7)	/* prefix for *?[!{,} during expand */
#define ISMAGIC(c)	((unsigned char)(c) == MAGIC)
#define	NOT		'!'	/* might use ^ (ie, [!...] vs [^..]) */

#define	LINE	2048		/* input line size */
#define	PATH	1024		/* pathname size (todo: PATH_MAX/pathconf()) */
#define ARRAYMAX 2047		/* max array index */

EXTERN	const char *kshname;	/* $0 */
EXTERN	pid_t	kshpid;		/* $$, shell pid */
EXTERN	pid_t	procpid;	/* pid of executing process */
EXTERN	uid_t	ksheuid;	/* effective uid of shell */
EXTERN	int	exstat;		/* exit status */
EXTERN	int	subst_exstat;	/* exit status of last $(..)/`..` */
EXTERN	const char *safe_prompt; /* safe prompt if PS1 substitution fails */
EXTERN	char	username[];	/* username for \u prompt expansion */

/*
 * Area-based allocation built on malloc/free
 */
typedef struct Area {
	struct link *freelist;	/* free list */
} Area;

EXTERN	Area	aperm;		/* permanent object space */
#define	APERM	&aperm
#define	ATEMP	&e->area

#ifdef KSH_DEBUG
# define kshdebug_init()	kshdebug_init_()
# define kshdebug_printf(a)	kshdebug_printf_ a
# define kshdebug_dump(a)	kshdebug_dump_ a
#else /* KSH_DEBUG */
# define kshdebug_init()
# define kshdebug_printf(a)
# define kshdebug_dump(a)
#endif /* KSH_DEBUG */

/*
 * parsing & execution environment
 */
EXTERN	struct env {
	short	type;			/* environment type - see below */
	short	flags;			/* EF_* */
	Area	area;			/* temporary allocation area */
	struct	block *loc;		/* local variables and functions */
	short  *savefd;			/* original redirected fd's */
	struct	env *oenv;		/* link to previous environment */
	sigjmp_buf jbuf;		/* long jump back to env creator */
	struct temp *temps;		/* temp files */
} *e;

/* struct env.type values */
#define	E_NONE	0		/* dummy environment */
#define	E_PARSE	1		/* parsing command # */
#define	E_FUNC	2		/* executing function # */
#define	E_INCL	3		/* including a file via . # */
#define	E_EXEC	4		/* executing command tree */
#define	E_LOOP	5		/* executing for/while # */
#define	E_ERRH	6		/* general error handler # */
/* # indicates env has valid jbuf (see unwind()) */

/* struct env.flag values */
#define EF_FUNC_PARSE	BIT(0)	/* function being parsed */
#define EF_BRKCONT_PASS	BIT(1)	/* set if E_LOOP must pass break/continue on */
#define EF_FAKE_SIGDIE	BIT(2)	/* hack to get info from unwind to quitenv */

/* Do breaks/continues stop at env type e? */
#define STOP_BRKCONT(t)	((t) == E_NONE || (t) == E_PARSE \
			 || (t) == E_FUNC || (t) == E_INCL)
/* Do returns stop at env type e? */
#define STOP_RETURN(t)	((t) == E_FUNC || (t) == E_INCL)

/* values for siglongjmp(e->jbuf, 0) */
#define LRETURN	1		/* return statement */
#define	LEXIT	2		/* exit statement */
#define LERROR	3		/* errorf() called */
#define LLEAVE	4		/* untrappable exit/error */
#define LINTR	5		/* ^C noticed */
#define	LBREAK	6		/* break statement */
#define	LCONTIN	7		/* continue statement */
#define LSHELL	8		/* return to interactive shell() */
#define LAEXPR	9		/* error in arithmetic expression */

/* option processing */
#define OF_CMDLINE	0x01	/* command line */
#define OF_SET		0x02	/* set builtin */
#define OF_SPECIAL	0x04	/* a special variable changing */
#define OF_INTERNAL	0x08	/* set internally by shell */
#define OF_ANY		(OF_CMDLINE | OF_SET | OF_SPECIAL | OF_INTERNAL)

struct option {
    const char	*name;	/* long name of option */
    char	c;	/* character flag (if any) */
    short	flags;	/* OF_* */
};
extern const struct option options[];

/*
 * flags (the order of these enums MUST match the order in misc.c(options[]))
 */
enum sh_flag {
	FEXPORT = 0,	/* -a: export all */
#ifdef BRACE_EXPAND
	FBRACEEXPAND,	/* enable {} globbing */
#endif
	FBGNICE,	/* bgnice */
	FCOMMAND,	/* -c: (invocation) execute specified command */
	FCSHHISTORY,	/* csh-style history enabled */
#ifdef EMACS
	FEMACS,		/* emacs command editing */
	FEMACSUSEMETA,	/* use 8th bit as meta */
#endif
	FERREXIT,	/* -e: quit on error */
#ifdef EMACS
	FGMACS,		/* gmacs command editing */
#endif
	FIGNOREEOF,	/* eof does not exit */
	FTALKING,	/* -i: interactive */
	FKEYWORD,	/* -k: name=value anywhere */
	FLOGIN,		/* -l: a login shell */
	FMARKDIRS,	/* mark dirs with / in file name completion */
	FMONITOR,	/* -m: job control monitoring */
	FNOCLOBBER,	/* -C: don't overwrite existing files */
	FNOEXEC,	/* -n: don't execute any commands */
	FNOGLOB,	/* -f: don't do file globbing */
	FNOHUP,		/* -H: don't kill running jobs when login shell exits */
	FNOLOG,		/* don't save functions in history (ignored) */
#ifdef	JOBS
	FNOTIFY,	/* -b: asynchronous job completion notification */
#endif
	FNOUNSET,	/* -u: using an unset var is an error */
	FPHYSICAL,	/* -o physical: don't do logical cd's/pwd's */
	FPOSIX,		/* -o posix: be posixly correct */
	FPRIVILEGED,	/* -p: use suid_profile */
	FRESTRICTED,	/* -r: restricted shell */
	FSH,		/* -o sh: favor sh behaviour */
	FSTDIN,		/* -s: (invocation) parse stdin */
	FTRACKALL,	/* -h: create tracked aliases for all commands */
	FVERBOSE,	/* -v: echo input */
#ifdef VI
	FVI,		/* vi command editing */
	FVIRAW,		/* always read in raw mode (ignored) */
	FVISHOW8,	/* display chars with 8th bit set as is (versus M-) */
	FVITABCOMPLETE,	/* enable tab as file name completion char */
	FVIESCCOMPLETE,	/* enable ESC as file name completion in command mode */
#endif
	FXTRACE,	/* -x: execution trace */
	FTALKING_I,	/* (internal): initial shell was interactive */
	FNFLAGS /* (place holder: how many flags are there) */
};

#define Flag(f)	(shell_flags[(int) (f)])

EXTERN	char shell_flags [FNFLAGS];

EXTERN	char	null [] I__("");	/* null value for variable */
EXTERN	char	space [] I__(" ");
EXTERN	char	newline [] I__("\n");
EXTERN	char	slash [] I__("/");

enum temp_type {
	TT_HEREDOC_EXP,	/* expanded heredoc */
	TT_HIST_EDIT	/* temp file used for history editing (fc -e) */
};
typedef enum temp_type Temp_type;
/* temp/heredoc files.  The file is removed when the struct is freed. */
struct temp {
	struct temp	*next;
	struct shf	*shf;
	int		pid;		/* pid of process parsed here-doc */
	Temp_type	type;
	char		*name;
};

/*
 * stdio and our IO routines
 */

#define shl_spare	(&shf_iob[0])	/* for c_read()/c_print() */
#define shl_stdout	(&shf_iob[1])
#define shl_out		(&shf_iob[2])
EXTERN int shl_stdout_ok;

/*
 * trap handlers
 */
typedef struct trap {
	int	signal;		/* signal number */
	const char *name;	/* short name */
	const char *mess;	/* descriptive name */
	char   *trap;		/* trap command */
	volatile sig_atomic_t set; /* trap pending */
	int	flags;		/* TF_* */
	sig_t cursig;		/* current handler (valid if TF_ORIG_* set) */
	sig_t shtrap;		/* shell signal handler */
} Trap;

/* values for Trap.flags */
#define TF_SHELL_USES	BIT(0)	/* shell uses signal, user can't change */
#define TF_USER_SET	BIT(1)	/* user has (tried to) set trap */
#define TF_ORIG_IGN	BIT(2)	/* original action was SIG_IGN */
#define TF_ORIG_DFL	BIT(3)	/* original action was SIG_DFL */
#define TF_EXEC_IGN	BIT(4)	/* restore SIG_IGN just before exec */
#define TF_EXEC_DFL	BIT(5)	/* restore SIG_DFL just before exec */
#define TF_DFL_INTR	BIT(6)	/* when received, default action is LINTR */
#define TF_TTY_INTR	BIT(7)	/* tty generated signal (see j_waitj) */
#define TF_CHANGED	BIT(8)	/* used by runtrap() to detect trap changes */
#define TF_FATAL	BIT(9)	/* causes termination if not trapped */

/* values for setsig()/setexecsig() flags argument */
#define SS_RESTORE_MASK	0x3	/* how to restore a signal before an exec() */
#define SS_RESTORE_CURR	0	/* leave current handler in place */
#define SS_RESTORE_ORIG	1	/* restore original handler */
#define SS_RESTORE_DFL	2	/* restore to SIG_DFL */
#define SS_RESTORE_IGN	3	/* restore to SIG_IGN */
#define SS_FORCE	BIT(3)	/* set signal even if original signal ignored */
#define SS_USER		BIT(4)	/* user is doing the set (ie, trap command) */
#define SS_SHTRAP	BIT(5)	/* trap for internal use (CHLD,ALRM,WINCH) */

#define SIGEXIT_	0	/* for trap EXIT */
#define SIGERR_		NSIG	/* for trap ERR */

EXTERN	volatile sig_atomic_t trap;	/* traps pending? */
EXTERN	volatile sig_atomic_t intrsig;	/* pending trap interrupts command */
EXTERN	volatile sig_atomic_t fatal_trap;/* received a fatal signal */
extern	Trap	sigtraps[NSIG+1];

/*
 * TMOUT support
 */
/* values for ksh_tmout_state */
enum tmout_enum {
	TMOUT_EXECUTING	= 0,	/* executing commands */
	TMOUT_READING,		/* waiting for input */
	TMOUT_LEAVING		/* have timed out */
};
EXTERN unsigned int ksh_tmout;
EXTERN enum tmout_enum ksh_tmout_state I__(TMOUT_EXECUTING);

/* For "You have stopped jobs" message */
EXTERN int really_exit;

/*
 * fast character classes
 */
#define	C_ALPHA	 BIT(0)		/* a-z_A-Z */
#define	C_DIGIT	 BIT(1)		/* 0-9 */
#define	C_LEX1	 BIT(2)		/* \0 \t\n|&;<>() */
#define	C_VAR1	 BIT(3)		/* *@#!$-? */
#define	C_IFSWS	 BIT(4)		/* \t \n (IFS white space) */
#define	C_SUBOP1 BIT(5)		/* "=-+?" */
#define	C_SUBOP2 BIT(6)		/* "#%" */
#define	C_IFS	 BIT(7)		/* $IFS */
#define	C_QUOTE	 BIT(8)		/*  \n\t"#$&'()*;<>?[\`| (needing quoting) */

extern	short ctypes [];

#define	ctype(c, t)	!!(ctypes[(unsigned char)(c)]&(t))
#define	letter(c)	ctype(c, C_ALPHA)
#define	digit(c)	ctype(c, C_DIGIT)
#define	letnum(c)	ctype(c, C_ALPHA|C_DIGIT)

EXTERN int ifs0 I__(' ');	/* for "$*" */

/* Argument parsing for built-in commands and getopts command */

/* Values for Getopt.flags */
#define GF_ERROR	BIT(0)	/* call errorf() if there is an error */
#define GF_PLUSOPT	BIT(1)	/* allow +c as an option */
#define GF_NONAME	BIT(2)	/* don't print argv[0] in errors */

/* Values for Getopt.info */
#define GI_MINUS	BIT(0)	/* an option started with -... */
#define GI_PLUS		BIT(1)	/* an option started with +... */
#define GI_MINUSMINUS	BIT(2)	/* arguments were ended with -- */

typedef struct {
	int		optind;
	int		uoptind;/* what user sees in $OPTIND */
	char		*optarg;
	int		flags;	/* see GF_* */
	int		info;	/* see GI_* */
	unsigned int	p;	/* 0 or index into argv[optind - 1] */
	char		buf[2];	/* for bad option OPTARG value */
} Getopt;

EXTERN Getopt builtin_opt;	/* for shell builtin commands */
EXTERN Getopt user_opt;		/* parsing state for getopts builtin command */

/* This for co-processes */

typedef INT32 Coproc_id; /* something that won't (realisticly) wrap */
struct coproc {
	int	read;		/* pipe from co-process's stdout */
	int	readw;		/* other side of read (saved temporarily) */
	int	write;		/* pipe to co-process's stdin */
	Coproc_id id;		/* id of current output pipe */
	int	njobs;		/* number of live jobs using output pipe */
	void    *job;           /* 0 or job of co-process using input pipe */
};
EXTERN struct coproc coproc;

/* Used in jobs.c and by coprocess stuff in exec.c */
EXTERN sigset_t		sm_default, sm_sigchld;

extern const char ksh_version[];

/* name of called builtin function (used by error functions) */
EXTERN char	*builtin_argv0;
EXTERN Tflag	builtin_flag;	/* flags of called builtin (SPEC_BI, etc.) */

/* current working directory, and size of memory allocated for same */
EXTERN char	*current_wd;
EXTERN int	current_wd_size;

#ifdef EDIT
/* Minimum required space to work with on a line - if the prompt leaves less
 * space than this on a line, the prompt is truncated.
 */
# define MIN_EDIT_SPACE	7
/* Minimum allowed value for x_cols: 2 for prompt, 3 for " < " at end of line
 */
# define MIN_COLS	(2 + MIN_EDIT_SPACE + 3)
EXTERN	int	x_cols I__(80);	/* tty columns */
#else
# define x_cols 80		/* for pr_menu(exec.c) */
#endif

/* These to avoid bracket matching problems */
#define OPAREN	'('
#define CPAREN	')'
#define OBRACK	'['
#define CBRACK	']'
#define OBRACE	'{'
#define CBRACE	'}'

/* Determine the location of the system (common) profile */
#define KSH_SYSTEM_PROFILE "/etc/profile"

/* Used by v_evaluate() and setstr() to control action when error occurs */
#define KSH_UNWIND_ERROR	0	/* unwind the stack (longjmp) */
#define KSH_RETURN_ERROR	1	/* return 1/0 for success/failure */

#include "shf.h"
#include "table.h"
#include "tree.h"
#include "expand.h"
#include "lex.h"
#include "proto.h"

/* be sure not to interfere with anyone else's idea about EXTERN */
#ifdef EXTERN_DEFINED
# undef EXTERN_DEFINED
# undef EXTERN
#endif
#undef I__
