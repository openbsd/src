/*	$OpenBSD: proto.h,v 1.3 1996/11/21 07:59:34 downsj Exp $	*/

/*
 * prototypes for PD-KSH
 * originally generated using "cproto.c 3.5 92/04/11 19:28:01 cthuang "
 * $From: proto.h,v 1.3 1994/05/19 18:32:40 michael Exp michael $
 */

/* alloc.c */
Area *	ainit		ARGS((Area *ap));
void 	afreeall	ARGS((Area *ap));
void *	alloc		ARGS((size_t size, Area *ap));
void *	aresize		ARGS((void *ptr, size_t size, Area *ap));
void 	afree		ARGS((void *ptr, Area *ap));
/* c_ksh.c */
int 	c_hash		ARGS((char **wp));
int 	c_cd		ARGS((char **wp));
int 	c_pwd		ARGS((char **wp));
int 	c_print		ARGS((char **wp));
int 	c_whence	ARGS((char **wp));
int 	c_command	ARGS((char **wp));
int 	c_typeset	ARGS((char **wp));
int 	c_alias		ARGS((char **wp));
int 	c_unalias	ARGS((char **wp));
int 	c_let		ARGS((char **wp));
int 	c_jobs		ARGS((char **wp));
int 	c_fgbg		ARGS((char **wp));
int 	c_kill		ARGS((char **wp));
void	getopts_reset	ARGS((int val));
int	c_getopts	ARGS((char **wp));
int 	c_bind		ARGS((char **wp));
/* c_sh.c */
int 	c_label		ARGS((char **wp));
int 	c_shift		ARGS((char **wp));
int 	c_umask		ARGS((char **wp));
int 	c_dot		ARGS((char **wp));
int 	c_wait		ARGS((char **wp));
int 	c_read		ARGS((char **wp));
int 	c_eval		ARGS((char **wp));
int 	c_trap		ARGS((char **wp));
int 	c_brkcont	ARGS((char **wp));
int 	c_exitreturn	ARGS((char **wp));
int 	c_set		ARGS((char **wp));
int 	c_unset		ARGS((char **wp));
int 	c_ulimit	ARGS((char **wp));
int 	c_times		ARGS((char **wp));
int 	timex		ARGS((struct op *t, int f));
int 	c_exec		ARGS((char **wp));
int 	c_builtin	ARGS((char **wp));
/* c_test.c */
int 	c_test		ARGS((char **wp));
/* edit.c: most prototypes in edit.h */
void 	x_init		ARGS((void));
int 	x_read		ARGS((char *buf, size_t len));
void	set_editmode	ARGS((const char *ed));
/* emacs.c: most prototypes in edit.h */
int 	x_bind		ARGS((const char *a1, const char *a2, int macro,
			      int list));
/* eval.c */
char *	substitute	ARGS((const char *cp, int f));
char **	eval		ARGS((char **ap, int f));
char *	evalstr		ARGS((char *cp, int f));
char *	evalonestr	ARGS((char *cp, int f));
char	*debunk		ARGS((char *dp, const char *sp));
void	expand		ARGS((char *cp, XPtrV *wp, int f));
int glob_str		ARGS((char *cp, XPtrV *wp, int markdirs));
/* exec.c */
int	fd_clexec	ARGS((int fd));
int 	execute		ARGS((struct op * volatile t, volatile int flags));
int 	shcomexec	ARGS((char **wp));
struct tbl * findfunc	ARGS((const char *name, unsigned int h, int create));
int 	define		ARGS((const char *name, struct op *t));
void 	builtin		ARGS((const char *name, int (*func)(char **)));
struct tbl *	findcom	ARGS((const char *name, int flags));
void 	flushcom	ARGS((int all));
char *	search		ARGS((const char *name, const char *path, int mode,
			      int *errnop));
int	search_access	ARGS((const char *path, int mode, int *errnop));
int	pr_menu		ARGS((char *const *ap));
/* expr.c */
int 	evaluate	ARGS((const char *expr, long *rval, int error_ok));
int	v_evaluate	ARGS((struct tbl *vp, const char *expr, volatile int error_ok));
/* history.c */
void	init_histvec	ARGS((void));
void 	hist_init	ARGS((Source *s));
void 	hist_finish	ARGS((void));
void	histsave	ARGS((int lno, const char *cmd, int dowrite));
#ifdef HISTORY
int 	c_fc	 	ARGS((register char **wp));
void	sethistsize	ARGS((int n));
void	sethistfile	ARGS((const char *name));
# ifdef EASY_HISTORY
void 	histappend	ARGS((const char *cmd, int nl_seperate));
# endif
char **	histpos	 	ARGS((void));
int 	histN	 	ARGS((void));
int 	histnum	 	ARGS((int n));
int	findhist	ARGS((int start, int fwd, const char *str,
			      int anchored));
#endif /* HISTORY */
/* io.c */
void 	errorf		ARGS((const char *fmt, ...))
				GCC_FUNC_ATTR2(noreturn, format(printf, 1, 2));
void 	warningf	ARGS((int fileline, const char *fmt, ...))
				GCC_FUNC_ATTR(format(printf, 2, 3));
void 	bi_errorf	ARGS((const char *fmt, ...))
				GCC_FUNC_ATTR(format(printf, 1, 2));
void 	internal_errorf	ARGS((int jump, const char *fmt, ...))
				GCC_FUNC_ATTR(format(printf, 2, 3));
void	error_prefix	ARGS((int fileline));
void 	shellf		ARGS((const char *fmt, ...))
				GCC_FUNC_ATTR(format(printf, 1, 2));
void 	shprintf	ARGS((const char *fmt, ...))
				GCC_FUNC_ATTR(format(printf, 1, 2));
int	can_seek	ARGS((int fd));
void	initio		ARGS((void));
int	ksh_dup2	ARGS((int ofd, int nfd, int errok));
int 	savefd		ARGS((int fd, int noclose));
void 	restfd		ARGS((int fd, int ofd));
void 	openpipe	ARGS((int *pv));
void 	closepipe	ARGS((int *pv));
int	check_fd	ARGS((char *name, int mode, const char **emsgp));
#ifdef KSH
void	coproc_init	ARGS((void));
void	coproc_read_close ARGS((int fd));
void	coproc_readw_close ARGS((int fd));
void	coproc_write_close ARGS((int fd));
int	coproc_getfd	ARGS((int mode, const char **emsgp));
void	coproc_cleanup	ARGS((int reuse));
#endif /* KSH */
struct temp *maketemp	ARGS((Area *ap));
/* jobs.c */
void 	j_init		ARGS((int mflagset));
void 	j_exit		ARGS((void));
void 	j_change	ARGS((void));
int 	exchild		ARGS((struct op *t, int flags, int close_fd));
void 	startlast	ARGS((void));
int 	waitlast	ARGS((void));
int 	waitfor		ARGS((const char *cp, int *sigp));
int 	j_kill		ARGS((const char *cp, int sig));
int 	j_resume	ARGS((const char *cp, int bg));
int 	j_jobs		ARGS((const char *cp, int slp, int nflag));
void 	j_notify	ARGS((void));
pid_t	j_async		ARGS((void));
int 	j_stopped_running	ARGS((void));
/* lex.c */
int 	yylex		ARGS((int cf));
void 	yyerror		ARGS((const char *fmt, ...))
				GCC_FUNC_ATTR2(noreturn, format(printf, 1, 2));
Source * pushs		ARGS((int type, Area *areap));
void	set_prompt	ARGS((int to, Source *s));
void 	pprompt		ARGS((const char *cp, int ntruncate));
/* mail.c */
#ifdef KSH
void 	mcheck		ARGS((void));
void 	mbset		ARGS((char *p));
void 	mpset		ARGS((char *mptoparse));
#endif /* KSH */
/* main.c */
int 	include		ARGS((const char *name, int argc, char **argv,
			      int intr_ok));
int 	command		ARGS((const char *comm));
int 	shell		ARGS((Source *volatile s, int volatile toplevel));
void 	unwind		ARGS((int i)) GCC_FUNC_ATTR(noreturn);
void 	newenv		ARGS((int type));
void 	quitenv		ARGS((void));
void	cleanup_parents_env ARGS((void));
void	cleanup_proc_env ARGS((void));
void 	aerror		ARGS((Area *ap, const char *msg))
				GCC_FUNC_ATTR(noreturn);
/* misc.c */
void 	setctypes	ARGS((const char *s, int t));
void 	initctypes	ARGS((void));
char *	ulton		ARGS((unsigned long n, int base));
char *	str_save	ARGS((const char *s, Area *ap));
char *	str_nsave	ARGS((const char *s, int n, Area *ap));
int	option		ARGS((const char *n));
char *	getoptions	ARGS((void));
void	change_flag	ARGS((enum sh_flag f, int what, int newval));
int	parse_args	ARGS((char **argv, int what, int *setargsp));
int 	getn		ARGS((const char *as, int *ai));
int 	bi_getn		ARGS((const char *as, int *ai));
char *	strerror	ARGS((int i));
int 	gmatch		ARGS((const char *s, const char *p, int isfile));
int	has_globbing	ARGS((const char *xp, const char *xpe));
const unsigned char *pat_scan ARGS((const unsigned char *p,
				const unsigned char *pe, int match_sep));
void 	qsortp		ARGS((void **base, size_t n, int (*f)(void *, void *)));
int 	xstrcmp		ARGS((void *p1, void *p2));
void	ksh_getopt_reset ARGS((Getopt *go, int));
int	ksh_getopt	ARGS((char **argv, Getopt *go, const char *options));
void	print_value_quoted ARGS((const char *s));
void	print_columns	ARGS((struct shf *shf, int n,
			      char *(*func)(void *, int, char *, int),
			      void *arg, int max_width));
int	strip_nuls	ARGS((char *buf, int nbytes));
char	*str_zcpy	ARGS((char *dst, const char *src, int dsize));
int	blocking_read	ARGS((int fd, char *buf, int nbytes));
int	reset_nonblock	ARGS((int fd));
char	*ksh_get_wd	ARGS((char *buf, int bsize));
/* path.c */
int	make_path	ARGS((const char *cwd, const char *file,
			      char **pathlist, XString *xsp, int *phys_pathp));
void	simplify_path	ARGS((char *path));
char	*get_phys_path	ARGS((const char *path));
void	set_current_wd	ARGS((char *path));
/* syn.c */
void 	initkeywords	ARGS((void));
struct op * compile	ARGS((Source *s));
/* table.c */
unsigned int 	hash	ARGS((const char *n));
void 	tinit		ARGS((struct table *tp, Area *ap, int tsize));
struct tbl *	tsearch	ARGS((struct table *tp, const char *n, unsigned int h));
struct tbl *	tenter	ARGS((struct table *tp, const char *n, unsigned int h));
void 	tdelete		ARGS((struct tbl *p));
void 	twalk		ARGS((struct tstate *ts, struct table *tp));
struct tbl *	tnext	ARGS((struct tstate *ts));
struct tbl **	tsort	ARGS((struct table *tp));
/* trace.c */
/* trap.c */
void	inittraps	ARGS((void));
#ifdef KSH
void	alarm_init	ARGS((void));
#endif /* KSH */
Trap *	gettrap		ARGS((const char *name));
RETSIGTYPE trapsig	ARGS((int i));
void	intrcheck	ARGS((void));
int	fatal_trap_check ARGS((void));
int	trap_pending	ARGS((void));
void 	runtraps	ARGS((int intr));
void 	runtrap		ARGS((Trap *p));
void 	cleartraps	ARGS((void));
void 	restoresigs	ARGS((void));
void	settrap		ARGS((Trap *p, char *s));
int	block_pipe	ARGS((void));
void	restore_pipe	ARGS((int restore_dfl));
int	setsig		ARGS((Trap *p, handler_t f, int flags));
void	setexecsig	ARGS((Trap *p, int restore));
/* tree.c */
int 	fptreef		ARGS((struct shf *f, int indent, const char *fmt, ...));
char *	snptreef	ARGS((char *s, int n, const char *fmt, ...));
struct op *	tcopy	ARGS((struct op *t, Area *ap));
char *	wdcopy		ARGS((const char *wp, Area *ap));
char *	wdscan		ARGS((const char *wp, int c));
void 	tfree		ARGS((struct op *t, Area *ap));
/* var.c */
void 	newblock	ARGS((void));
void 	popblock	ARGS((void));
void	initvar		ARGS((void));
struct tbl *	global	ARGS((const char *n));
struct tbl *	local	ARGS((const char *n, bool_t copy));
char *	str_val		ARGS((struct tbl *vp));
long 	intval		ARGS((struct tbl *vp));
void 	setstr		ARGS((struct tbl *vq, const char *s));
struct tbl *setint_v	ARGS((struct tbl *vq, struct tbl *vp));
void 	setint		ARGS((struct tbl *vq, long n));
int	getint		ARGS((struct tbl *vp, long *nump));
struct tbl *	typeset	ARGS((const char *var, Tflag set, Tflag clr, int field, int base));
void 	unset		ARGS((struct tbl *vp, int array_ref));
char  * skip_varname	ARGS((const char *s, int aok));
char	*skip_wdvarname ARGS((const char *s, int aok));
int	is_wdvarname	ARGS((const char *s, int aok));
int	is_wdvarassign	ARGS((const char *s));
char **	makenv		ARGS((void));
int	array_ref_len	ARGS((const char *cp));
char *	arrayname	ARGS((const char *str));
void    set_array	ARGS((const char *var, int reset, char **vals));
/* version.c */
/* vi.c: see edit.h */


/* Hack to avoid billions of compile warnings on SunOS 4.1.x */
#if defined(MUN) && defined(sun) && !defined(__svr4__)
extern void bcopy ARGS((const void *src, void *dst, size_t size));
extern int fclose ARGS((FILE *fp));
extern int fprintf ARGS((FILE *fp, const char *fmt, ...));
extern int fread ARGS((void *buf, int size, int num, FILE *fp));
extern int ioctl ARGS((int fd, int request, void *arg));
extern int killpg ARGS((int pgrp, int sig));
extern int nice ARGS((int n));
extern int readlink ARGS((const char *path, char *buf, int bufsize));
extern int setpgrp ARGS((int pid, int pgrp));
extern int strcasecmp ARGS((const char *s1, const char *s2));
extern int tolower ARGS((int));
extern int toupper ARGS((int));
/*  Include files aren't included yet */
extern int getrlimit ARGS(( /* int resource, struct rlimit *rpl */ ));
extern int getrusage ARGS(( /* int who, struct rusage *rusage */ ));
extern int gettimeofday ARGS(( /* struct timeval *tv, struct timezone *tz */ ));
extern int setrlimit ARGS(( /* int resource, struct rlimit *rlp */ ));
extern int lstat ARGS(( /* const char *path, struct stat *buf */ ));
#endif
