#ifndef NEXT30_NO_ATTRIBUTE
#ifndef HASATTRIBUTE       /* disable GNU-cc attribute checking? */
#ifdef  __attribute__      /* Avoid possible redefinition errors */
#undef  __attribute__
#endif
#define __attribute__(attr)
#endif 
#endif
#ifdef OVERLOAD
SV*	amagic_call _((SV* left,SV* right,int method,int dir));
bool	Gv_AMupdate _((HV* stash));
#endif /* OVERLOAD */
OP*	append_elem _((I32 optype, OP* head, OP* tail));
OP*	append_list _((I32 optype, LISTOP* first, LISTOP* last));
I32	apply _((I32 type, SV** mark, SV** sp));
void	assertref _((OP* op));
void	av_clear _((AV* ar));
void	av_extend _((AV* ar, I32 key));
AV*	av_fake _((I32 size, SV** svp));
SV**	av_fetch _((AV* ar, I32 key, I32 lval));
void	av_fill _((AV* ar, I32 fill));
I32	av_len _((AV* ar));
AV*	av_make _((I32 size, SV** svp));
SV*	av_pop _((AV* ar));
void	av_push _((AV* ar, SV* val));
void	av_reify _((AV* ar));
SV*	av_shift _((AV* ar));
SV**	av_store _((AV* ar, I32 key, SV* val));
void	av_undef _((AV* ar));
void	av_unshift _((AV* ar, I32 num));
OP*	bind_match _((I32 type, OP* left, OP* pat));
OP*	block_end _((I32 floor, OP* seq));
I32	block_gimme _((void));
int	block_start _((int full));
void	boot_core_UNIVERSAL _((void));
void	call_list _((I32 oldscope, AV* list));
I32	cando _((I32 bit, I32 effective, struct stat* statbufp));
#ifndef CASTNEGFLOAT
U32	cast_ulong _((double f));
#endif
#if !defined(HAS_TRUNCATE) && !defined(HAS_CHSIZE) && defined(F_FREESP)
I32	my_chsize _((int fd, Off_t length));
#endif
OP*	ck_gvconst _((OP*  o));
OP*	ck_retarget _((OP* op));
OP*	convert _((I32 optype, I32 flags, OP* op));
void	croak _((const char* pat,...)) __attribute__((noreturn));
void	cv_ckproto _((CV* cv, GV* gv, char* p));
CV*	cv_clone _((CV* proto));
SV*	cv_const_sv _((CV* cv));
void	cv_undef _((CV* cv));
#ifdef DEBUGGING
void	cx_dump _((CONTEXT* cs));
#endif
SV*	filter_add _((filter_t funcp, SV* datasv));
void	filter_del _((filter_t funcp));
I32	filter_read _((int idx, SV* buffer, int maxlen));
I32	cxinc _((void));
void	deb _((const char* pat,...)) __attribute__((format(printf,1,2)));
void	deb_growlevel _((void));
I32	debop _((OP* op));
I32	debstackptrs _((void));
#ifdef DEBUGGING
void	debprofdump _((void));
#endif
I32	debstack _((void));
char*	delimcpy _((char* to, char* toend, char* from, char* fromend,
		    int delim, I32* retlen));
void	deprecate _((char* s));
OP*	die _((const char* pat,...));
OP*	die_where _((char* message));
void	dounwind _((I32 cxix));
bool	do_aexec _((SV* really, SV** mark, SV** sp));
void    do_chop _((SV* asv, SV* sv));
bool	do_close _((GV* gv, bool not_implicit));
bool	do_eof _((GV* gv));
bool	do_exec _((char* cmd));
void	do_execfree _((void));
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
I32	do_ipcctl _((I32 optype, SV** mark, SV** sp));
I32	do_ipcget _((I32 optype, SV** mark, SV** sp));
#endif
void	do_join _((SV* sv, SV* del, SV** mark, SV** sp));
OP*	do_kv _((void));
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
I32	do_msgrcv _((SV** mark, SV** sp));
I32	do_msgsnd _((SV** mark, SV** sp));
#endif
bool	do_open _((GV* gv, char* name, I32 len,
		   int as_raw, int rawmode, int rawperm, PerlIO* supplied_fp));
void	do_pipe _((SV* sv, GV* rgv, GV* wgv));
bool	do_print _((SV* sv, PerlIO* fp));
OP*	do_readline _((void));
I32	do_chomp _((SV* sv));
bool	do_seek _((GV* gv, long pos, int whence));
#if defined(HAS_MSG) || defined(HAS_SEM) || defined(HAS_SHM)
I32	do_semop _((SV** mark, SV** sp));
I32	do_shmio _((I32 optype, SV** mark, SV** sp));
#endif
void	do_sprintf _((SV* sv, I32 len, SV** sarg));
long	do_sysseek _((GV* gv, long pos, int whence));
long	do_tell _((GV* gv));
I32	do_trans _((SV* sv, OP* arg));
void	do_vecset _((SV* sv));
void	do_vop _((I32 optype, SV* sv, SV* left, SV* right));
I32	dowantarray _((void));
void	dump_all _((void));
void	dump_eval _((void));
#ifdef DUMP_FDS  /* See util.c */
int	dump_fds _((char* s));
#endif
void	dump_form _((GV* gv));
void	dump_gv _((GV* gv));
#ifdef MYMALLOC
void	dump_mstats _((char* s));
#endif
void	dump_op _((OP* arg));
void	dump_pm _((PMOP* pm));
void	dump_packsubs _((HV* stash));
void	dump_sub _((GV* gv));
void	fbm_compile _((SV* sv));
char*	fbm_instr _((unsigned char* big, unsigned char* bigend, SV* littlesv));
OP*	force_list _((OP* arg));
OP*	fold_constants _((OP* arg));
char*	form _((const char* pat, ...));
void	free_tmps _((void));
OP*	gen_constant_list _((OP* op));
void	gp_free _((GV* gv));
GP*	gp_ref _((GP* gp));
GV*	gv_AVadd _((GV* gv));
GV*	gv_HVadd _((GV* gv));
GV*	gv_IOadd _((GV* gv));
GV*	gv_autoload4 _((HV* stash, char* name, STRLEN len, I32 method));
void	gv_check _((HV* stash));
void	gv_efullname _((SV* sv, GV* gv));
void	gv_efullname3 _((SV* sv, GV* gv, char* prefix));
GV*	gv_fetchfile _((char* name));
GV*	gv_fetchmeth _((HV* stash, char* name, STRLEN len, I32 level));
GV*	gv_fetchmethod _((HV* stash, char* name));
GV*	gv_fetchmethod_autoload _((HV* stash, char* name, I32 autoload));
GV*	gv_fetchpv _((char* name, I32 add, I32 sv_type));
void	gv_fullname _((SV* sv, GV* gv));
void	gv_fullname3 _((SV* sv, GV* gv, char* prefix));
void	gv_init _((GV* gv, HV* stash, char* name, STRLEN len, int multi));
HV*	gv_stashpv _((char* name, I32 create));
HV*	gv_stashpvn _((char* name, U32 namelen, I32 create));
HV*	gv_stashsv _((SV* sv, I32 create));
void	hoistmust _((PMOP* pm));
void	hv_clear _((HV* tb));
void	hv_delayfree_ent _((HV* hv, HE* entry));
SV*	hv_delete _((HV* tb, char* key, U32 klen, I32 flags));
SV*	hv_delete_ent _((HV* tb, SV* key, I32 flags, U32 hash));
bool	hv_exists _((HV* tb, char* key, U32 klen));
bool	hv_exists_ent _((HV* tb, SV* key, U32 hash));
SV**	hv_fetch _((HV* tb, char* key, U32 klen, I32 lval));
HE*	hv_fetch_ent _((HV* tb, SV* key, I32 lval, U32 hash));
void	hv_free_ent _((HV* hv, HE* entry));
I32	hv_iterinit _((HV* tb));
char*	hv_iterkey _((HE* entry, I32* retlen));
SV*	hv_iterkeysv _((HE* entry));
HE*	hv_iternext _((HV* tb));
SV*	hv_iternextsv _((HV* hv, char** key, I32* retlen));
SV*	hv_iterval _((HV* tb, HE* entry));
void	hv_ksplit _((HV* hv, IV newmax));
void	hv_magic _((HV* hv, GV* gv, int how));
SV**	hv_store _((HV* tb, char* key, U32 klen, SV* val, U32 hash));
HE*	hv_store_ent _((HV* tb, SV* key, SV* val, U32 hash));
void	hv_undef _((HV* tb));
I32	ibcmp _((char* a, char* b, I32 len));
I32	ibcmp_locale _((char* a, char* b, I32 len));
I32	ingroup _((I32 testgid, I32 effective));
U32	intro_my _((void));
char*	instr _((char* big, char* little));
bool	io_close _((IO* io));
OP*	invert _((OP* cmd));
OP*	jmaybe _((OP* arg));
I32	keyword _((char* d, I32 len));
void	leave_scope _((I32 base));
void	lex_end _((void));
void	lex_start _((SV* line));
OP*	linklist _((OP* op));
OP*	list _((OP* o));
OP*	listkids _((OP* o));
OP*	localize _((OP* arg, I32 lexical));
I32	looks_like_number _((SV* sv));
int	magic_clearenv	_((SV* sv, MAGIC* mg));
int	magic_clear_all_env _((SV* sv, MAGIC* mg));
int	magic_clearpack	_((SV* sv, MAGIC* mg));
int	magic_clearsig	_((SV* sv, MAGIC* mg));
int	magic_existspack _((SV* sv, MAGIC* mg));
int	magic_freedefelem _((SV* sv, MAGIC* mg));
int	magic_get	_((SV* sv, MAGIC* mg));
int	magic_getarylen	_((SV* sv, MAGIC* mg));
int	magic_getdefelem _((SV* sv, MAGIC* mg));
int	magic_getglob	_((SV* sv, MAGIC* mg));
int	magic_getpack	_((SV* sv, MAGIC* mg));
int	magic_getpos	_((SV* sv, MAGIC* mg));
int	magic_getsig	_((SV* sv, MAGIC* mg));
int	magic_gettaint	_((SV* sv, MAGIC* mg));
int	magic_getuvar	_((SV* sv, MAGIC* mg));
U32	magic_len	_((SV* sv, MAGIC* mg));
int	magic_nextpack	_((SV* sv, MAGIC* mg, SV* key));
int	magic_set	_((SV* sv, MAGIC* mg));
#ifdef OVERLOAD
int	magic_setamagic	_((SV* sv, MAGIC* mg));
#endif /* OVERLOAD */
int	magic_setarylen	_((SV* sv, MAGIC* mg));
int	magic_setbm	_((SV* sv, MAGIC* mg));
int	magic_setdbline	_((SV* sv, MAGIC* mg));
#ifdef USE_LOCALE_COLLATE
int	magic_setcollxfrm _((SV* sv, MAGIC* mg));
#endif
int	magic_setdefelem _((SV* sv, MAGIC* mg));
int	magic_setenv	_((SV* sv, MAGIC* mg));
int	magic_setfm	_((SV* sv, MAGIC* mg));
int	magic_setisa	_((SV* sv, MAGIC* mg));
int	magic_setglob	_((SV* sv, MAGIC* mg));
int	magic_setmglob	_((SV* sv, MAGIC* mg));
int	magic_setnkeys	_((SV* sv, MAGIC* mg));
int	magic_setpack	_((SV* sv, MAGIC* mg));
int	magic_setpos	_((SV* sv, MAGIC* mg));
int	magic_setsig	_((SV* sv, MAGIC* mg));
int	magic_setsubstr	_((SV* sv, MAGIC* mg));
int	magic_settaint	_((SV* sv, MAGIC* mg));
int	magic_setuvar	_((SV* sv, MAGIC* mg));
int	magic_setvec	_((SV* sv, MAGIC* mg));
int	magic_set_all_env _((SV* sv, MAGIC* mg));
int	magic_wipepack	_((SV* sv, MAGIC* mg));
void	magicname _((char* sym, char* name, I32 namlen));
int	main _((int argc, char** argv, char** env));
void	markstack_grow _((void));
#ifdef USE_LOCALE_COLLATE
char*	mem_collxfrm _((const char* s, STRLEN len, STRLEN* xlen));
#endif
char*	mess _((const char* pat, va_list* args));
int	mg_clear _((SV* sv));
int	mg_copy _((SV* , SV* , char* , I32));
MAGIC*	mg_find _((SV* sv, int type));
int	mg_free _((SV* sv));
int	mg_get _((SV* sv));
U32	mg_len _((SV* sv));
void	mg_magical _((SV* sv));
int	mg_set _((SV* sv));
OP*	mod _((OP* op, I32 type));
char*	moreswitches _((char* s));
OP*	my _((OP* op));
#if !defined(HAS_BCOPY) || !defined(HAS_SAFE_BCOPY)
char*	my_bcopy _((char* from, char* to, I32 len));
#endif
#if !defined(HAS_BZERO) && !defined(HAS_MEMSET)
char*	my_bzero _((char* loc, I32 len));
#endif
void	my_exit _((U32 status)) __attribute__((noreturn));
void	my_failure_exit _((void)) __attribute__((noreturn));
I32	my_lstat _((void));
#if !defined(HAS_MEMCMP) || !defined(HAS_SANE_MEMCMP)
I32	my_memcmp _((char* s1, char* s2, I32 len));
#endif
#if !defined(HAS_MEMSET)
void*	my_memset _((char* loc, I32 ch, I32 len));
#endif
I32	my_pclose _((PerlIO* ptr));
PerlIO*	my_popen _((char* cmd, char* mode));
void	my_setenv _((char* nam, char* val));
I32	my_stat _((void));
#ifdef MYSWAP
short	my_swap _((short s));
long	my_htonl _((long l));
long	my_ntohl _((long l));
#endif
void	my_unexec _((void));
OP*	newANONLIST _((OP* op));
OP*	newANONHASH _((OP* op));
OP*	newANONSUB _((I32 floor, OP* proto, OP* block));
OP*	newASSIGNOP _((I32 flags, OP* left, I32 optype, OP* right));
OP*	newCONDOP _((I32 flags, OP* expr, OP* trueop, OP* falseop));
void	newFORM _((I32 floor, OP* op, OP* block));
OP*	newFOROP _((I32 flags, char* label, line_t forline, OP* scalar, OP* expr, OP*block, OP*cont));
OP*	newLOGOP _((I32 optype, I32 flags, OP* left, OP* right));
OP*	newLOOPEX _((I32 type, OP* label));
OP*	newLOOPOP _((I32 flags, I32 debuggable, OP* expr, OP* block));
OP*	newNULLLIST _((void));
OP*	newOP _((I32 optype, I32 flags));
void	newPROG _((OP* op));
OP*	newRANGE _((I32 flags, OP* left, OP* right));
OP*	newSLICEOP _((I32 flags, OP* subscript, OP* list));
OP*	newSTATEOP _((I32 flags, char* label, OP* o));
CV*	newSUB _((I32 floor, OP* op, OP* proto, OP* block));
CV*	newXS _((char* name, void (*subaddr)(CV* cv), char* filename));
#ifdef DEPRECATED
CV*	newXSUB _((char* name, I32 ix, I32 (*subaddr)(int,int,int), char* filename));
#endif
AV*	newAV _((void));
OP*	newAVREF _((OP* o));
OP*	newBINOP _((I32 type, I32 flags, OP* first, OP* last));
OP*	newCVREF _((I32 flags, OP* o));
OP*	newGVOP _((I32 type, I32 flags, GV* gv));
GV*	newGVgen _((char* pack));
OP*	newGVREF _((I32 type, OP* o));
OP*	newHVREF _((OP* o));
HV*	newHV _((void));
IO*	newIO _((void));
OP*	newLISTOP _((I32 type, I32 flags, OP* first, OP* last));
OP*	newPMOP _((I32 type, I32 flags));
OP*	newPVOP _((I32 type, I32 flags, char* pv));
SV*	newRV _((SV* ref));
#ifdef LEAKTEST
SV*	newSV _((I32 x, STRLEN len));
#else
SV*	newSV _((STRLEN len));
#endif
OP*	newSVREF _((OP* o));
OP*	newSVOP _((I32 type, I32 flags, SV* sv));
SV*	newSViv _((IV i));
SV*	newSVnv _((double n));
SV*	newSVpv _((char* s, STRLEN len));
SV*	newSVpvf _((const char* pat, ...));
SV*	newSVrv _((SV* rv, char* classname));
SV*	newSVsv _((SV* old));
OP*	newUNOP _((I32 type, I32 flags, OP* first));
OP*	newWHILEOP _((I32 flags, I32 debuggable, LOOP* loop,
		      I32 whileline, OP* expr, OP* block, OP* cont));
PerlIO*	nextargv _((GV* gv));
char*	ninstr _((char* big, char* bigend, char* little, char* lend));
OP*	oopsCV _((OP* o));
void	op_free _((OP* arg));
void	package _((OP* op));
PADOFFSET	pad_alloc _((I32 optype, U32 tmptype));
PADOFFSET	pad_allocmy _((char* name));
PADOFFSET	pad_findmy _((char* name));
OP*	oopsAV _((OP* o));
OP*	oopsHV _((OP* o));
void	pad_leavemy _((I32 fill));
SV*	pad_sv _((PADOFFSET po));
void	pad_free _((PADOFFSET po));
void	pad_reset _((void));
void	pad_swipe _((PADOFFSET po));
void	peep _((OP* op));
PerlInterpreter*	perl_alloc _((void));
I32	perl_call_argv _((char* subname, I32 flags, char** argv));
I32	perl_call_method _((char* methname, I32 flags));
I32	perl_call_pv _((char* subname, I32 flags));
I32	perl_call_sv _((SV* sv, I32 flags));
void	perl_construct _((PerlInterpreter* sv_interp));
void	perl_destruct _((PerlInterpreter* sv_interp));
SV*	perl_eval_pv _((char* p, I32 croak_on_error));
I32	perl_eval_sv _((SV* sv, I32 flags));
void	perl_free _((PerlInterpreter* sv_interp));
SV*	perl_get_sv _((char* name, I32 create));
AV*	perl_get_av _((char* name, I32 create));
HV*	perl_get_hv _((char* name, I32 create));
CV*	perl_get_cv _((char* name, I32 create));
int	perl_init_i18nl10n _((int printwarn));
int	perl_init_i18nl14n _((int printwarn));
void	perl_new_collate _((char* newcoll));
void	perl_new_ctype _((char* newctype));
void	perl_new_numeric _((char* newcoll));
void	perl_set_numeric_local _((void));
void	perl_set_numeric_standard _((void));
int	perl_parse _((PerlInterpreter* sv_interp, void(*xsinit)(void), int argc, char** argv, char** env));
void	perl_require_pv _((char* pv));
#define perl_requirepv perl_require_pv
int	perl_run _((PerlInterpreter* sv_interp));
void	pidgone _((int pid, int status));
void	pmflag _((U16* pmfl, int ch));
OP*	pmruntime _((OP* pm, OP* expr, OP* repl));
OP*	pmtrans _((OP* op, OP* expr, OP* repl));
OP*	pop_return _((void));
void	pop_scope _((void));
OP*	prepend_elem _((I32 optype, OP* head, OP* tail));
void	push_return _((OP* op));
void	push_scope _((void));
regexp*	pregcomp _((char* exp, char* xend, PMOP* pm));
OP*	ref _((OP* op, I32 type));
OP*	refkids _((OP* op, I32 type));
void	regdump _((regexp* r));
I32	pregexec _((regexp* prog, char* stringarg, char* strend, char* strbeg, I32 minend, SV* screamer, I32 safebase));
void	pregfree _((struct regexp* r));
char*	regnext _((char* p));
void	regprop _((SV* sv, char* op));
void	repeatcpy _((char* to, char* from, I32 len, I32 count));
char*	rninstr _((char* big, char* bigend, char* little, char* lend));
Sighandler_t rsignal _((int, Sighandler_t));
int	rsignal_restore _((int, Sigsave_t*));
int	rsignal_save _((int, Sighandler_t, Sigsave_t*));
Sighandler_t rsignal_state _((int));
int	runops _((void));
void	rxres_free _((void** rsp));
void	rxres_restore _((void** rsp, REGEXP* rx));
void	rxres_save _((void** rsp, REGEXP* rx));
#ifndef HAS_RENAME
I32	same_dirent _((char* a, char* b));
#endif
char*	savepv _((char* sv));
char*	savepvn _((char* sv, I32 len));
void	savestack_grow _((void));
void	save_aptr _((AV** aptr));
AV*	save_ary _((GV* gv));
void	save_clearsv _((SV** svp));
void	save_delete _((HV* hv, char* key, I32 klen));
#ifndef titan  /* TitanOS cc can't handle this */
void	save_destructor _((void (*f)(void*), void* p));
#endif /* titan */
void	save_freesv _((SV* sv));
void	save_freeop _((OP* op));
void	save_freepv _((char* pv));
void	save_gp _((GV* gv, I32 empty));
HV*	save_hash _((GV* gv));
void	save_hptr _((HV** hptr));
void	save_I16 _((I16* intp));
void	save_I32 _((I32* intp));
void	save_int _((int* intp));
void	save_item _((SV* item));
void	save_iv _((IV* iv));
void	save_list _((SV** sarg, I32 maxsarg));
void	save_long _((long* longp));
void	save_nogv _((GV* gv));
SV*	save_scalar _((GV* gv));
void	save_pptr _((char** pptr));
void	save_sptr _((SV** sptr));
SV*	save_svref _((SV** sptr));
OP*	sawparens _((OP* o));
OP*	scalar _((OP* o));
OP*	scalarkids _((OP* op));
OP*	scalarseq _((OP* o));
OP*	scalarvoid _((OP* op));
UV	scan_hex _((char* start, I32 len, I32* retlen));
char*	scan_num _((char* s));
UV	scan_oct _((char* start, I32 len, I32* retlen));
OP*	scope _((OP* o));
char*	screaminstr _((SV* bigsv, SV* littlesv));
#ifndef VMS
I32	setenv_getix _((char* nam));
#endif
void	setdefout _((GV* gv));
char*	sharepvn _((char* sv, I32 len, U32 hash));
HEK*	share_hek _((char* sv, I32 len, U32 hash));
Signal_t sighandler _((int sig));
SV**	stack_grow _((SV** sp, SV**p, int n));
I32	start_subparse _((I32 is_format, U32 flags));
void	sub_crush_depth _((CV* cv));
bool	sv_2bool _((SV* sv));
CV*	sv_2cv _((SV* sv, HV** st, GV** gvp, I32 lref));
IO*	sv_2io _((SV* sv));
IV	sv_2iv _((SV* sv));
SV*	sv_2mortal _((SV* sv));
double	sv_2nv _((SV* sv));
char*	sv_2pv _((SV* sv, STRLEN* lp));
UV	sv_2uv _((SV* sv));
void	sv_add_arena _((char* ptr, U32 size, U32 flags));
int	sv_backoff _((SV* sv));
SV*	sv_bless _((SV* sv, HV* stash));
void	sv_catpvf _((SV* sv, const char* pat, ...));
void	sv_catpv _((SV* sv, char* ptr));
void	sv_catpvn _((SV* sv, char* ptr, STRLEN len));
void	sv_catsv _((SV* dsv, SV* ssv));
void	sv_chop _((SV* sv, char* ptr));
void	sv_clean_all _((void));
void	sv_clean_objs _((void));
void	sv_clear _((SV* sv));
I32	sv_cmp _((SV* sv1, SV* sv2));
I32	sv_cmp_locale _((SV* sv1, SV* sv2));
#ifdef USE_LOCALE_COLLATE
char*	sv_collxfrm _((SV* sv, STRLEN* nxp));
#endif
void	sv_dec _((SV* sv));
void	sv_dump _((SV* sv));
bool	sv_derived_from _((SV* sv, char* name));
I32	sv_eq _((SV* sv1, SV* sv2));
void	sv_free _((SV* sv));
void	sv_free_arenas _((void));
char*	sv_gets _((SV* sv, PerlIO* fp, I32 append));
#ifndef DOSISH
char*	sv_grow _((SV* sv, I32 newlen));
#else
char*	sv_grow _((SV* sv, unsigned long newlen));
#endif
void	sv_inc _((SV* sv));
void	sv_insert _((SV* bigsv, STRLEN offset, STRLEN len, char* little, STRLEN littlelen));
int	sv_isa _((SV* sv, char* name));
int	sv_isobject _((SV* sv));
STRLEN	sv_len _((SV* sv));
void	sv_magic _((SV* sv, SV* obj, int how, char* name, I32 namlen));
SV*	sv_mortalcopy _((SV* oldsv));
SV*	sv_newmortal _((void));
SV*	sv_newref _((SV* sv));
char*	sv_peek _((SV* sv));
char*	sv_pvn_force _((SV* sv, STRLEN* lp));
char*	sv_reftype _((SV* sv, int ob));
void	sv_replace _((SV* sv, SV* nsv));
void	sv_report_used _((void));
void	sv_reset _((char* s, HV* stash));
void	sv_setpvf _((SV* sv, const char* pat, ...));
void	sv_setiv _((SV* sv, IV num));
void	sv_setpviv _((SV* sv, IV num));
void	sv_setuv _((SV* sv, UV num));
void	sv_setnv _((SV* sv, double num));
SV*	sv_setref_iv _((SV* rv, char* classname, IV iv));
SV*	sv_setref_nv _((SV* rv, char* classname, double nv));
SV*	sv_setref_pv _((SV* rv, char* classname, void* pv));
SV*	sv_setref_pvn _((SV* rv, char* classname, char* pv, I32 n));
void	sv_setpv _((SV* sv, const char* ptr));
void	sv_setpvn _((SV* sv, const char* ptr, STRLEN len));
void	sv_setsv _((SV* dsv, SV* ssv));
void	sv_taint _((SV* sv));
bool	sv_tainted _((SV* sv));
int	sv_unmagic _((SV* sv, int type));
void	sv_unref _((SV* sv));
void	sv_untaint _((SV* sv));
bool	sv_upgrade _((SV* sv, U32 mt));
void	sv_usepvn _((SV* sv, char* ptr, STRLEN len));
void	sv_vcatpvfn _((SV* sv, const char* pat, STRLEN patlen,
		       va_list* args, SV** svargs, I32 svmax,
		       bool *used_locale));
void	sv_vsetpvfn _((SV* sv, const char* pat, STRLEN patlen,
		       va_list* args, SV** svargs, I32 svmax,
		       bool *used_locale));
void	taint_env _((void));
void	taint_proper _((const char* f, char* s));
#ifdef UNLINK_ALL_VERSIONS
I32	unlnk _((char* f));
#endif
void	unsharepvn _((char* sv, I32 len, U32 hash));
void	unshare_hek _((HEK* hek));
void	utilize _((int aver, I32 floor, OP* version, OP* id, OP* arg));
void	vivify_defelem _((SV* sv));
void	vivify_ref _((SV* sv, U32 to_what));
I32	wait4pid _((int pid, int* statusp, int flags));
void	warn _((const char* pat,...));
void	watch _((char** addr));
I32	whichsig _((char* sig));
int	yyerror _((char* s));
int	yylex _((void));
int	yyparse _((void));
int	yywarn _((char* s));

#if defined(MYMALLOC) || !defined(STANDARD_C)
Malloc_t malloc _((MEM_SIZE nbytes));
Malloc_t calloc _((MEM_SIZE elements, MEM_SIZE size));
Malloc_t realloc _((Malloc_t where, MEM_SIZE nbytes));
Free_t   free _((Malloc_t where));
#endif

#ifndef MYMALLOC
Malloc_t safemalloc _((MEM_SIZE nbytes));
Malloc_t safecalloc _((MEM_SIZE elements, MEM_SIZE size));
Malloc_t saferealloc _((Malloc_t where, MEM_SIZE nbytes));
Free_t   safefree _((Malloc_t where));
#endif

#ifdef LEAKTEST
Malloc_t safexmalloc _((I32 x, MEM_SIZE size));
Malloc_t safexcalloc _((I32 x, MEM_SIZE elements, MEM_SIZE size));
Malloc_t safexrealloc _((Malloc_t where, MEM_SIZE size));
void     safexfree _((Malloc_t where));
#endif
