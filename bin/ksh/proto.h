/*	$OpenBSD: proto.h,v 1.19 2004/12/18 22:42:26 millert Exp $	*/

/*
 * prototypes for PD-KSH
 * originally generated using "cproto.c 3.5 92/04/11 19:28:01 cthuang "
 * $From: proto.h,v 1.3 1994/05/19 18:32:40 michael Exp michael $
 */

/* alloc.c */
Area *	ainit(Area *ap);
void 	afreeall(Area *ap);
void *	alloc(size_t size, Area *ap);
void *	aresize(void *ptr, size_t size, Area *ap);
void 	afree(void *ptr, Area *ap);
/* c_ksh.c */
int 	c_hash(char **wp);
int 	c_cd(char **wp);
int 	c_pwd(char **wp);
int 	c_print(char **wp);
int 	c_whence(char **wp);
int 	c_command(char **wp);
int 	c_typeset(char **wp);
int 	c_alias(char **wp);
int 	c_unalias(char **wp);
int 	c_let(char **wp);
int 	c_jobs(char **wp);
int 	c_fgbg(char **wp);
int 	c_kill(char **wp);
void	getopts_reset(int val);
int	c_getopts(char **wp);
int 	c_bind(char **wp);
/* c_sh.c */
int 	c_label(char **wp);
int 	c_shift(char **wp);
int 	c_umask(char **wp);
int 	c_dot(char **wp);
int 	c_wait(char **wp);
int 	c_read(char **wp);
int 	c_eval(char **wp);
int 	c_trap(char **wp);
int 	c_brkcont(char **wp);
int 	c_exitreturn(char **wp);
int 	c_set(char **wp);
int 	c_unset(char **wp);
int 	c_ulimit(char **wp);
int 	c_times(char **wp);
int 	timex(struct op *t, int f);
void	timex_hook(struct op *t, char ** volatile *app);
int 	c_exec(char **wp);
int 	c_builtin(char **wp);
/* c_test.c */
int 	c_test(char **wp);
/* edit.c: most prototypes in edit.h */
void 	x_init(void);
int 	x_read(char *buf, size_t len);
void	set_editmode(const char *ed);
/* emacs.c: most prototypes in edit.h */
int 	x_bind(const char *a1, const char *a2, int macro, int list);
/* eval.c */
char *	substitute(const char *cp, int f);
char **	eval(char **ap, int f);
char *	evalstr(char *cp, int f);
char *	evalonestr(char *cp, int f);
char	*debunk(char *dp, const char *sp, size_t dlen);
void	expand(char *cp, XPtrV *wp, int f);
int	glob_str(char *cp, XPtrV *wp, int markdirs);
/* exec.c */
int 	execute(struct op * volatile t, volatile int flags);
int 	shcomexec(char **wp);
struct tbl * findfunc(const char *name, unsigned int h, int create);
int 	define(const char *name, struct op *t);
void 	builtin(const char *name, int (*func)(char **));
struct tbl *	findcom(const char *name, int flags);
void 	flushcom(int all);
char *	search(const char *name, const char *path, int mode, int *errnop);
int	search_access(const char *path, int mode, int *errnop);
int	pr_menu(char *const *ap);
int	pr_list(char *const *ap);
/* expr.c */
int 	evaluate(const char *expr, long *rval, int error_ok);
int	v_evaluate(struct tbl *vp, const char *expr, volatile int error_ok);
/* history.c */
void	init_histvec(void);
void 	hist_init(Source *s);
void 	hist_finish(void);
void	histsave(int lno, const char *cmd, int dowrite);
#ifdef HISTORY
int 	c_fc(char **wp);
void	sethistsize(int n);
void	sethistfile(const char *name);
char **	histpos(void);
int 	histN(void);
int 	histnum(int n);
int	findhist(int start, int fwd, const char *str, int anchored);
int	findhistrel(const char *str);
char  **hist_get_newest(int allow_cur);

#endif /* HISTORY */
/* io.c */
void 	errorf(const char *fmt, ...)
	    __attribute__((__noreturn__, __format__ (printf, 1, 2)));
void 	warningf(int fileline, const char *fmt, ...)
	    __attribute__((__format__ (printf, 2, 3)));
void 	bi_errorf(const char *fmt, ...)
	    __attribute__((__format__ (printf, 1, 2)));
void 	internal_errorf(int jump, const char *fmt, ...)
	    __attribute__((__format__ (printf, 2, 3)));
void	error_prefix(int fileline);
void 	shellf(const char *fmt, ...)
	    __attribute__((__format__ (printf, 1, 2)));
void 	shprintf(const char *fmt, ...)
	    __attribute__((__format__ (printf, 1, 2)));
#ifdef KSH_DEBUG
void 	kshdebug_init_(void);
void 	kshdebug_printf_(const char *fmt, ...)
	    __attribute__((__format__ (printf, 1, 2)));
void 	kshdebug_dump_(const char *str, const void *mem, int nbytes);
#endif /* KSH_DEBUG */
int	can_seek(int fd);
void	initio(void);
int	ksh_dup2(int ofd, int nfd, int errok);
int 	savefd(int fd, int noclose);
void 	restfd(int fd, int ofd);
void 	openpipe(int *pv);
void 	closepipe(int *pv);
int	check_fd(char *name, int mode, const char **emsgp);
void	coproc_init(void);
void	coproc_read_close(int fd);
void	coproc_readw_close(int fd);
void	coproc_write_close(int fd);
int	coproc_getfd(int mode, const char **emsgp);
void	coproc_cleanup(int reuse);
struct temp *maketemp(Area *ap, Temp_type type, struct temp **tlist);
/* jobs.c */
void 	j_init(int mflagset);
void 	j_exit(void);
void 	j_change(void);
int 	exchild(struct op *t, int flags, int close_fd);
void 	startlast(void);
int 	waitlast(void);
int 	waitfor(const char *cp, int *sigp);
int 	j_kill(const char *cp, int sig);
int 	j_resume(const char *cp, int bg);
int 	j_jobs(const char *cp, int slp, int nflag);
int	j_njobs(void);
void 	j_notify(void);
pid_t	j_async(void);
int 	j_stopped_running(void);
/* lex.c */
int 	yylex(int cf);
void 	yyerror(const char *fmt, ...)
	    __attribute__((__noreturn__, __format__ (printf, 1, 2)));
Source * pushs(int type, Area *areap);
void	set_prompt(int to, Source *s);
void 	pprompt(const char *cp, int ntruncate);
/* mail.c */
void 	mcheck(void);
void 	mcset(long interval);
void 	mbset(char *p);
void 	mpset(char *mptoparse);
/* main.c */
int 	include(const char *name, int argc, char **argv, int intr_ok);
int 	command(const char *comm);
int 	shell(Source *volatile s, int volatile toplevel);
void 	unwind(int i) __attribute__((__noreturn__));
void 	newenv(int type);
void 	quitenv(void);
void	cleanup_parents_env(void);
void	cleanup_proc_env(void);
void 	aerror(Area *ap, const char *msg)
	    __attribute__((__noreturn__));
/* misc.c */
void 	setctypes(const char *s, int t);
void 	initctypes(void);
char *	ulton(unsigned long n, int base);
char *	str_save(const char *s, Area *ap);
char *	str_nsave(const char *s, int n, Area *ap);
int	option(const char *n);
char *	getoptions(void);
void	change_flag(enum sh_flag f, int what, int newval);
int	parse_args(char **argv, int what, int *setargsp);
int 	getn(const char *as, int *ai);
int 	bi_getn(const char *as, int *ai);
int 	gmatch(const char *s, const char *p, int isfile);
int	has_globbing(const char *xp, const char *xpe);
const unsigned char *pat_scan(const unsigned char *p,
				const unsigned char *pe, int match_sep);
void 	qsortp(void **base, size_t n, int (*f)(void *, void *));
int 	xstrcmp(void *p1, void *p2);
void	ksh_getopt_reset(Getopt *go, int);
int	ksh_getopt(char **argv, Getopt *go, const char *options);
void	print_value_quoted(const char *s);
void	print_columns(struct shf *shf, int n,
			  char *(*func)(void *, int, char *, int),
			  void *arg, int max_width, int prefcol);
int	strip_nuls(char *buf, int nbytes);
char	*str_zcpy(char *dst, const char *src, int dsize);
int	blocking_read(int fd, char *buf, int nbytes);
int	reset_nonblock(int fd);
char	*ksh_get_wd(char *buf, int bsize);
/* path.c */
int	make_path(const char *cwd, const char *file,
		      char **pathlist, XString *xsp, int *phys_pathp);
void	simplify_path(char *path);
char	*get_phys_path(const char *path);
void	set_current_wd(char *path);
/* syn.c */
void 	initkeywords(void);
struct op * compile(Source *s);
/* table.c */
unsigned int 	hash(const char *n);
void 		tinit(struct table *tp, Area *ap, int tsize);
struct tbl *	tsearch(struct table *tp, const char *n, unsigned int h);
struct tbl *	tenter(struct table *tp, const char *n, unsigned int h);
void 		tdelete(struct tbl *p);
void 		twalk(struct tstate *ts, struct table *tp);
struct tbl *	tnext(struct tstate *ts);
struct tbl **	tsort(struct table *tp);
/* trace.c */
/* trap.c */
void	inittraps(void);
void	alarm_init(void);
Trap *	gettrap(const char *name, int igncase);
void	trapsig(int i);
void	intrcheck(void);
int	fatal_trap_check(void);
int	trap_pending(void);
void 	runtraps(int intr);
void 	runtrap(Trap *p);
void 	cleartraps(void);
void 	restoresigs(void);
void	settrap(Trap *p, char *s);
int	block_pipe(void);
void	restore_pipe(int restore_dfl);
int	setsig(Trap *p, sig_t f, int flags);
void	setexecsig(Trap *p, int restore);
/* tree.c */
int 	fptreef(struct shf *f, int indent, const char *fmt, ...);
char *	snptreef(char *s, int n, const char *fmt, ...);
struct op *	tcopy(struct op *t, Area *ap);
char *	wdcopy(const char *wp, Area *ap);
char *	wdscan(const char *wp, int c);
char *	wdstrip(const char *wp);
void 	tfree(struct op *t, Area *ap);
/* var.c */
void 	newblock(void);
void 	popblock(void);
void	initvar(void);
struct tbl *	global(const char *n);
struct tbl *	local(const char *n, bool_t copy);
char *	str_val(struct tbl *vp);
long 	intval(struct tbl *vp);
int 	setstr(struct tbl *vq, const char *s, int error_ok);
struct tbl *setint_v(struct tbl *vq, struct tbl *vp);
void 	setint(struct tbl *vq, long n);
int	getint(struct tbl *vp, long *nump);
struct tbl *	typeset(const char *var, Tflag set, Tflag clr, int field, int base);
void 	unset(struct tbl *vp, int array_ref);
char  * skip_varname(const char *s, int aok);
char	*skip_wdvarname(const char *s, int aok);
int	is_wdvarname(const char *s, int aok);
int	is_wdvarassign(const char *s);
char **	makenv(void);
void	change_random(void);
int	array_ref_len(const char *cp);
char *	arrayname(const char *str);
void    set_array(const char *var, int reset, char **vals);
/* version.c */
/* vi.c: see edit.h */
