/*    perl.c
 *
 *    Copyright (c) 1987-1996 Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "A ship then new they built for him/of mithril and of elven glass" --Bilbo
 */

#include "EXTERN.h"
#include "perl.h"
#include "patchlevel.h"

/* Omit -- it causes too much grief on mixed systems.
#ifdef I_UNISTD
#include <unistd.h>
#endif
*/

dEXT char rcsid[] = "perl.c\nPatch level: ###\n";

#ifdef IAMSUID
#ifndef DOSUID
#define DOSUID
#endif
#endif

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
#ifdef DOSUID
#undef DOSUID
#endif
#endif

static void find_beginning _((void));
static void incpush _((char *));
static void init_ids _((void));
static void init_debugger _((void));
static void init_lexer _((void));
static void init_main_stash _((void));
static void init_perllib _((void));
static void init_postdump_symbols _((int, char **, char **));
static void init_predump_symbols _((void));
static void init_stacks _((void));
static void open_script _((char *, bool, SV *));
static void usage _((char *));
static void validate_suid _((char *, char*));

static int fdscript = -1;

PerlInterpreter *
perl_alloc()
{
    PerlInterpreter *sv_interp;

    curinterp = 0;
    New(53, sv_interp, 1, PerlInterpreter);
    return sv_interp;
}

void
perl_construct( sv_interp )
register PerlInterpreter *sv_interp;
{
    if (!(curinterp = sv_interp))
	return;

#ifdef MULTIPLICITY
    Zero(sv_interp, 1, PerlInterpreter);
#endif

    /* Init the real globals? */
    if (!linestr) {
	linestr = NEWSV(65,80);
	sv_upgrade(linestr,SVt_PVIV);

	SvREADONLY_on(&sv_undef);

	sv_setpv(&sv_no,No);
	SvNV(&sv_no);
	SvREADONLY_on(&sv_no);

	sv_setpv(&sv_yes,Yes);
	SvNV(&sv_yes);
	SvREADONLY_on(&sv_yes);

	nrs = newSVpv("\n", 1);
	rs = SvREFCNT_inc(nrs);

#ifdef MSDOS
	/*
	 * There is no way we can refer to them from Perl so close them to save
	 * space.  The other alternative would be to provide STDAUX and STDPRN
	 * filehandles.
	 */
	(void)fclose(stdaux);
	(void)fclose(stdprn);
#endif
    }

#ifdef MULTIPLICITY
    chopset	= " \n-";
    copline	= NOLINE;
    curcop	= &compiling;
    dbargs	= 0;
    dlmax	= 128;
    laststatval	= -1;
    laststype	= OP_STAT;
    maxscream	= -1;
    maxsysfd	= MAXSYSFD;
    rsfp	= Nullfp;
    statname	= Nullsv;
    tmps_floor	= -1;
#endif

    init_ids();

#if defined(SUBVERSION) && SUBVERSION > 0
    sprintf(patchlevel, "%7.5f", 5.0 + (PATCHLEVEL / 1000.0)
				     + (SUBVERSION / 100000.0));
#else
    sprintf(patchlevel, "%5.3f", 5.0 + (PATCHLEVEL / 1000.0));
#endif

#if defined(LOCAL_PATCH_COUNT)
    Ilocalpatches = local_patches;	/* For possible -v */
#endif

    fdpid = newAV();	/* for remembering popen pids by fd */
    pidstatus = newHV();/* for remembering status of dead pids */

    init_stacks();
    ENTER;
}

void
perl_destruct(sv_interp)
register PerlInterpreter *sv_interp;
{
    int destruct_level;  /* 0=none, 1=full, 2=full with checks */
    I32 last_sv_count;
    HV *hv;

    if (!(curinterp = sv_interp))
	return;

    destruct_level = perl_destruct_level;
#ifdef DEBUGGING
    {
	char *s;
	if (s = getenv("PERL_DESTRUCT_LEVEL"))
	    destruct_level = atoi(s);
    }
#endif

    LEAVE;
    FREETMPS;

    if (sv_objcount) {
	/* We must account for everything.  First the syntax tree. */
	if (main_root) {
	    curpad = AvARRAY(comppad);
	    op_free(main_root);
	    main_root = 0;
	}
    }
    if (sv_objcount) {
	/*
	 * Try to destruct global references.  We do this first so that the
	 * destructors and destructees still exist.  Some sv's might remain.
	 * Non-referenced objects are on their own.
	 */
    
	dirty = TRUE;
	sv_clean_objs();
    }

    if (destruct_level == 0){

	DEBUG_P(debprofdump());
    
	/* The exit() function will do everything that needs doing. */
	return;
    }
    
    /* Prepare to destruct main symbol table.  */
    hv = defstash;
    defstash = 0;
    SvREFCNT_dec(hv);

    FREETMPS;
    if (destruct_level >= 2) {
	if (scopestack_ix != 0)
	    warn("Unbalanced scopes: %d more ENTERs than LEAVEs\n", scopestack_ix);
	if (savestack_ix != 0)
	    warn("Unbalanced saves: %d more saves than restores\n", savestack_ix);
	if (tmps_floor != -1)
	    warn("Unbalanced tmps: %d more allocs than frees\n", tmps_floor + 1);
	if (cxstack_ix != -1)
	    warn("Unbalanced context: %d more PUSHes than POPs\n", cxstack_ix + 1);
    }

    /* Now absolutely destruct everything, somehow or other, loops or no. */
    last_sv_count = 0;
    while (sv_count != 0 && sv_count != last_sv_count) {
	last_sv_count = sv_count;
	sv_clean_all();
    }
    if (sv_count != 0)
	warn("Scalars leaked: %d\n", sv_count);
    sv_free_arenas();
    
    DEBUG_P(debprofdump());
}

void
perl_free(sv_interp)
PerlInterpreter *sv_interp;
{
    if (!(curinterp = sv_interp))
	return;
    Safefree(sv_interp);
}
#if !defined(STANDARD_C) && !defined(HAS_GETENV_PROTOTYPE)
char *getenv _((char *)); /* Usually in <stdlib.h> */
#endif

int
perl_parse(sv_interp, xsinit, argc, argv, env)
PerlInterpreter *sv_interp;
void (*xsinit)_((void));
int argc;
char **argv;
char **env;
{
    register SV *sv;
    register char *s;
    char *scriptname = NULL;
    VOL bool dosearch = FALSE;
    char *validarg = "";
    AV* comppadlist;

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
#ifdef IAMSUID
#undef IAMSUID
    croak("suidperl is no longer needed since the kernel can now execute\n\
setuid perl scripts securely.\n");
#endif
#endif

    if (!(curinterp = sv_interp))
	return 255;

    origargv = argv;
    origargc = argc;
#ifndef VMS  /* VMS doesn't have environ array */
    origenviron = environ;
#endif
    e_tmpname = Nullch;

    if (do_undump) {

	/* Come here if running an undumped a.out. */

	origfilename = savepv(argv[0]);
	do_undump = FALSE;
	cxstack_ix = -1;		/* start label stack again */
	init_ids();
	init_postdump_symbols(argc,argv,env);
	return 0;
    }

    if (main_root)
	op_free(main_root);
    main_root = 0;

    switch (Sigsetjmp(top_env,1)) {
    case 1:
#ifdef VMS
	statusvalue = 255;
#else
	statusvalue = 1;
#endif
    case 2:
	curstash = defstash;
	if (endav)
	    calllist(endav);
	return(statusvalue);	/* my_exit() was called */
    case 3:
	fprintf(stderr, "panic: top_env\n");
	return 1;
    }

    sv_setpvn(linestr,"",0);
    sv = newSVpv("",0);		/* first used for -I flags */
    SAVEFREESV(sv);
    init_main_stash();
    for (argc--,argv++; argc > 0; argc--,argv++) {
	if (argv[0][0] != '-' || !argv[0][1])
	    break;
#ifdef DOSUID
    if (*validarg)
	validarg = " PHOOEY ";
    else
	validarg = argv[0];
#endif
	s = argv[0]+1;
      reswitch:
	switch (*s) {
	case '0':
	case 'F':
	case 'a':
	case 'c':
	case 'd':
	case 'D':
	case 'h':
	case 'i':
	case 'l':
	case 'M':
	case 'm':
	case 'n':
	case 'p':
	case 's':
	case 'T':
	case 'u':
	case 'U':
	case 'v':
	case 'w':
	    if (s = moreswitches(s))
		goto reswitch;
	    break;

	case 'e':
	    if (euid != uid || egid != gid)
		croak("No -e allowed in setuid scripts");
	    if (!e_fp) {
		int fd;

	        e_tmpname = savepv(TMPPATH);
		fd = mkstemp(e_tmpname);
		if (fd == -1)
		    croak("Can't mkstemp()");
		e_fp = fdopen(fd,"w");
		if (!e_fp) {
		    close(fd);
		    croak("Cannot open temporary file");
		}
	    }
	    if (argv[1]) {
		fputs(argv[1],e_fp);
		argc--,argv++;
	    }
	    (void)putc('\n', e_fp);
	    break;
	case 'I':
	    taint_not("-I");
	    sv_catpv(sv,"-");
	    sv_catpv(sv,s);
	    sv_catpv(sv," ");
	    if (*++s) {
		av_push(GvAVn(incgv),newSVpv(s,0));
	    }
	    else if (argv[1]) {
		av_push(GvAVn(incgv),newSVpv(argv[1],0));
		sv_catpv(sv,argv[1]);
		argc--,argv++;
		sv_catpv(sv," ");
	    }
	    break;
	case 'P':
	    taint_not("-P");
	    preprocess = TRUE;
	    s++;
	    goto reswitch;
	case 'S':
	    taint_not("-S");
	    dosearch = TRUE;
	    s++;
	    goto reswitch;
	case 'V':
	    if (!preambleav)
		preambleav = newAV();
	    av_push(preambleav, newSVpv("use Config qw(myconfig config_vars)",0));
	    if (*++s != ':')  {
		Sv = newSVpv("print myconfig(),'@INC: '.\"@INC\\n\"",0);
	    }
	    else {
		Sv = newSVpv("config_vars(qw(",0);
		sv_catpv(Sv, ++s);
		sv_catpv(Sv, "))");
		s += strlen(s);
	    }
	    av_push(preambleav, Sv);
	    scriptname = BIT_BUCKET;	/* don't look for script or read stdin */
	    goto reswitch;
	case 'x':
	    doextract = TRUE;
	    s++;
	    if (*s)
		cddir = savepv(s);
	    break;
	case '-':
	    argc--,argv++;
	    goto switch_end;
	case 0:
	    break;
	default:
	    croak("Unrecognized switch: -%s",s);
	}
    }
  switch_end:
    if (!scriptname)
	scriptname = argv[0];
    if (e_fp) {
	if (Fflush(e_fp) || ferror(e_fp) || fclose(e_fp))
	    croak("Can't write to temp file for -e: %s", Strerror(errno));
	e_fp = Nullfp;
	argc++,argv--;
	scriptname = e_tmpname;
    }
    else if (scriptname == Nullch) {
#ifdef MSDOS
	if ( isatty(fileno(stdin)) )
	    moreswitches("v");
#endif
	scriptname = "-";
    }

    init_perllib();

    open_script(scriptname,dosearch,sv);

    validate_suid(validarg, scriptname);

    if (doextract)
	find_beginning();

    compcv = (CV*)NEWSV(1104,0);
    sv_upgrade((SV *)compcv, SVt_PVCV);

    pad = newAV();
    comppad = pad;
    av_push(comppad, Nullsv);
    curpad = AvARRAY(comppad);
    padname = newAV();
    comppad_name = padname;
    comppad_name_fill = 0;
    min_intro_pending = 0;
    padix = 0;

    comppadlist = newAV();
    AvREAL_off(comppadlist);
    av_store(comppadlist, 0, (SV*)comppad_name);
    av_store(comppadlist, 1, (SV*)comppad);
    CvPADLIST(compcv) = comppadlist;

    if (xsinit)
	(*xsinit)();	/* in case linked C routines want magical variables */
#ifdef VMS
    init_os_extras();
#endif

    init_predump_symbols();
    if (!do_undump)
	init_postdump_symbols(argc,argv,env);

    init_lexer();

    /* now parse the script */

    error_count = 0;
    if (yyparse() || error_count) {
	if (minus_c)
	    croak("%s had compilation errors.\n", origfilename);
	else {
	    croak("Execution of %s aborted due to compilation errors.\n",
		origfilename);
	}
    }
    curcop->cop_line = 0;
    curstash = defstash;
    preprocess = FALSE;
    if (e_tmpname) {
	(void)UNLINK(e_tmpname);
	Safefree(e_tmpname);
	e_tmpname = Nullch;
    }

    /* now that script is parsed, we can modify record separator */
    SvREFCNT_dec(rs);
    rs = SvREFCNT_inc(nrs);
    sv_setsv(GvSV(gv_fetchpv("/", TRUE, SVt_PV)), rs);

    if (do_undump)
	my_unexec();

    if (dowarn)
	gv_check(defstash);

    LEAVE;
    FREETMPS;

#ifdef DEBUGGING_MSTATS
    if ((s=getenv("PERL_DEBUG_MSTATS")) && atoi(s) >= 2)
	dump_mstats("after compilation:");
#endif

    ENTER;
    restartop = 0;
    return 0;
}

int
perl_run(sv_interp)
PerlInterpreter *sv_interp;
{
    if (!(curinterp = sv_interp))
	return 255;
    switch (Sigsetjmp(top_env,1)) {
    case 1:
	cxstack_ix = -1;		/* start context stack again */
	break;
    case 2:
	curstash = defstash;
	if (endav)
	    calllist(endav);
	FREETMPS;
#ifdef DEBUGGING_MSTATS
	if (getenv("PERL_DEBUG_MSTATS"))
	    dump_mstats("after execution:  ");
#endif
	return(statusvalue);		/* my_exit() was called */
    case 3:
	if (!restartop) {
	    fprintf(stderr, "panic: restartop\n");
	    FREETMPS;
	    return 1;
	}
	if (stack != mainstack) {
	    dSP;
	    SWITCHSTACK(stack, mainstack);
	}
	break;
    }

    if (!restartop) {
	DEBUG_x(dump_all());
	DEBUG(fprintf(stderr,"\nEXECUTING...\n\n"));

	if (minus_c) {
	    fprintf(stderr,"%s syntax OK\n", origfilename);
	    my_exit(0);
	}
	if (perldb && DBsingle)
	   sv_setiv(DBsingle, 1); 
    }

    /* do it */

    if (restartop) {
	op = restartop;
	restartop = 0;
	runops();
    }
    else if (main_start) {
	op = main_start;
	runops();
    }

    my_exit(0);
    return 0;
}

void
my_exit(status)
U32 status;
{
    register CONTEXT *cx;
    I32 gimme;
    SV **newsp;

    statusvalue = FIXSTATUS(status);
    if (cxstack_ix >= 0) {
	if (cxstack_ix > 0)
	    dounwind(0);
	POPBLOCK(cx,curpm);
	LEAVE;
    }
    Siglongjmp(top_env, 2);
}

SV*
perl_get_sv(name, create)
char* name;
I32 create;
{
    GV* gv = gv_fetchpv(name, create, SVt_PV);
    if (gv)
	return GvSV(gv);
    return Nullsv;
}

AV*
perl_get_av(name, create)
char* name;
I32 create;
{
    GV* gv = gv_fetchpv(name, create, SVt_PVAV);
    if (create)
    	return GvAVn(gv);
    if (gv)
	return GvAV(gv);
    return Nullav;
}

HV*
perl_get_hv(name, create)
char* name;
I32 create;
{
    GV* gv = gv_fetchpv(name, create, SVt_PVHV);
    if (create)
    	return GvHVn(gv);
    if (gv)
	return GvHV(gv);
    return Nullhv;
}

CV*
perl_get_cv(name, create)
char* name;
I32 create;
{
    GV* gv = gv_fetchpv(name, create, SVt_PVCV);
    if (create && !GvCV(gv))
    	return newSUB(start_subparse(),
		      newSVOP(OP_CONST, 0, newSVpv(name,0)),
		      Nullop,
		      Nullop);
    if (gv)
	return GvCV(gv);
    return Nullcv;
}

/* Be sure to refetch the stack pointer after calling these routines. */

I32
perl_call_argv(subname, flags, argv)
char *subname;
I32 flags;		/* See G_* flags in cop.h */
register char **argv;	/* null terminated arg list */
{
    dSP;

    PUSHMARK(sp);
    if (argv) {
	while (*argv) {
	    XPUSHs(sv_2mortal(newSVpv(*argv,0)));
	    argv++;
	}
	PUTBACK;
    }
    return perl_call_pv(subname, flags);
}

I32
perl_call_pv(subname, flags)
char *subname;		/* name of the subroutine */
I32 flags;		/* See G_* flags in cop.h */
{
    return perl_call_sv((SV*)perl_get_cv(subname, TRUE), flags);
}

I32
perl_call_method(methname, flags)
char *methname;		/* name of the subroutine */
I32 flags;		/* See G_* flags in cop.h */
{
    dSP;
    OP myop;
    if (!op)
	op = &myop;
    XPUSHs(sv_2mortal(newSVpv(methname,0)));
    PUTBACK;
    pp_method();
    return perl_call_sv(*stack_sp--, flags);
}

/* May be called with any of a CV, a GV, or an SV containing the name. */
I32
perl_call_sv(sv, flags)
SV* sv;
I32 flags;		/* See G_* flags in cop.h */
{
    LOGOP myop;		/* fake syntax tree node */
    SV** sp = stack_sp;
    I32 oldmark = TOPMARK;
    I32 retval;
    Sigjmp_buf oldtop;
    I32 oldscope;
    
    if (flags & G_DISCARD) {
	ENTER;
	SAVETMPS;
    }

    SAVESPTR(op);
    op = (OP*)&myop;
    Zero(op, 1, LOGOP);
    EXTEND(stack_sp, 1);
    *++stack_sp = sv;
    oldscope = scopestack_ix;

    if (!(flags & G_NOARGS))
	myop.op_flags = OPf_STACKED;
    myop.op_next = Nullop;
    myop.op_flags |= OPf_KNOW;
    if (flags & G_ARRAY)
      myop.op_flags |= OPf_LIST;

    if (flags & G_EVAL) {
	Copy(top_env, oldtop, 1, Sigjmp_buf);

	cLOGOP->op_other = op;
	markstack_ptr--;
	/* we're trying to emulate pp_entertry() here */
	{
	    register CONTEXT *cx;
	    I32 gimme = GIMME;
	    
	    ENTER;
	    SAVETMPS;
	    
	    push_return(op->op_next);
	    PUSHBLOCK(cx, CXt_EVAL, stack_sp);
	    PUSHEVAL(cx, 0, 0);
	    eval_root = op;             /* Only needed so that goto works right. */
	    
	    in_eval = 1;
	    if (flags & G_KEEPERR)
		in_eval |= 4;
	    else
		sv_setpv(GvSV(errgv),"");
	}
	markstack_ptr++;

    restart:
	switch (Sigsetjmp(top_env,1)) {
	case 0:
	    break;
	case 1:
#ifdef VMS
	    statusvalue = 255;	/* XXX I don't think we use 1 anymore. */
#else
	statusvalue = 1;
#endif
	    /* FALL THROUGH */
	case 2:
	    /* my_exit() was called */
	    curstash = defstash;
	    FREETMPS;
	    Copy(oldtop, top_env, 1, Sigjmp_buf);
	    if (statusvalue)
		croak("Callback called exit");
	    my_exit(statusvalue);
	    /* NOTREACHED */
	case 3:
	    if (restartop) {
		op = restartop;
		restartop = 0;
		goto restart;
	    }
	    stack_sp = stack_base + oldmark;
	    if (flags & G_ARRAY)
		retval = 0;
	    else {
		retval = 1;
		*++stack_sp = &sv_undef;
	    }
	    goto cleanup;
	}
    }

    if (op == (OP*)&myop)
	op = pp_entersub();
    if (op)
	runops();
    retval = stack_sp - (stack_base + oldmark);
    if ((flags & G_EVAL) && !(flags & G_KEEPERR))
	sv_setpv(GvSV(errgv),"");

  cleanup:
    if (flags & G_EVAL) {
	if (scopestack_ix > oldscope) {
	    SV **newsp;
	    PMOP *newpm;
	    I32 gimme;
	    register CONTEXT *cx;
	    I32 optype;

	    POPBLOCK(cx,newpm);
	    POPEVAL(cx);
	    pop_return();
	    curpm = newpm;
	    LEAVE;
	}
	Copy(oldtop, top_env, 1, Sigjmp_buf);
    }
    if (flags & G_DISCARD) {
	stack_sp = stack_base + oldmark;
	retval = 0;
	FREETMPS;
	LEAVE;
    }
    return retval;
}

/* Eval a string. */

I32
perl_eval_sv(sv, flags)
SV* sv;
I32 flags;		/* See G_* flags in cop.h */
{
    UNOP myop;		/* fake syntax tree node */
    SV** sp = stack_sp;
    I32 oldmark = sp - stack_base;
    I32 retval;
    Sigjmp_buf oldtop;
    I32 oldscope;
    
    if (flags & G_DISCARD) {
	ENTER;
	SAVETMPS;
    }

    SAVESPTR(op);
    op = (OP*)&myop;
    Zero(op, 1, UNOP);
    EXTEND(stack_sp, 1);
    *++stack_sp = sv;
    oldscope = scopestack_ix;

    if (!(flags & G_NOARGS))
	myop.op_flags = OPf_STACKED;
    myop.op_next = Nullop;
    myop.op_flags |= OPf_KNOW;
    if (flags & G_ARRAY)
      myop.op_flags |= OPf_LIST;

    Copy(top_env, oldtop, 1, Sigjmp_buf);

restart:
    switch (Sigsetjmp(top_env,1)) {
    case 0:
	break;
    case 1:
#ifdef VMS
	statusvalue = 255;	/* XXX I don't think we use 1 anymore. */
#else
    statusvalue = 1;
#endif
	/* FALL THROUGH */
    case 2:
	/* my_exit() was called */
	curstash = defstash;
	FREETMPS;
	Copy(oldtop, top_env, 1, Sigjmp_buf);
	if (statusvalue)
	    croak("Callback called exit");
	my_exit(statusvalue);
	/* NOTREACHED */
    case 3:
	if (restartop) {
	    op = restartop;
	    restartop = 0;
	    goto restart;
	}
	stack_sp = stack_base + oldmark;
	if (flags & G_ARRAY)
	    retval = 0;
	else {
	    retval = 1;
	    *++stack_sp = &sv_undef;
	}
	goto cleanup;
    }

    if (op == (OP*)&myop)
	op = pp_entereval();
    if (op)
	runops();
    retval = stack_sp - (stack_base + oldmark);
    if ((flags & G_EVAL) && !(flags & G_KEEPERR))
	sv_setpv(GvSV(errgv),"");

  cleanup:
    Copy(oldtop, top_env, 1, Sigjmp_buf);
    if (flags & G_DISCARD) {
	stack_sp = stack_base + oldmark;
	retval = 0;
	FREETMPS;
	LEAVE;
    }
    return retval;
}

/* Require a module. */

void
perl_require_pv(pv)
char* pv;
{
    SV* sv = sv_newmortal();
    sv_setpv(sv, "require '");
    sv_catpv(sv, pv);
    sv_catpv(sv, "'");
    perl_eval_sv(sv, G_DISCARD);
}

void
magicname(sym,name,namlen)
char *sym;
char *name;
I32 namlen;
{
    register GV *gv;

    if (gv = gv_fetchpv(sym,TRUE, SVt_PV))
	sv_magic(GvSV(gv), (SV*)gv, 0, name, namlen);
}

#if defined(DOSISH)
#    define PERLLIB_SEP ';'
#else
#  if defined(VMS)
#    define PERLLIB_SEP '|'
#  else
#    define PERLLIB_SEP ':'
#  endif
#endif

static void
incpush(p)
char *p;
{
    char *s;

    if (!p)
	return;

    /* Break at all separators */
    while (*p) {
	/* First, skip any consecutive separators */
	while ( *p == PERLLIB_SEP ) {
	    /* Uncomment the next line for PATH semantics */
	    /* av_push(GvAVn(incgv), newSVpv(".", 1)); */
	    p++;
	}
	if ( (s = strchr(p, PERLLIB_SEP)) != Nullch ) {
	    av_push(GvAVn(incgv), newSVpv(p, (STRLEN)(s - p)));
	    p = s + 1;
	} else {
	    av_push(GvAVn(incgv), newSVpv(p, 0));
	    break;
	}
    }
}

static void
usage(name)		/* XXX move this out into a module ? */
char *name;
{
    /* This message really ought to be max 23 lines.
     * Removed -h because the user already knows that opton. Others? */
    printf("\nUsage: %s [switches] [--] [programfile] [arguments]", name);
    printf("\n  -0[octal]       specify record separator (\\0, if no argument)");
    printf("\n  -a              autosplit mode with -n or -p (splits $_ into @F)");
    printf("\n  -c              check syntax only (runs BEGIN and END blocks)");
    printf("\n  -d[:debugger]   run scripts under debugger");
    printf("\n  -D[number/list] set debugging flags (argument is a bit mask or flags)");
    printf("\n  -e 'command'    one line of script. Several -e's allowed. Omit [programfile].");
    printf("\n  -F/pattern/     split() pattern for autosplit (-a). The //'s are optional.");
    printf("\n  -i[extension]   edit <> files in place (make backup if extension supplied)");
    printf("\n  -Idirectory     specify @INC/#include directory (may be used more then once)");
    printf("\n  -l[octal]       enable line ending processing, specifies line teminator");
    printf("\n  -[mM][-]module.. executes `use/no module...' before executing your script.");
    printf("\n  -n              assume 'while (<>) { ... }' loop arround your script");
    printf("\n  -p              assume loop like -n but print line also like sed");
    printf("\n  -P              run script through C preprocessor before compilation");
#ifdef OS2
    printf("\n  -R              enable REXX variable pool");
#endif      
    printf("\n  -s              enable some switch parsing for switches after script name");
    printf("\n  -S              look for the script using PATH environment variable");
    printf("\n  -T              turn on tainting checks");
    printf("\n  -u              dump core after parsing script");
    printf("\n  -U              allow unsafe operations");
    printf("\n  -v              print version number and patchlevel of perl");
    printf("\n  -V[:variable]   print perl configuration information");
    printf("\n  -w              TURN WARNINGS ON FOR COMPILATION OF YOUR SCRIPT.");
    printf("\n  -x[directory]   strip off text before #!perl line and perhaps cd to directory\n");
}

/* This routine handles any switches that can be given during run */

char *
moreswitches(s)
char *s;
{
    I32 numlen;
    U32 rschar;

    switch (*s) {
    case '0':
	rschar = scan_oct(s, 4, &numlen);
	SvREFCNT_dec(nrs);
	if (rschar & ~((U8)~0))
	    nrs = &sv_undef;
	else if (!rschar && numlen >= 2)
	    nrs = newSVpv("", 0);
	else {
	    char ch = rschar;
	    nrs = newSVpv(&ch, 1);
	}
	return s + numlen;
    case 'F':
	minus_F = TRUE;
	splitstr = savepv(s + 1);
	s += strlen(s);
	return s;
    case 'a':
	minus_a = TRUE;
	s++;
	return s;
    case 'c':
	minus_c = TRUE;
	s++;
	return s;
    case 'd':
	taint_not("-d");
	s++;
	if (*s == ':' || *s == '=')  {
	    sprintf(buf, "use Devel::%s;", ++s);
	    s += strlen(s);
	    my_setenv("PERL5DB",buf);
	}
	if (!perldb) {
	    perldb = TRUE;
	    init_debugger();
	}
	return s;
    case 'D':
#ifdef DEBUGGING
	taint_not("-D");
	if (isALPHA(s[1])) {
	    static char debopts[] = "psltocPmfrxuLHXD";
	    char *d;

	    for (s++; *s && (d = strchr(debopts,*s)); s++)
		debug |= 1 << (d - debopts);
	}
	else {
	    debug = atoi(s+1);
	    for (s++; isDIGIT(*s); s++) ;
	}
	debug |= 0x80000000;
#else
	warn("Recompile perl with -DDEBUGGING to use -D switch\n");
	for (s++; isALNUM(*s); s++) ;
#endif
	/*SUPPRESS 530*/
	return s;
    case 'h':
	usage(origargv[0]);    
	exit(0);
    case 'i':
	if (inplace)
	    Safefree(inplace);
	inplace = savepv(s+1);
	/*SUPPRESS 530*/
	for (s = inplace; *s && !isSPACE(*s); s++) ;
	*s = '\0';
	break;
    case 'I':
	taint_not("-I");
	if (*++s) {
	    char *e;
	    for (e = s; *e && !isSPACE(*e); e++) ;
	    av_push(GvAVn(incgv),newSVpv(s,e-s));
	    if (*e)
		return e;
	}
	else
	    croak("No space allowed after -I");
	break;
    case 'l':
	minus_l = TRUE;
	s++;
	if (ors)
	    Safefree(ors);
	if (isDIGIT(*s)) {
	    ors = savepv("\n");
	    orslen = 1;
	    *ors = scan_oct(s, 3 + (*s == '0'), &numlen);
	    s += numlen;
	}
	else {
	    if (RsPARA(nrs)) {
		ors = savepvn("\n\n", 2);
		orslen = 2;
	    }
	    else
		ors = SvPV(nrs, orslen);
	}
	return s;
    case 'M':
	taint_not("-M");	/* XXX ? */
	/* FALL THROUGH */
    case 'm':
	taint_not("-m");	/* XXX ? */
	if (*++s) {
	    char *start;
	    char *use = "use ";
	    /* -M-foo == 'no foo'	*/
	    if (*s == '-') { use = "no "; ++s; }
	    Sv = newSVpv(use,0);
	    start = s;
	    /* We allow -M'Module qw(Foo Bar)'	*/
	    while(isALNUM(*s) || *s==':') ++s;
	    if (*s != '=') {
		sv_catpv(Sv, start);
		if (*(start-1) == 'm') {
		    if (*s != '\0')
			croak("Can't use '%c' after -mname", *s);
		    sv_catpv( Sv, " ()");
		}
	    } else {
		sv_catpvn(Sv, start, s-start);
		sv_catpv(Sv, " split(/,/,q{");
		sv_catpv(Sv, ++s);
		sv_catpv(Sv,    "})");
	    }
	    s += strlen(s);
	    if (preambleav == NULL)
		preambleav = newAV();
	    av_push(preambleav, Sv);
	}
	else
	    croak("No space allowed after -%c", *(s-1));
	return s;
    case 'n':
	minus_n = TRUE;
	s++;
	return s;
    case 'p':
	minus_p = TRUE;
	s++;
	return s;
    case 's':
	taint_not("-s");
	doswitches = TRUE;
	s++;
	return s;
    case 'T':
	tainting = TRUE;
	s++;
	return s;
    case 'u':
	do_undump = TRUE;
	s++;
	return s;
    case 'U':
	unsafe = TRUE;
	s++;
	return s;
    case 'v':
#if defined(SUBVERSION) && SUBVERSION > 0
	printf("\nThis is perl, version 5.%03d_%02d", PATCHLEVEL, SUBVERSION);
#else
	printf("\nThis is perl, version %s",patchlevel);
#endif

#if defined(DEBUGGING) || defined(EMBED) || defined(MULTIPLICITY)
	fputs(" with", stdout);
#ifdef DEBUGGING
	fputs(" DEBUGGING", stdout);
#endif
#ifdef EMBED
	fputs(" EMBED", stdout);
#endif
#ifdef MULTIPLICITY
	fputs(" MULTIPLICITY", stdout);
#endif
#endif

#if defined(LOCAL_PATCH_COUNT)
    if (LOCAL_PATCH_COUNT > 0)
    {	int i;
	fputs("\n\tLocally applied patches:\n", stdout);
	for (i = 1; i <= LOCAL_PATCH_COUNT; i++) {
		if (Ilocalpatches[i])
			fprintf(stdout, "\t  %s\n", Ilocalpatches[i]);
	}
    }
#endif
    printf("\n\tbuilt under %s",OSNAME);
#ifdef __DATE__
#  ifdef __TIME__
	printf(" at %s %s",__DATE__,__TIME__);
#  else
	printf(" on %s",__DATE__);
#  endif
#endif
	fputs("\n\t+ suidperl security patch", stdout);
	fputs("\n\nCopyright 1987-1996, Larry Wall\n",stdout);
#ifdef MSDOS
	fputs("MS-DOS port Copyright (c) 1989, 1990, Diomidis Spinellis\n",
	stdout);
#endif
#ifdef OS2
	fputs("OS/2 port Copyright (c) 1990, 1991, Raymond Chen, Kai Uwe Rommel\n"
	    "Version 5 port Copyright (c) 1994-1995, Andreas Kaiser\n", stdout);
#endif
#ifdef atarist
	fputs("atariST series port, ++jrb  bammi@cadence.com\n", stdout);
#endif
	fputs("\n\
Perl may be copied only under the terms of either the Artistic License or the\n\
GNU General Public License, which may be found in the Perl 5.0 source kit.\n\n",stdout);
#ifdef MSDOS
        usage(origargv[0]);
#endif
	exit(0);
    case 'w':
	dowarn = TRUE;
	s++;
	return s;
    case '*':
    case ' ':
	if (s[1] == '-')	/* Additional switches on #! line. */
	    return s+2;
	break;
    case '-':
    case 0:
    case '\n':
    case '\t':
	break;
    case 'P':
	if (preprocess)
	    return s+1;
	/* FALL THROUGH */
    default:
	croak("Can't emulate -%.1s on #! line",s);
    }
    return Nullch;
}

/* compliments of Tom Christiansen */

/* unexec() can be found in the Gnu emacs distribution */

void
my_unexec()
{
#ifdef UNEXEC
    int    status;
    extern int etext;

    sprintf (buf, "%s.perldump", origfilename);
    sprintf (tokenbuf, "%s/perl", BIN);

    status = unexec(buf, tokenbuf, &etext, sbrk(0), 0);
    if (status)
	fprintf(stderr, "unexec of %s into %s failed!\n", tokenbuf, buf);
    exit(status);
#else
#  ifdef VMS
#    include <lib$routines.h>
     lib$signal(SS$_DEBUG);  /* ssdef.h #included from vmsish.h */
#else
    ABORT();		/* for use with undump */
#endif
#endif
}

static void
init_main_stash()
{
    GV *gv;
    curstash = defstash = newHV();
    curstname = newSVpv("main",4);
    gv = gv_fetchpv("main::",TRUE, SVt_PVHV);
    SvREFCNT_dec(GvHV(gv));
    GvHV(gv) = (HV*)SvREFCNT_inc(defstash);
    SvREADONLY_on(gv);
    HvNAME(defstash) = savepv("main");
    incgv = gv_HVadd(gv_AVadd(gv_fetchpv("INC",TRUE, SVt_PVAV)));
    GvMULTI_on(incgv);
    defgv = gv_fetchpv("_",TRUE, SVt_PVAV);
    errgv = gv_HVadd(gv_fetchpv("@", TRUE, SVt_PV));
    GvMULTI_on(errgv);
    curstash = defstash;
    compiling.cop_stash = defstash;
    debstash = GvHV(gv_fetchpv("DB::", GV_ADDMULTI, SVt_PVHV));
    /* We must init $/ before switches are processed. */
    sv_setpvn(GvSV(gv_fetchpv("/", TRUE, SVt_PV)), "\n", 1);
}

#ifdef CAN_PROTOTYPE
static void
open_script(char *scriptname, bool dosearch, SV *sv)
#else
static void
open_script(scriptname,dosearch,sv)
char *scriptname;
bool dosearch;
SV *sv;
#endif
{
    char *xfound = Nullch;
    char *xfailed = Nullch;
    register char *s;
    I32 len;
    int retval;
#if defined(DOSISH) && !defined(OS2) && !defined(atarist)
#define SEARCH_EXTS ".bat", ".cmd", NULL
#endif
#ifdef VMS
#  define SEARCH_EXTS ".pl", ".com", NULL
#endif
    /* additional extensions to try in each dir if scriptname not found */
#ifdef SEARCH_EXTS
    char *ext[] = { SEARCH_EXTS };
    int extidx = (strchr(scriptname,'.')) ? -1 : 0; /* has ext already */
#endif

#ifdef VMS
    if (dosearch && !strpbrk(scriptname,":[</") && (my_getenv("DCL$PATH"))) {
	int idx = 0;

	while (my_trnlnm("DCL$PATH",tokenbuf,idx++)) {
	    strcat(tokenbuf,scriptname);
#else  /* !VMS */
    if (dosearch && !strchr(scriptname, '/') && (s = getenv("PATH"))) {

	bufend = s + strlen(s);
	while (*s) {
#ifndef DOSISH
	    s = cpytill(tokenbuf,s,bufend,':',&len);
#else
#ifdef atarist
	    for (len = 0; *s && *s != ',' && *s != ';'; tokenbuf[len++] = *s++);
	    tokenbuf[len] = '\0';
#else
	    for (len = 0; *s && *s != ';'; tokenbuf[len++] = *s++);
	    tokenbuf[len] = '\0';
#endif
#endif
	    if (*s)
		s++;
#ifndef DOSISH
	    if (len && tokenbuf[len-1] != '/')
#else
#ifdef atarist
	    if (len && ((tokenbuf[len-1] != '\\') && (tokenbuf[len-1] != '/')))
#else
	    if (len && tokenbuf[len-1] != '\\')
#endif
#endif
		(void)strcat(tokenbuf+len,"/");
	    (void)strcat(tokenbuf+len,scriptname);
#endif  /* !VMS */

#ifdef SEARCH_EXTS
	    len = strlen(tokenbuf);
	    if (extidx > 0)	/* reset after previous loop */
		extidx = 0;
	    do {
#endif
		DEBUG_p(fprintf(stderr,"Looking for %s\n",tokenbuf));
		retval = Stat(tokenbuf,&statbuf);
#ifdef SEARCH_EXTS
	    } while (  retval < 0		/* not there */
		    && extidx>=0 && ext[extidx]	/* try an extension? */
		    && strcpy(tokenbuf+len, ext[extidx++])
		);
#endif
	    if (retval < 0)
		continue;
	    if (S_ISREG(statbuf.st_mode)
	     && cando(S_IRUSR,TRUE,&statbuf) && cando(S_IXUSR,TRUE,&statbuf)) {
		xfound = tokenbuf;              /* bingo! */
		break;
	    }
	    if (!xfailed)
		xfailed = savepv(tokenbuf);
	}
	if (!xfound)
	    croak("Can't execute %s", xfailed ? xfailed : scriptname );
	if (xfailed)
	    Safefree(xfailed);
	scriptname = xfound;
    }

    if (strnEQ(scriptname, "/dev/fd/", 8) && isDIGIT(scriptname[8]) ) {
	char *s = scriptname + 8;
	fdscript = atoi(s);
	while (isDIGIT(*s))
	    s++;
	if (*s)
	    scriptname = s + 1;
    }
    else
	fdscript = -1;
    origfilename = savepv(e_tmpname ? "-e" : scriptname);
    curcop->cop_filegv = gv_fetchfile(origfilename);
    if (strEQ(origfilename,"-"))
	scriptname = "";
    if (fdscript >= 0) {
	rsfp = fdopen(fdscript,"r");
#if defined(HAS_FCNTL) && defined(F_SETFD)
	fcntl(fileno(rsfp),F_SETFD,1);	/* ensure close-on-exec */
#endif
    }
    else if (preprocess) {
	char *cpp = CPPSTDIN;

	if (strEQ(cpp,"cppstdin"))
	    sprintf(tokenbuf, "%s/%s", SCRIPTDIR, cpp);
	else
	    sprintf(tokenbuf, "%s", cpp);
	sv_catpv(sv,"-I");
	sv_catpv(sv,PRIVLIB_EXP);
#ifdef MSDOS
	(void)sprintf(buf, "\
sed %s -e \"/^[^#]/b\" \
 -e \"/^#[ 	]*include[ 	]/b\" \
 -e \"/^#[ 	]*define[ 	]/b\" \
 -e \"/^#[ 	]*if[ 	]/b\" \
 -e \"/^#[ 	]*ifdef[ 	]/b\" \
 -e \"/^#[ 	]*ifndef[ 	]/b\" \
 -e \"/^#[ 	]*else/b\" \
 -e \"/^#[ 	]*elif[ 	]/b\" \
 -e \"/^#[ 	]*undef[ 	]/b\" \
 -e \"/^#[ 	]*endif/b\" \
 -e \"s/^#.*//\" \
 %s | %s -C %s %s",
	  (doextract ? "-e \"1,/^#/d\n\"" : ""),
#else
	(void)sprintf(buf, "\
%s %s -e '/^[^#]/b' \
 -e '/^#[ 	]*include[ 	]/b' \
 -e '/^#[ 	]*define[ 	]/b' \
 -e '/^#[ 	]*if[ 	]/b' \
 -e '/^#[ 	]*ifdef[ 	]/b' \
 -e '/^#[ 	]*ifndef[ 	]/b' \
 -e '/^#[ 	]*else/b' \
 -e '/^#[ 	]*elif[ 	]/b' \
 -e '/^#[ 	]*undef[ 	]/b' \
 -e '/^#[ 	]*endif/b' \
 -e 's/^[ 	]*#.*//' \
 %s | %s -C %s %s",
#ifdef LOC_SED
	  LOC_SED,
#else
	  "sed",
#endif
	  (doextract ? "-e '1,/^#/d\n'" : ""),
#endif
	  scriptname, tokenbuf, SvPV(sv, na), CPPMINUS);
	doextract = FALSE;
#ifdef IAMSUID				/* actually, this is caught earlier */
	if (euid != uid && !euid) {	/* if running suidperl */
#ifdef HAS_SETEUID
	    (void)seteuid(uid);		/* musn't stay setuid root */
#else
#ifdef HAS_SETREUID
	    (void)setreuid((Uid_t)-1, uid);
#else
#ifdef HAS_SETRESUID
	    (void)setresuid((Uid_t)-1, uid, (Uid_t)-1);
#else
	    setuid(uid);
#endif
#endif
#endif
	    if (geteuid() != uid)
		croak("Can't do seteuid!\n");
	}
#endif /* IAMSUID */
	rsfp = my_popen(buf,"r");
    }
    else if (!*scriptname) {
	taint_not("program input from stdin");
	rsfp = stdin;
    }
    else {
	rsfp = fopen(scriptname,"r");
#if defined(HAS_FCNTL) && defined(F_SETFD)
	fcntl(fileno(rsfp),F_SETFD,1);	/* ensure close-on-exec */
#endif
    }
    if ((FILE*)rsfp == Nullfp) {
#ifdef DOSUID
#ifndef IAMSUID		/* in case script is not readable before setuid */
	if (euid && Stat(SvPVX(GvSV(curcop->cop_filegv)),&statbuf) >= 0 &&
	  statbuf.st_mode & (S_ISUID|S_ISGID)) {
	    (void)sprintf(buf, "%s/sperl%s", BIN, patchlevel);
	    execv(buf, origargv);	/* try again */
	    croak("Can't do setuid\n");
	}
#endif
#endif
	croak("Can't open perl script \"%s\": %s\n",
	  SvPVX(GvSV(curcop->cop_filegv)), Strerror(errno));
    }
}

static void
validate_suid(validarg, scriptname)
char *validarg;
char *scriptname;
{
    int which;

    /* do we need to emulate setuid on scripts? */

    /* This code is for those BSD systems that have setuid #! scripts disabled
     * in the kernel because of a security problem.  Merely defining DOSUID
     * in perl will not fix that problem, but if you have disabled setuid
     * scripts in the kernel, this will attempt to emulate setuid and setgid
     * on scripts that have those now-otherwise-useless bits set.  The setuid
     * root version must be called suidperl or sperlN.NNN.  If regular perl
     * discovers that it has opened a setuid script, it calls suidperl with
     * the same argv that it had.  If suidperl finds that the script it has
     * just opened is NOT setuid root, it sets the effective uid back to the
     * uid.  We don't just make perl setuid root because that loses the
     * effective uid we had before invoking perl, if it was different from the
     * uid.
     *
     * DOSUID must be defined in both perl and suidperl, and IAMSUID must
     * be defined in suidperl only.  suidperl must be setuid root.  The
     * Configure script will set this up for you if you want it.
     */

#ifdef DOSUID
    char *s;

    if (Fstat(fileno(rsfp),&statbuf) < 0)	/* normal stat is insecure */
	croak("Can't stat script \"%s\"",origfilename);
    if (fdscript < 0 && statbuf.st_mode & (S_ISUID|S_ISGID)) {
	I32 len;

#ifdef IAMSUID
#ifndef HAS_SETREUID
	/* On this access check to make sure the directories are readable,
	 * there is actually a small window that the user could use to make
	 * filename point to an accessible directory.  So there is a faint
	 * chance that someone could execute a setuid script down in a
	 * non-accessible directory.  I don't know what to do about that.
	 * But I don't think it's too important.  The manual lies when
	 * it says access() is useful in setuid programs.
	 */
	if (access(SvPVX(GvSV(curcop->cop_filegv)),1))	/*double check*/
	    croak("Permission denied");
#else
	/* If we can swap euid and uid, then we can determine access rights
	 * with a simple stat of the file, and then compare device and
	 * inode to make sure we did stat() on the same file we opened.
	 * Then we just have to make sure he or she can execute it.
	 */
	{
	    struct stat tmpstatbuf;

	    if (
#ifdef HAS_SETREUID
		setreuid(euid,uid) < 0
#else
# if HAS_SETRESUID
		setresuid(euid,uid,(Uid_t)-1) < 0
# endif
#endif
		|| getuid() != euid || geteuid() != uid)
		croak("Can't swap uid and euid");	/* really paranoid */
	    if (Stat(SvPVX(GvSV(curcop->cop_filegv)),&tmpstatbuf) < 0)
		croak("Permission denied");	/* testing full pathname here */
	    if (tmpstatbuf.st_dev != statbuf.st_dev ||
		tmpstatbuf.st_ino != statbuf.st_ino) {
		(void)fclose(rsfp);
		if (rsfp = my_popen("/bin/mail root","w")) {	/* heh, heh */
		    fprintf(rsfp,
"User %d tried to run dev %d ino %d in place of dev %d ino %d!\n\
(Filename of set-id script was %s, uid %d gid %d.)\n\nSincerely,\nperl\n",
			uid,tmpstatbuf.st_dev, tmpstatbuf.st_ino,
			statbuf.st_dev, statbuf.st_ino,
			SvPVX(GvSV(curcop->cop_filegv)),
			statbuf.st_uid, statbuf.st_gid);
		    (void)my_pclose(rsfp);
		}
		croak("Permission denied\n");
	    }
	    if (
#ifdef HAS_SETREUID
              setreuid(uid,euid) < 0
#else
# if defined(HAS_SETRESUID)
              setresuid(uid,euid,(Uid_t)-1) < 0
# endif
#endif
              || getuid() != uid || geteuid() != euid)
		croak("Can't reswap uid and euid");
	    if (!cando(S_IXUSR,FALSE,&statbuf))		/* can real uid exec? */
		croak("Permission denied\n");
	}
#endif /* HAS_SETREUID */
#endif /* IAMSUID */

	if (!S_ISREG(statbuf.st_mode))
	    croak("Permission denied");
	if (statbuf.st_mode & S_IWOTH)
	    croak("Setuid/gid script is writable by world");
	doswitches = FALSE;		/* -s is insecure in suid */
	curcop->cop_line++;
	if (fgets(tokenbuf,sizeof tokenbuf, rsfp) == Nullch ||
	  strnNE(tokenbuf,"#!",2) )	/* required even on Sys V */
	    croak("No #! line");
	s = tokenbuf+2;
	if (*s == ' ') s++;
	while (!isSPACE(*s)) s++;
	if (strnNE(s-4,"perl",4) && strnNE(s-9,"perl",4))  /* sanity check */
	    croak("Not a perl script");
	while (*s == ' ' || *s == '\t') s++;
	/*
	 * #! arg must be what we saw above.  They can invoke it by
	 * mentioning suidperl explicitly, but they may not add any strange
	 * arguments beyond what #! says if they do invoke suidperl that way.
	 */
	len = strlen(validarg);
	if (strEQ(validarg," PHOOEY ") ||
	    strnNE(s,validarg,len) || !isSPACE(s[len]))
	    croak("Args must match #! line");

#ifndef IAMSUID
	if (euid != uid && (statbuf.st_mode & S_ISUID) &&
	    euid == statbuf.st_uid)
	    if (!do_undump)
		croak("YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, PUT A C WRAPPER AROUND THIS SCRIPT, OR USE -u AND UNDUMP!\n");
#endif /* IAMSUID */

	if (euid) {	/* oops, we're not the setuid root perl */
	    (void)fclose(rsfp);
#ifndef IAMSUID
	    (void)sprintf(buf, "%s/sperl%s", BIN, patchlevel);
	    execv(buf, origargv);	/* try again */
#endif
	    croak("Can't do setuid\n");
	}

	if (statbuf.st_mode & S_ISGID && statbuf.st_gid != egid) {
#ifdef HAS_SETEGID
	    (void)setegid(statbuf.st_gid);
#else
#ifdef HAS_SETREGID
           (void)setregid((Gid_t)-1,statbuf.st_gid);
#else
#ifdef HAS_SETRESGID
           (void)setresgid((Gid_t)-1,statbuf.st_gid,(Gid_t)-1);
#else
	    setgid(statbuf.st_gid);
#endif
#endif
#endif
	    if (getegid() != statbuf.st_gid)
		croak("Can't do setegid!\n");
	}
	if (statbuf.st_mode & S_ISUID) {
	    if (statbuf.st_uid != euid)
#ifdef HAS_SETEUID
		(void)seteuid(statbuf.st_uid);	/* all that for this */
#else
#ifdef HAS_SETREUID
                (void)setreuid((Uid_t)-1,statbuf.st_uid);
#else
#ifdef HAS_SETRESUID
                (void)setresuid((Uid_t)-1,statbuf.st_uid,(Uid_t)-1);
#else
		setuid(statbuf.st_uid);
#endif
#endif
#endif
	    if (geteuid() != statbuf.st_uid)
		croak("Can't do seteuid!\n");
	}
	else if (uid) {			/* oops, mustn't run as root */
#ifdef HAS_SETEUID
          (void)seteuid((Uid_t)uid);
#else
#ifdef HAS_SETREUID
          (void)setreuid((Uid_t)-1,(Uid_t)uid);
#else
#ifdef HAS_SETRESUID
          (void)setresuid((Uid_t)-1,(Uid_t)uid,(Uid_t)-1);
#else
          setuid((Uid_t)uid);
#endif
#endif
#endif
	    if (geteuid() != uid)
		croak("Can't do seteuid!\n");
	}
	init_ids();
	if (!cando(S_IXUSR,TRUE,&statbuf))
	    croak("Permission denied\n");	/* they can't do this */
    }
#ifdef IAMSUID
    else if (preprocess)
	croak("-P not allowed for setuid/setgid script\n");
    else if (fdscript >= 0)
	croak("fd script not allowed in suidperl\n");
    else
	croak("Script is not setuid/setgid in suidperl\n");

    /* We absolutely must clear out any saved ids here, so we */
    /* exec the real perl, substituting fd script for scriptname. */
    /* (We pass script name as "subdir" of fd, which perl will grok.) */
    rewind(rsfp);
    for (which = 1; origargv[which] && origargv[which] != scriptname; which++) ;
    if (!origargv[which])
	croak("Permission denied");
    (void)sprintf(buf, "/dev/fd/%d/%.127s", fileno(rsfp), origargv[which]);
    origargv[which] = buf;

#if defined(HAS_FCNTL) && defined(F_SETFD)
    fcntl(fileno(rsfp),F_SETFD,0);	/* ensure no close-on-exec */
#endif

    (void)sprintf(tokenbuf, "%s/perl%s", BIN, patchlevel);
    execv(tokenbuf, origargv);	/* try again */
    croak("Can't do setuid\n");
#endif /* IAMSUID */
#else /* !DOSUID */
    if (euid != uid || egid != gid) {	/* (suidperl doesn't exist, in fact) */
#ifndef SETUID_SCRIPTS_ARE_SECURE_NOW
	Fstat(fileno(rsfp),&statbuf);	/* may be either wrapped or real suid */
	if ((euid != uid && euid == statbuf.st_uid && statbuf.st_mode & S_ISUID)
	    ||
	    (egid != gid && egid == statbuf.st_gid && statbuf.st_mode & S_ISGID)
	   )
	    if (!do_undump)
		croak("YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, PUT A C WRAPPER AROUND THIS SCRIPT, OR USE -u AND UNDUMP!\n");
#endif /* SETUID_SCRIPTS_ARE_SECURE_NOW */
	/* not set-id, must be wrapped */
    }
#endif /* DOSUID */
}

static void
find_beginning()
{
    register char *s;

    /* skip forward in input to the real script? */

    taint_not("-x");
    while (doextract) {
	if ((s = sv_gets(linestr, rsfp, 0)) == Nullch)
	    croak("No Perl script found in input\n");
	if (*s == '#' && s[1] == '!' && instr(s,"perl")) {
	    ungetc('\n',rsfp);		/* to keep line count right */
	    doextract = FALSE;
	    if (s = instr(s,"perl -")) {
		s += 6;
		/*SUPPRESS 530*/
		while (s = moreswitches(s)) ;
	    }
	    if (cddir && chdir(cddir) < 0)
		croak("Can't chdir to %s",cddir);
	}
    }
}

static void
init_ids()
{
    uid = (int)getuid();
    euid = (int)geteuid();
    gid = (int)getgid();
    egid = (int)getegid();
#ifdef VMS
    uid |= gid << 16;
    euid |= egid << 16;
#endif
    tainting |= (uid && (euid != uid || egid != gid));
}

static void
init_debugger()
{
    curstash = debstash;
    dbargs = GvAV(gv_AVadd((gv_fetchpv("args", GV_ADDMULTI, SVt_PVAV))));
    AvREAL_off(dbargs);
    DBgv = gv_fetchpv("DB", GV_ADDMULTI, SVt_PVGV);
    DBline = gv_fetchpv("dbline", GV_ADDMULTI, SVt_PVAV);
    DBsub = gv_HVadd(gv_fetchpv("sub", GV_ADDMULTI, SVt_PVHV));
    DBsingle = GvSV((gv_fetchpv("single", GV_ADDMULTI, SVt_PV)));
    sv_setiv(DBsingle, 0); 
    DBtrace = GvSV((gv_fetchpv("trace", GV_ADDMULTI, SVt_PV)));
    sv_setiv(DBtrace, 0); 
    DBsignal = GvSV((gv_fetchpv("signal", GV_ADDMULTI, SVt_PV)));
    sv_setiv(DBsignal, 0); 
    curstash = defstash;
}

static void
init_stacks()
{
    stack = newAV();
    mainstack = stack;			/* remember in case we switch stacks */
    AvREAL_off(stack);			/* not a real array */
    av_extend(stack,127);

    stack_base = AvARRAY(stack);
    stack_sp = stack_base;
    stack_max = stack_base + 127;

    New(54,markstack,64,I32);
    markstack_ptr = markstack;
    markstack_max = markstack + 64;

    New(54,scopestack,32,I32);
    scopestack_ix = 0;
    scopestack_max = 32;

    New(54,savestack,128,ANY);
    savestack_ix = 0;
    savestack_max = 128;

    New(54,retstack,16,OP*);
    retstack_ix = 0;
    retstack_max = 16;

    cxstack_max = 8192 / sizeof(CONTEXT) - 2;	/* Use most of 8K. */
    New(50,cxstack,cxstack_max + 1,CONTEXT);
    cxstack_ix	= -1;

    New(50,tmps_stack,128,SV*);
    tmps_ix = -1;
    tmps_max = 128;

    DEBUG( {
	New(51,debname,128,char);
	New(52,debdelim,128,char);
    } )
}

static FILE *tmpfp;  /* moved outside init_lexer() because of UNICOS bug */
static void
init_lexer()
{
    tmpfp = rsfp;

    lex_start(linestr);
    rsfp = tmpfp;
    subname = newSVpv("main",4);
}

static void
init_predump_symbols()
{
    GV *tmpgv;
    GV *othergv;

    sv_setpvn(GvSV(gv_fetchpv("\"", TRUE, SVt_PV)), " ", 1);

    stdingv = gv_fetchpv("STDIN",TRUE, SVt_PVIO);
    GvMULTI_on(stdingv);
    IoIFP(GvIOp(stdingv)) = stdin;
    tmpgv = gv_fetchpv("stdin",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(GvIOp(stdingv));

    tmpgv = gv_fetchpv("STDOUT",TRUE, SVt_PVIO);
    GvMULTI_on(tmpgv);
    IoOFP(GvIOp(tmpgv)) = IoIFP(GvIOp(tmpgv)) = stdout;
    setdefout(tmpgv);
    tmpgv = gv_fetchpv("stdout",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(GvIOp(defoutgv));

    othergv = gv_fetchpv("STDERR",TRUE, SVt_PVIO);
    GvMULTI_on(othergv);
    IoOFP(GvIOp(othergv)) = IoIFP(GvIOp(othergv)) = stderr;
    tmpgv = gv_fetchpv("stderr",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(GvIOp(othergv));

    statname = NEWSV(66,0);		/* last filename we did stat on */

    osname = savepv(OSNAME);
}

static void
init_postdump_symbols(argc,argv,env)
register int argc;
register char **argv;
register char **env;
{
    char *s;
    SV *sv;
    GV* tmpgv;

    argc--,argv++;	/* skip name of script */
    if (doswitches) {
	for (; argc > 0 && **argv == '-'; argc--,argv++) {
	    if (!argv[0][1])
		break;
	    if (argv[0][1] == '-') {
		argc--,argv++;
		break;
	    }
	    if (s = strchr(argv[0], '=')) {
		*s++ = '\0';
		sv_setpv(GvSV(gv_fetchpv(argv[0]+1,TRUE, SVt_PV)),s);
	    }
	    else
		sv_setiv(GvSV(gv_fetchpv(argv[0]+1,TRUE, SVt_PV)),1);
	}
    }
    toptarget = NEWSV(0,0);
    sv_upgrade(toptarget, SVt_PVFM);
    sv_setpvn(toptarget, "", 0);
    bodytarget = NEWSV(0,0);
    sv_upgrade(bodytarget, SVt_PVFM);
    sv_setpvn(bodytarget, "", 0);
    formtarget = bodytarget;

    tainted = 1;
    if (tmpgv = gv_fetchpv("0",TRUE, SVt_PV)) {
	sv_setpv(GvSV(tmpgv),origfilename);
	magicname("0", "0", 1);
    }
    if (tmpgv = gv_fetchpv("\024",TRUE, SVt_PV))
	time(&basetime);
    if (tmpgv = gv_fetchpv("\030",TRUE, SVt_PV))
	sv_setpv(GvSV(tmpgv),origargv[0]);
    if (argvgv = gv_fetchpv("ARGV",TRUE, SVt_PVAV)) {
	GvMULTI_on(argvgv);
	(void)gv_AVadd(argvgv);
	av_clear(GvAVn(argvgv));
	for (; argc > 0; argc--,argv++) {
	    av_push(GvAVn(argvgv),newSVpv(argv[0],0));
	}
    }
    if (envgv = gv_fetchpv("ENV",TRUE, SVt_PVHV)) {
	HV *hv;
	GvMULTI_on(envgv);
	hv = GvHVn(envgv);
	hv_clear(hv);
#ifndef VMS  /* VMS doesn't have environ array */
	/* Note that if the supplied env parameter is actually a copy
	   of the global environ then it may now point to free'd memory
	   if the environment has been modified since. To avoid this
	   problem we treat env==NULL as meaning 'use the default'
	*/
	if (!env)
	    env = environ;
	if (env != environ) {
	    environ[0] = Nullch;
	    hv_magic(hv, envgv, 'E');
	}
	for (; *env; env++) {
	    if (!(s = strchr(*env,'=')))
		continue;
	    *s++ = '\0';
	    sv = newSVpv(s--,0);
	    sv_magic(sv, sv, 'e', *env, s - *env);
	    (void)hv_store(hv, *env, s - *env, sv, 0);
	    *s = '=';
	}
#endif
#ifdef DYNAMIC_ENV_FETCH
	HvNAME(hv) = savepv(ENV_HV_NAME);
#endif
	hv_magic(hv, envgv, 'E');
    }
    tainted = 0;
    if (tmpgv = gv_fetchpv("$",TRUE, SVt_PV))
	sv_setiv(GvSV(tmpgv),(I32)getpid());

}

static void
init_perllib()
{
    char *s;
    if (!tainting) {
	s = getenv("PERL5LIB");
	if (s)
	    incpush(s);
	else
	    incpush(getenv("PERLLIB"));
    }

#ifdef APPLLIB_EXP
    incpush(APPLLIB_EXP);
#endif

#ifdef ARCHLIB_EXP
    incpush(ARCHLIB_EXP);
#endif
#ifndef PRIVLIB_EXP
#define PRIVLIB_EXP "/usr/local/lib/perl5:/usr/local/lib/perl"
#endif
    incpush(PRIVLIB_EXP);

#ifdef SITEARCH_EXP
    incpush(SITEARCH_EXP);
#endif
#ifdef SITELIB_EXP
    incpush(SITELIB_EXP);
#endif
#ifdef OLDARCHLIB_EXP  /* 5.00[01] compatibility */
    incpush(OLDARCHLIB_EXP);
#endif
    
    if (!tainting)
	incpush(".");
}

void
calllist(list)
AV* list;
{
    Sigjmp_buf oldtop;
    STRLEN len;
    line_t oldline = curcop->cop_line;

    Copy(top_env, oldtop, 1, Sigjmp_buf);

    while (AvFILL(list) >= 0) {
	CV *cv = (CV*)av_shift(list);

	SAVEFREESV(cv);

	switch (Sigsetjmp(top_env,1)) {
	case 0: {
		SV* atsv = GvSV(errgv);
		PUSHMARK(stack_sp);
		perl_call_sv((SV*)cv, G_EVAL|G_DISCARD);
		(void)SvPV(atsv, len);
		if (len) {
		    Copy(oldtop, top_env, 1, Sigjmp_buf);
		    curcop = &compiling;
		    curcop->cop_line = oldline;
		    if (list == beginav)
			sv_catpv(atsv, "BEGIN failed--compilation aborted");
		    else
			sv_catpv(atsv, "END failed--cleanup aborted");
		    croak("%s", SvPVX(atsv));
		}
	    }
	    break;
	case 1:
#ifdef VMS
	    statusvalue = 255;	/* XXX I don't think we use 1 anymore. */
#else
	statusvalue = 1;
#endif
	    /* FALL THROUGH */
	case 2:
	    /* my_exit() was called */
	    curstash = defstash;
	    if (endav)
		calllist(endav);
	    FREETMPS;
	    Copy(oldtop, top_env, 1, Sigjmp_buf);
	    curcop = &compiling;
	    curcop->cop_line = oldline;
	    if (statusvalue) {
		if (list == beginav)
		    croak("BEGIN failed--compilation aborted");
		else
		    croak("END failed--cleanup aborted");
	    }
	    my_exit(statusvalue);
	    /* NOTREACHED */
	    return;
	case 3:
	    if (!restartop) {
		fprintf(stderr, "panic: restartop\n");
		FREETMPS;
		break;
	    }
	    Copy(oldtop, top_env, 1, Sigjmp_buf);
	    curcop = &compiling;
	    curcop->cop_line = oldline;
	    Siglongjmp(top_env, 3);
	}
    }

    Copy(oldtop, top_env, 1, Sigjmp_buf);
}

