/*	$OpenBSD: proto.h,v 1.38 2015/10/10 07:35:16 nicm Exp $	*/

/*
 * prototypes for PD-KSH
 * originally generated using "cproto.c 3.5 92/04/11 19:28:01 cthuang "
 * $From: proto.h,v 1.3 1994/05/19 18:32:40 michael Exp michael $
 */

/* alloc.c */
Area *	ainit(Area *);
void	afreeall(Area *);
void *	alloc(size_t, Area *);
void *	aresize(void *, size_t, Area *);
void	afree(void *, Area *);
/* c_ksh.c */
int	c_hash(char **);
int	c_cd(char **);
int	c_pwd(char **);
int	c_print(char **);
int	c_whence(char **);
int	c_command(char **);
int	c_typeset(char **);
int	c_alias(char **);
int	c_unalias(char **);
int	c_let(char **);
int	c_jobs(char **);
int	c_fgbg(char **);
int	c_kill(char **);
void	getopts_reset(int);
int	c_getopts(char **);
int	c_bind(char **);
/* c_sh.c */
int	c_label(char **);
int	c_shift(char **);
int	c_umask(char **);
int	c_dot(char **);
int	c_wait(char **);
int	c_read(char **);
int	c_eval(char **);
int	c_trap(char **);
int	c_brkcont(char **);
int	c_exitreturn(char **);
int	c_set(char **);
int	c_unset(char **);
int	c_ulimit(char **);
int	c_times(char **);
int	timex(struct op *, int, volatile int *);
void	timex_hook(struct op *, char ** volatile *);
int	c_exec(char **);
int	c_builtin(char **);
/* c_test.c */
int	c_test(char **);
/* edit.c: most prototypes in edit.h */
void	x_init(void);
int	x_read(char *, size_t);
void	set_editmode(const char *);
/* emacs.c: most prototypes in edit.h */
int	x_bind(const char *, const char *, int, int);
/* eval.c */
char *	substitute(const char *, int);
char **	eval(char **, int);
char *	evalstr(char *cp, int);
char *	evalonestr(char *cp, int);
char	*debunk(char *, const char *, size_t);
void	expand(char *, XPtrV *, int);
int	glob_str(char *, XPtrV *, int);
/* exec.c */
int	execute(struct op * volatile, volatile int, volatile int *);
int	shcomexec(char **);
struct tbl * findfunc(const char *, unsigned int, int);
int	define(const char *, struct op *);
void	builtin(const char *, int (*)(char **));
struct tbl *	findcom(const char *, int);
void	flushcom(int);
char *	search(const char *, const char *, int, int *);
int	search_access(const char *, int, int *);
int	pr_menu(char *const *);
int	pr_list(char *const *);
/* expr.c */
int	evaluate(const char *, long *, int, bool);
int	v_evaluate(struct tbl *, const char *, volatile int, bool);
/* history.c */
void	init_histvec(void);
void	hist_init(Source *);
void	hist_finish(void);
void	histsave(int, const char *, int);
#ifdef HISTORY
int	c_fc(char **);
void	sethistsize(int);
void	sethistfile(const char *);
char **	histpos(void);
int	histnum(int);
int	findhist(int, int, const char *, int);
int	findhistrel(const char *);
char  **hist_get_newest(int);

#endif /* HISTORY */
/* io.c */
void	errorf(const char *, ...)
	    __attribute__((__noreturn__, __format__ (printf, 1, 2)));
void	warningf(int, const char *, ...)
	    __attribute__((__format__ (printf, 2, 3)));
void	bi_errorf(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));
void	internal_errorf(int, const char *, ...)
	    __attribute__((__format__ (printf, 2, 3)));
void	error_prefix(int);
void	shellf(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));
void	shprintf(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));
#ifdef KSH_DEBUG
void	kshdebug_init_(void);
void	kshdebug_printf_(const char *, ...)
	    __attribute__((__format__ (printf, 1, 2)));
void	kshdebug_dump_(const char *, const void *, int);
#endif /* KSH_DEBUG */
int	can_seek(int);
void	initio(void);
int	ksh_dup2(int, int, int);
int	savefd(int);
void	restfd(int, int);
void	openpipe(int *);
void	closepipe(int *);
int	check_fd(char *, int, const char **);
void	coproc_init(void);
void	coproc_read_close(int);
void	coproc_readw_close(int);
void	coproc_write_close(int);
int	coproc_getfd(int, const char **);
void	coproc_cleanup(int);
struct temp *maketemp(Area *, Temp_type, struct temp **);
/* jobs.c */
void	j_init(int);
void	j_suspend(void);
void	j_exit(void);
void	j_change(void);
int	exchild(struct op *, int, volatile int *, int);
void	startlast(void);
int	waitlast(void);
int	waitfor(const char *, int *);
int	j_kill(const char *, int);
int	j_resume(const char *, int);
int	j_jobs(const char *, int, int);
int	j_njobs(void);
void	j_notify(void);
pid_t	j_async(void);
int	j_stopped_running(void);
/* mail.c */
void	mcheck(void);
void	mcset(long);
void	mbset(char *);
void	mpset(char *);
/* main.c */
int	include(const char *, int, char **, int);
int	command(const char *, int);
int	shell(Source *volatile, int volatile);
void	unwind(int) __attribute__((__noreturn__));
void	newenv(int);
void	quitenv(struct shf *);
void	cleanup_parents_env(void);
void	cleanup_proc_env(void);
/* misc.c */
void	setctypes(const char *, int);
void	initctypes(void);
char *	ulton(unsigned long, int);
char *	str_save(const char *, Area *);
char *	str_nsave(const char *, int, Area *);
int	option(const char *);
char *	getoptions(void);
void	change_flag(enum sh_flag, int, int);
int	parse_args(char **, int, int *);
int	getn(const char *, int *);
int	bi_getn(const char *, int *);
int	gmatch(const char *, const char *, int);
int	has_globbing(const char *, const char *);
const unsigned char *pat_scan(const unsigned char *, const unsigned char *,
    int);
void	qsortp(void **, size_t, int (*)(const void *, const void *));
int	xstrcmp(const void *, const void *);
void	ksh_getopt_reset(Getopt *, int);
int	ksh_getopt(char **, Getopt *, const char *);
void	print_value_quoted(const char *);
void	print_columns(struct shf *, int, char *(*)(void *, int, char *, int),
    void *, int, int prefcol);
int	strip_nuls(char *, int);
int	blocking_read(int, char *, int);
int	reset_nonblock(int);
char	*ksh_get_wd(char *, int);
/* mknod.c */
int domknod(int, char **, mode_t);
int domkfifo(int, char **, mode_t);
/* path.c */
int	make_path(const char *, const char *, char **, XString *, int *);
void	simplify_path(char *);
char	*get_phys_path(const char *);
void	set_current_wd(char *);
/* syn.c */
void	initkeywords(void);
struct op * compile(Source *);
/* trace.c */
/* trap.c */
void	inittraps(void);
void	alarm_init(void);
Trap *	gettrap(const char *, int);
void	trapsig(int);
void	intrcheck(void);
int	fatal_trap_check(void);
int	trap_pending(void);
void	runtraps(int intr);
void	runtrap(Trap *);
void	cleartraps(void);
void	restoresigs(void);
void	settrap(Trap *, char *);
int	block_pipe(void);
void	restore_pipe(int);
int	setsig(Trap *, sig_t, int);
void	setexecsig(Trap *, int);
/* var.c */
void	newblock(void);
void	popblock(void);
void	initvar(void);
struct tbl *	global(const char *);
struct tbl *	local(const char *, bool);
char *	str_val(struct tbl *);
long	intval(struct tbl *);
int	setstr(struct tbl *, const char *, int);
struct tbl *setint_v(struct tbl *, struct tbl *, bool);
void	setint(struct tbl *, long);
int	getint(struct tbl *, long *, bool);
struct tbl *typeset(const char *, int, int, int, int);
void	unset(struct tbl *, int);
char  * skip_varname(const char *, int);
char	*skip_wdvarname(const char *, int);
int	is_wdvarname(const char *, int);
int	is_wdvarassign(const char *);
char **	makenv(void);
void	change_random(void);
int	array_ref_len(const char *);
char *	arrayname(const char *);
void    set_array(const char *, int, char **);
/* version.c */
/* vi.c: see edit.h */
