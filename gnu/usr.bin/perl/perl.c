/*    perl.c
 *
 *    Copyright (c) 1987-1997 Larry Wall
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

/* XXX If this causes problems, set i_unistd=undef in the hint file.  */
#ifdef I_UNISTD
#include <unistd.h>
#endif

#if !defined(STANDARD_C) && !defined(HAS_GETENV_PROTOTYPE)
char *getenv _((char *)); /* Usually in <stdlib.h> */
#endif

dEXTCONST char rcsid[] = "perl.c\nPatch level: ###\n";

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

#define I_REINIT \
  STMT_START {			\
    chopset	= " \n-";	\
    copline	= NOLINE;	\
    curcop	= &compiling;	\
    curcopdb    = NULL;		\
    cxstack_ix  = -1;		\
    cxstack_max = 128;		\
    dbargs	= 0;		\
    dlmax	= 128;		\
    laststatval	= -1;		\
    laststype	= OP_STAT;	\
    maxscream	= -1;		\
    maxsysfd	= MAXSYSFD;	\
    statname	= Nullsv;	\
    tmps_floor	= -1;		\
    tmps_ix     = -1;		\
    op_mask     = NULL;		\
    dlmax       = 128;		\
    laststatval = -1;		\
    laststype   = OP_STAT;	\
    mess_sv     = Nullsv;	\
  } STMT_END

static void find_beginning _((void));
static void forbid_setid _((char *));
static void incpush _((char *, int));
static void init_ids _((void));
static void init_debugger _((void));
static void init_lexer _((void));
static void init_main_stash _((void));
static void init_perllib _((void));
static void init_postdump_symbols _((int, char **, char **));
static void init_predump_symbols _((void));
static void init_stacks _((void));
static void my_exit_jump _((void)) __attribute__((noreturn));
static void nuke_stacks _((void));
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

	if (!SvREADONLY(&sv_undef)) {
	    SvREADONLY_on(&sv_undef);

	    sv_setpv(&sv_no,No);
	    SvNV(&sv_no);
	    SvREADONLY_on(&sv_no);

	    sv_setpv(&sv_yes,Yes);
	    SvNV(&sv_yes);
	    SvREADONLY_on(&sv_yes);
	}

	nrs = newSVpv("\n", 1);
	rs = SvREFCNT_inc(nrs);

	pidstatus = newHV();

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
    I_REINIT;
    perl_destruct_level = 1; 
#else
   if(perl_destruct_level > 0)
       I_REINIT;
#endif

    init_ids();
    lex_state = LEX_NOTPARSING;

    start_env.je_prev = NULL;
    start_env.je_ret = -1;
    start_env.je_mustcatch = TRUE;
    top_env     = &start_env;
    STATUS_ALL_SUCCESS;

    SET_NUMERIC_STANDARD();
#if defined(SUBVERSION) && SUBVERSION > 0
    sprintf(patchlevel, "%7.5f",   (double) 5 
				+ ((double) PATCHLEVEL / (double) 1000)
				+ ((double) SUBVERSION / (double) 100000));
#else
    sprintf(patchlevel, "%5.3f", (double) 5 +
				((double) PATCHLEVEL / (double) 1000));
#endif

#if defined(LOCAL_PATCH_COUNT)
    localpatches = local_patches;	/* For possible -v */
#endif

    PerlIO_init();      /* Hook to IO system */

    fdpid = newAV();	/* for remembering popen pids by fd */

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
	if (s = getenv("PERL_DESTRUCT_LEVEL")) {
	    int i = atoi(s);
	    if (destruct_level < i)
		destruct_level = i;
	}
    }
#endif

    LEAVE;
    FREETMPS;

    /* We must account for everything.  */

    /* Destroy the main CV and syntax tree */
    if (main_root) {
	curpad = AvARRAY(comppad);
	op_free(main_root);
	main_root = Nullop;
    }
    main_start = Nullop;
    SvREFCNT_dec(main_cv);
    main_cv = Nullcv;

    if (sv_objcount) {
	/*
	 * Try to destruct global references.  We do this first so that the
	 * destructors and destructees still exist.  Some sv's might remain.
	 * Non-referenced objects are on their own.
	 */
    
	dirty = TRUE;
	sv_clean_objs();
    }

    /* unhook hooks which will soon be, or use, destroyed data */
    SvREFCNT_dec(warnhook);
    warnhook = Nullsv;
    SvREFCNT_dec(diehook);
    diehook = Nullsv;
    SvREFCNT_dec(parsehook);
    parsehook = Nullsv;

    if (destruct_level == 0){

	DEBUG_P(debprofdump());
    
	/* The exit() function will do everything that needs doing. */
	return;
    }

    /* loosen bonds of global variables */

    if(rsfp) {
	(void)PerlIO_close(rsfp);
	rsfp = Nullfp;
    }

    /* Filters for program text */
    SvREFCNT_dec(rsfp_filters);
    rsfp_filters = Nullav;

    /* switches */
    preprocess   = FALSE;
    minus_n      = FALSE;
    minus_p      = FALSE;
    minus_l      = FALSE;
    minus_a      = FALSE;
    minus_F      = FALSE;
    doswitches   = FALSE;
    dowarn       = FALSE;
    doextract    = FALSE;
    sawampersand = FALSE;	/* must save all match strings */
    sawstudy     = FALSE;	/* do fbm_instr on all strings */
    sawvec       = FALSE;
    unsafe       = FALSE;

    Safefree(inplace);
    inplace = Nullch;

    Safefree(e_tmpname);
    e_tmpname = Nullch;

    if (e_fp) {
	PerlIO_close(e_fp);
	e_fp = Nullfp;
    }

    /* magical thingies */

    Safefree(ofs);	/* $, */
    ofs = Nullch;

    Safefree(ors);	/* $\ */
    ors = Nullch;

    SvREFCNT_dec(nrs);	/* $\ helper */
    nrs = Nullsv;

    multiline = 0;	/* $* */

    SvREFCNT_dec(statname);
    statname = Nullsv;
    statgv = Nullgv;

    /* defgv, aka *_ should be taken care of elsewhere */

#if 0  /* just about all regexp stuff, seems to be ok */

    /* shortcuts to regexp stuff */
    leftgv = Nullgv;
    ampergv = Nullgv;

    SAVEFREEOP(curpm);
    SAVEFREEOP(oldlastpm); /* for saving regexp context during debugger */

    regprecomp = NULL;	/* uncompiled string. */
    regparse = NULL;	/* Input-scan pointer. */
    regxend = NULL;	/* End of input for compile */
    regnpar = 0;	/* () count. */
    regcode = NULL;	/* Code-emit pointer; &regdummy = don't. */
    regsize = 0;	/* Code size. */
    regnaughty = 0;	/* How bad is this pattern? */
    regsawback = 0;	/* Did we see \1, ...? */

    reginput = NULL;		/* String-input pointer. */
    regbol = NULL;		/* Beginning of input, for ^ check. */
    regeol = NULL;		/* End of input, for $ check. */
    regstartp = (char **)NULL;	/* Pointer to startp array. */
    regendp = (char **)NULL;	/* Ditto for endp. */
    reglastparen = 0;		/* Similarly for lastparen. */
    regtill = NULL;		/* How far we are required to go. */
    regflags = 0;		/* are we folding, multilining? */
    regprev = (char)NULL;	/* char before regbol, \n if none */

#endif /* if 0 */

    /* clean up after study() */
    SvREFCNT_dec(lastscream);
    lastscream = Nullsv;
    Safefree(screamfirst);
    screamfirst = 0;
    Safefree(screamnext);
    screamnext  = 0;

    /* startup and shutdown function lists */
    SvREFCNT_dec(beginav);
    SvREFCNT_dec(endav);
    beginav = Nullav;
    endav = Nullav;

    /* temp stack during pp_sort() */
    SvREFCNT_dec(sortstack);
    sortstack = Nullav;

    /* shortcuts just get cleared */
    envgv = Nullgv;
    siggv = Nullgv;
    incgv = Nullgv;
    errgv = Nullgv;
    argvgv = Nullgv;
    argvoutgv = Nullgv;
    stdingv = Nullgv;
    last_in_gv = Nullgv;

    /* reset so print() ends up where we expect */
    setdefout(Nullgv);

    /* Prepare to destruct main symbol table.  */

    hv = defstash;
    defstash = 0;
    SvREFCNT_dec(hv);

    FREETMPS;
    if (destruct_level >= 2) {
	if (scopestack_ix != 0)
	    warn("Unbalanced scopes: %ld more ENTERs than LEAVEs\n",
		 (long)scopestack_ix);
	if (savestack_ix != 0)
	    warn("Unbalanced saves: %ld more saves than restores\n",
		 (long)savestack_ix);
	if (tmps_floor != -1)
	    warn("Unbalanced tmps: %ld more allocs than frees\n",
		 (long)tmps_floor + 1);
	if (cxstack_ix != -1)
	    warn("Unbalanced context: %ld more PUSHes than POPs\n",
		 (long)cxstack_ix + 1);
    }

    /* Now absolutely destruct everything, somehow or other, loops or no. */
    last_sv_count = 0;
    SvFLAGS(strtab) |= SVTYPEMASK;		/* don't clean out strtab now */
    while (sv_count != 0 && sv_count != last_sv_count) {
	last_sv_count = sv_count;
	sv_clean_all();
    }
    SvFLAGS(strtab) &= ~SVTYPEMASK;
    SvFLAGS(strtab) |= SVt_PVHV;
    
    /* Destruct the global string table. */
    {
	/* Yell and reset the HeVAL() slots that are still holding refcounts,
	 * so that sv_free() won't fail on them.
	 */
	I32 riter;
	I32 max;
	HE *hent;
	HE **array;

	riter = 0;
	max = HvMAX(strtab);
	array = HvARRAY(strtab);
	hent = array[0];
	for (;;) {
	    if (hent) {
		warn("Unbalanced string table refcount: (%d) for \"%s\"",
		     HeVAL(hent) - Nullsv, HeKEY(hent));
		HeVAL(hent) = Nullsv;
		hent = HeNEXT(hent);
	    }
	    if (!hent) {
		if (++riter > max)
		    break;
		hent = array[riter];
	    }
	}
    }
    SvREFCNT_dec(strtab);

    if (sv_count != 0)
	warn("Scalars leaked: %ld\n", (long)sv_count);

    sv_free_arenas();

    /* No SVs have survived, need to clean out */
    linestr = NULL;
    pidstatus = Nullhv;
    if (origfilename)
    	Safefree(origfilename);
    nuke_stacks();
    hints = 0;		/* Reset hints. Should hints be per-interpreter ? */
    
    DEBUG_P(debprofdump());

    /* As the absolutely last thing, free the non-arena SV for mess() */

    if (mess_sv) {
	/* we know that type >= SVt_PV */
	SvOOK_off(mess_sv);
	Safefree(SvPVX(mess_sv));
	Safefree(SvANY(mess_sv));
	Safefree(mess_sv);
	mess_sv = Nullsv;
    }
}

void
perl_free(sv_interp)
PerlInterpreter *sv_interp;
{
    if (!(curinterp = sv_interp))
	return;
    Safefree(sv_interp);
}

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
    I32 oldscope;
    AV* comppadlist;
    dJMPENV;
    int ret;

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
#ifdef IAMSUID
#undef IAMSUID
    croak("suidperl is no longer needed since the kernel can now execute\n\
setuid perl scripts securely.\n");
#endif
#endif

    if (!(curinterp = sv_interp))
	return 255;

#if defined(NeXT) && defined(__DYNAMIC__)
    _dyld_lookup_and_bind
	("__environ", (unsigned long *) &environ_pointer, NULL);
#endif /* environ */

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

    if (main_root) {
	curpad = AvARRAY(comppad);
	op_free(main_root);
	main_root = Nullop;
    }
    main_start = Nullop;
    SvREFCNT_dec(main_cv);
    main_cv = Nullcv;

    time(&basetime);
    oldscope = scopestack_ix;

    JMPENV_PUSH(ret);
    switch (ret) {
    case 1:
	STATUS_ALL_FAILURE;
	/* FALL THROUGH */
    case 2:
	/* my_exit() was called */
	while (scopestack_ix > oldscope)
	    LEAVE;
	FREETMPS;
	curstash = defstash;
	if (endav)
	    call_list(oldscope, endav);
	JMPENV_POP;
	return STATUS_NATIVE_EXPORT;
    case 3:
	JMPENV_POP;
	PerlIO_printf(PerlIO_stderr(), "panic: top_env\n");
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
	case 'u':
	case 'U':
	case 'v':
	case 'w':
	    if (s = moreswitches(s))
		goto reswitch;
	    break;

	case 'T':
	    tainting = TRUE;
	    s++;
	    goto reswitch;

	case 'e':
	    if (euid != uid || egid != gid)
		croak("No -e allowed in setuid scripts");
	    if (!e_fp) {
		int fd;

	        e_tmpname = savepv(TMPPATH);
		fd = mkstemp(e_tmpname);
		if (fd == -1)
		    croak("Can't mkstemp()");
		e_fp = PerlIO_fdopen(fd,"w");
		if (!e_fp) {
		    (void)close(fd);
		    croak("Cannot open temporary file");
		}
	    }
	    if (*++s)
		PerlIO_puts(e_fp,s);
	    else if (argv[1]) {
		PerlIO_puts(e_fp,argv[1]);
		argc--,argv++;
	    }
	    else
		croak("No code specified for -e");
	    (void)PerlIO_putc(e_fp,'\n');
	    break;
	case 'I':	/* -I handled both here and in moreswitches() */
	    forbid_setid("-I");
	    if (!*++s && (s=argv[1]) != Nullch) {
		argc--,argv++;
	    }
	    while (s && isSPACE(*s))
		++s;
	    if (s && *s) {
		char *e, *p;
		for (e = s; *e && !isSPACE(*e); e++) ;
		p = savepvn(s, e-s);
		incpush(p, TRUE);
		sv_catpv(sv,"-I");
		sv_catpv(sv,p);
		sv_catpv(sv," ");
		Safefree(p);
	    }	/* XXX else croak? */
	    break;
	case 'P':
	    forbid_setid("-P");
	    preprocess = TRUE;
	    s++;
	    goto reswitch;
	case 'S':
	    forbid_setid("-S");
	    dosearch = TRUE;
	    s++;
	    goto reswitch;
	case 'V':
	    if (!preambleav)
		preambleav = newAV();
	    av_push(preambleav, newSVpv("use Config qw(myconfig config_vars)",0));
	    if (*++s != ':')  {
		Sv = newSVpv("print myconfig();",0);
#ifdef VMS
		sv_catpv(Sv,"print \"\\nCharacteristics of this PERLSHR image: \\n\",");
#else
		sv_catpv(Sv,"print \"\\nCharacteristics of this binary (from libperl): \\n\",");
#endif
#if defined(DEBUGGING) || defined(NO_EMBED) || defined(MULTIPLICITY)
		sv_catpv(Sv,"\"  Compile-time options:");
#  ifdef DEBUGGING
		sv_catpv(Sv," DEBUGGING");
#  endif
#  ifdef NO_EMBED
		sv_catpv(Sv," NO_EMBED");
#  endif
#  ifdef MULTIPLICITY
		sv_catpv(Sv," MULTIPLICITY");
#  endif
		sv_catpv(Sv,"\\n\",");
#endif
#if defined(LOCAL_PATCH_COUNT)
		if (LOCAL_PATCH_COUNT > 0) {
		    int i;
		    sv_catpv(Sv,"\"  Locally applied patches:\\n\",");
		    for (i = 1; i <= LOCAL_PATCH_COUNT; i++) {
			if (localpatches[i])
			    sv_catpvf(Sv,"\"  \\t%s\\n\",",localpatches[i]);
		    }
		}
#endif
		sv_catpvf(Sv,"\"  Built under %s\\n\"",OSNAME);
#ifdef __DATE__
#  ifdef __TIME__
		sv_catpvf(Sv,",\"  Compiled at %s %s\\n\"",__DATE__,__TIME__);
#  else
		sv_catpvf(Sv,",\"  Compiled on %s\\n\"",__DATE__);
#  endif
#endif
		sv_catpv(Sv, "; \
$\"=\"\\n    \"; \
@env = map { \"$_=\\\"$ENV{$_}\\\"\" } sort grep {/^PERL/} keys %ENV; \
print \"  \\%ENV:\\n    @env\\n\" if @env; \
print \"  \\@INC:\\n    @INC\\n\";");
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
	case 0:
	    break;
	case '-':
	    if (!*++s || isSPACE(*s)) {
		argc--,argv++;
		goto switch_end;
	    }
	    /* catch use of gnu style long options */
	    if (strEQ(s, "version")) {
		s = "v";
		goto reswitch;
	    }
	    if (strEQ(s, "help")) {
		s = "h";
		goto reswitch;
	    }
	    s--;
	    /* FALL THROUGH */
	default:
	    croak("Unrecognized switch: -%s  (-h will show valid options)",s);
	}
    }
  switch_end:

    if (!tainting && (s = getenv("PERL5OPT"))) {
	while (s && *s) {
	    while (isSPACE(*s))
		s++;
	    if (*s == '-') {
		s++;
		if (isSPACE(*s))
		    continue;
	    }
	    if (!*s)
		break;
	    if (!strchr("DIMUdmw", *s))
		croak("Illegal switch in PERL5OPT: -%c", *s);
	    s = moreswitches(s);
	}
    }

    if (!scriptname)
	scriptname = argv[0];
    if (e_fp) {
	if (PerlIO_flush(e_fp) || PerlIO_error(e_fp) || PerlIO_close(e_fp)) {
#ifndef MULTIPLICITY
	    warn("Did you forget to compile with -DMULTIPLICITY?");
#endif	    
	    croak("Can't write to temp file for -e: %s", Strerror(errno));
	}
	e_fp = Nullfp;
	argc++,argv--;
	scriptname = e_tmpname;
    }
    else if (scriptname == Nullch) {
#ifdef MSDOS
	if ( isatty(PerlIO_fileno(PerlIO_stdin())) )
	    moreswitches("h");
#endif
	scriptname = "-";
    }

    init_perllib();

    open_script(scriptname,dosearch,sv);

    validate_suid(validarg, scriptname);

    if (doextract)
	find_beginning();

    main_cv = compcv = (CV*)NEWSV(1104,0);
    sv_upgrade((SV *)compcv, SVt_PVCV);
    CvUNIQUE_on(compcv);

    comppad = newAV();
    av_push(comppad, Nullsv);
    curpad = AvARRAY(comppad);
    comppad_name = newAV();
    comppad_name_fill = 0;
    min_intro_pending = 0;
    padix = 0;

    comppadlist = newAV();
    AvREAL_off(comppadlist);
    av_store(comppadlist, 0, (SV*)comppad_name);
    av_store(comppadlist, 1, (SV*)comppad);
    CvPADLIST(compcv) = comppadlist;

    boot_core_UNIVERSAL();
    if (xsinit)
	(*xsinit)();	/* in case linked C routines want magical variables */
#if defined(VMS) || defined(WIN32)
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

#ifdef MYMALLOC
    if ((s=getenv("PERL_DEBUG_MSTATS")) && atoi(s) >= 2)
	dump_mstats("after compilation:");
#endif

    ENTER;
    restartop = 0;
    JMPENV_POP;
    return 0;
}

int
perl_run(sv_interp)
PerlInterpreter *sv_interp;
{
    I32 oldscope;
    dJMPENV;
    int ret;

    if (!(curinterp = sv_interp))
	return 255;

    oldscope = scopestack_ix;

    JMPENV_PUSH(ret);
    switch (ret) {
    case 1:
	cxstack_ix = -1;		/* start context stack again */
	break;
    case 2:
	/* my_exit() was called */
	while (scopestack_ix > oldscope)
	    LEAVE;
	FREETMPS;
	curstash = defstash;
	if (endav)
	    call_list(oldscope, endav);
#ifdef MYMALLOC
	if (getenv("PERL_DEBUG_MSTATS"))
	    dump_mstats("after execution:  ");
#endif
	JMPENV_POP;
	return STATUS_NATIVE_EXPORT;
    case 3:
	if (!restartop) {
	    PerlIO_printf(PerlIO_stderr(), "panic: restartop\n");
	    FREETMPS;
	    JMPENV_POP;
	    return 1;
	}
	if (curstack != mainstack) {
	    dSP;
	    SWITCHSTACK(curstack, mainstack);
	}
	break;
    }

    DEBUG_r(PerlIO_printf(Perl_debug_log, "%s $` $& $' support.\n",
                    sawampersand ? "Enabling" : "Omitting"));

    if (!restartop) {
	DEBUG_x(dump_all());
	DEBUG(PerlIO_printf(Perl_debug_log, "\nEXECUTING...\n\n"));

	if (minus_c) {
	    PerlIO_printf(PerlIO_stderr(), "%s syntax OK\n", origfilename);
	    my_exit(0);
	}
	if (PERLDB_SINGLE && DBsingle)
	   sv_setiv(DBsingle, 1); 
    }

    /* do it */

    if (restartop) {
	op = restartop;
	restartop = 0;
	runops();
    }
    else if (main_start) {
	CvDEPTH(main_cv) = 1;
	op = main_start;
	runops();
    }

    my_exit(0);
    /* NOTREACHED */
    return 0;
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
    if (create && !GvCVu(gv))
    	return newSUB(start_subparse(FALSE, 0),
		      newSVOP(OP_CONST, 0, newSVpv(name,0)),
		      Nullop,
		      Nullop);
    if (gv)
	return GvCVu(gv);
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
    I32 oldmark;
    I32 retval;
    I32 oldscope;
    static CV *DBcv;
    bool oldcatch = CATCH_GET;
    dJMPENV;
    int ret;
    OP* oldop = op;

    if (flags & G_DISCARD) {
	ENTER;
	SAVETMPS;
    }

    Zero(&myop, 1, LOGOP);
    myop.op_next = Nullop;
    if (!(flags & G_NOARGS))
	myop.op_flags |= OPf_STACKED;
    myop.op_flags |= ((flags & G_VOID) ? OPf_WANT_VOID :
		      (flags & G_ARRAY) ? OPf_WANT_LIST :
		      OPf_WANT_SCALAR);
    SAVESPTR(op);
    op = (OP*)&myop;

    EXTEND(stack_sp, 1);
    *++stack_sp = sv;
    oldmark = TOPMARK;
    oldscope = scopestack_ix;

    if (PERLDB_SUB && curstash != debstash
	   /* Handle first BEGIN of -d. */
	  && (DBcv || (DBcv = GvCV(DBsub)))
	   /* Try harder, since this may have been a sighandler, thus
	    * curstash may be meaningless. */
	  && (SvTYPE(sv) != SVt_PVCV || CvSTASH((CV*)sv) != debstash))
	op->op_private |= OPpENTERSUB_DB;

    if (flags & G_EVAL) {
	cLOGOP->op_other = op;
	markstack_ptr--;
	/* we're trying to emulate pp_entertry() here */
	{
	    register CONTEXT *cx;
	    I32 gimme = GIMME_V;
	    
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

	JMPENV_PUSH(ret);
	switch (ret) {
	case 0:
	    break;
	case 1:
	    STATUS_ALL_FAILURE;
	    /* FALL THROUGH */
	case 2:
	    /* my_exit() was called */
	    curstash = defstash;
	    FREETMPS;
	    JMPENV_POP;
	    if (statusvalue)
		croak("Callback called exit");
	    my_exit_jump();
	    /* NOTREACHED */
	case 3:
	    if (restartop) {
		op = restartop;
		restartop = 0;
		break;
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
    else
	CATCH_SET(TRUE);

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
	JMPENV_POP;
    }
    else
	CATCH_SET(oldcatch);

    if (flags & G_DISCARD) {
	stack_sp = stack_base + oldmark;
	retval = 0;
	FREETMPS;
	LEAVE;
    }
    op = oldop;
    return retval;
}

/* Eval a string. The G_EVAL flag is always assumed. */

I32
perl_eval_sv(sv, flags)
SV* sv;
I32 flags;		/* See G_* flags in cop.h */
{
    UNOP myop;		/* fake syntax tree node */
    SV** sp = stack_sp;
    I32 oldmark = sp - stack_base;
    I32 retval;
    I32 oldscope;
    dJMPENV;
    int ret;
    OP* oldop = op;

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
    myop.op_type = OP_ENTEREVAL;
    myop.op_flags |= ((flags & G_VOID) ? OPf_WANT_VOID :
		      (flags & G_ARRAY) ? OPf_WANT_LIST :
		      OPf_WANT_SCALAR);
    if (flags & G_KEEPERR)
	myop.op_flags |= OPf_SPECIAL;

    JMPENV_PUSH(ret);
    switch (ret) {
    case 0:
	break;
    case 1:
	STATUS_ALL_FAILURE;
	/* FALL THROUGH */
    case 2:
	/* my_exit() was called */
	curstash = defstash;
	FREETMPS;
	JMPENV_POP;
	if (statusvalue)
	    croak("Callback called exit");
	my_exit_jump();
	/* NOTREACHED */
    case 3:
	if (restartop) {
	    op = restartop;
	    restartop = 0;
	    break;
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
    if (!(flags & G_KEEPERR))
	sv_setpv(GvSV(errgv),"");

  cleanup:
    JMPENV_POP;
    if (flags & G_DISCARD) {
	stack_sp = stack_base + oldmark;
	retval = 0;
	FREETMPS;
	LEAVE;
    }
    op = oldop;
    return retval;
}

SV*
perl_eval_pv(p, croak_on_error)
char* p;
I32 croak_on_error;
{
    dSP;
    SV* sv = newSVpv(p, 0);

    PUSHMARK(sp);
    perl_eval_sv(sv, G_SCALAR);
    SvREFCNT_dec(sv);

    SPAGAIN;
    sv = POPs;
    PUTBACK;

    if (croak_on_error && SvTRUE(GvSV(errgv)))
	croak(SvPVx(GvSV(errgv), na));

    return sv;
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

static void
usage(name)		/* XXX move this out into a module ? */
char *name;
{
    /* This message really ought to be max 23 lines.
     * Removed -h because the user already knows that opton. Others? */

    static char *usage[] = {
"-0[octal]       specify record separator (\\0, if no argument)",
"-a              autosplit mode with -n or -p (splits $_ into @F)",
"-c              check syntax only (runs BEGIN and END blocks)",
"-d[:debugger]   run scripts under debugger",
"-D[number/list] set debugging flags (argument is a bit mask or flags)",
"-e 'command'    one line of script. Several -e's allowed. Omit [programfile].",
"-F/pattern/     split() pattern for autosplit (-a). The //'s are optional.",
"-i[extension]   edit <> files in place (make backup if extension supplied)",
"-Idirectory     specify @INC/#include directory (may be used more than once)",
"-l[octal]       enable line ending processing, specifies line terminator",
"-[mM][-]module.. executes `use/no module...' before executing your script.",
"-n              assume 'while (<>) { ... }' loop around your script",
"-p              assume loop like -n but print line also like sed",
"-P              run script through C preprocessor before compilation",
"-s              enable some switch parsing for switches after script name",
"-S              look for the script using PATH environment variable",
"-T              turn on tainting checks",
"-u              dump core after parsing script",
"-U              allow unsafe operations",
"-v              print version number and patchlevel of perl",
"-V[:variable]   print perl configuration information",
"-w              TURN WARNINGS ON FOR COMPILATION OF YOUR SCRIPT. Recommended.",
"-x[directory]   strip off text before #!perl line and perhaps cd to directory",
"\n",
NULL
};
    char **p = usage;

    printf("\nUsage: %s [switches] [--] [programfile] [arguments]", name);
    while (*p)
	printf("\n  %s", *p++);
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
	forbid_setid("-d");
	s++;
	if (*s == ':' || *s == '=')  {
	    my_setenv("PERL5DB", form("use Devel::%s;", ++s));
	    s += strlen(s);
	}
	if (!perldb) {
	    perldb = PERLDB_ALL;
	    init_debugger();
	}
	return s;
    case 'D':
#ifdef DEBUGGING
	forbid_setid("-D");
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
	if (*s)
	    *s++ = '\0';
	return s;
    case 'I':	/* -I handled both here and in parse_perl() */
	forbid_setid("-I");
	++s;
	while (*s && isSPACE(*s))
	    ++s;
	if (*s) {
	    char *e, *p;
	    for (e = s; *e && !isSPACE(*e); e++) ;
	    p = savepvn(s, e-s);
	    incpush(p, TRUE);
	    Safefree(p);
	    s = e;
	}
	else
	    croak("No space allowed after -I");
	return s;
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
		ors = "\n\n";
		orslen = 2;
	    }
	    else
		ors = SvPV(nrs, orslen);
	    ors = savepvn(ors, orslen);
	}
	return s;
    case 'M':
	forbid_setid("-M");	/* XXX ? */
	/* FALL THROUGH */
    case 'm':
	forbid_setid("-m");	/* XXX ? */
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
	forbid_setid("-s");
	doswitches = TRUE;
	s++;
	return s;
    case 'T':
	if (!tainting)
	    croak("Too late for \"-T\" option");
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
	printf("\nThis is perl, version 5.%03d_%02d built for %s",
	    PATCHLEVEL, SUBVERSION, ARCHNAME);
#else
	printf("\nThis is perl, version %s built for %s",
		patchlevel, ARCHNAME);
#endif
#if defined(LOCAL_PATCH_COUNT)
	if (LOCAL_PATCH_COUNT > 0)
	    printf("\n(with %d registered patch%s, see perl -V for more detail)",
		LOCAL_PATCH_COUNT, (LOCAL_PATCH_COUNT!=1) ? "es" : "");
#endif

	printf("\n\nCopyright 1987-1997, Larry Wall\n");
#ifdef MSDOS
	printf("\nMS-DOS port Copyright (c) 1989, 1990, Diomidis Spinellis\n");
#endif
#ifdef DJGPP
	printf("djgpp v2 port (jpl5003c) by Hirofumi Watanabe, 1996\n");
#endif
#ifdef OS2
	printf("\n\nOS/2 port Copyright (c) 1990, 1991, Raymond Chen, Kai Uwe Rommel\n"
	    "Version 5 port Copyright (c) 1994-1997, Andreas Kaiser, Ilya Zakharevich\n");
#endif
#ifdef atarist
	printf("atariST series port, ++jrb  bammi@cadence.com\n");
#endif
	printf("\n\
Perl may be copied only under the terms of either the Artistic License or the\n\
GNU General Public License, which may be found in the Perl 5.0 source kit.\n\n");
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
#ifdef ALTERNATE_SHEBANG
    case 'S':			/* OS/2 needs -S on "extproc" line. */
	break;
#endif
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
    SV*    prog;
    SV*    file;
    int    status;
    extern int etext;

    prog = newSVpv(BIN_EXP);
    sv_catpv(prog, "/perl");
    file = newSVpv(origfilename);
    sv_catpv(file, ".perldump");

    status = unexec(SvPVX(file), SvPVX(prog), &etext, sbrk(0), 0);
    if (status)
	PerlIO_printf(PerlIO_stderr(), "unexec of %s into %s failed!\n",
		      SvPVX(prog), SvPVX(file));
    exit(status);
#else
#  ifdef VMS
#    include <lib$routines.h>
     lib$signal(SS$_DEBUG);  /* ssdef.h #included from vmsish.h */
#  else
    ABORT();		/* for use with undump */
#  endif
#endif
}

static void
init_main_stash()
{
    GV *gv;

    /* Note that strtab is a rather special HV.  Assumptions are made
       about not iterating on it, and not adding tie magic to it.
       It is properly deallocated in perl_destruct() */
    strtab = newHV();
    HvSHAREKEYS_off(strtab);			/* mandatory */
    Newz(506,((XPVHV*)SvANY(strtab))->xhv_array,
	 sizeof(HE*) * (((XPVHV*)SvANY(strtab))->xhv_max + 1), char);
    
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
    (void)form("%240s","");	/* Preallocate temp - for immediate signals. */
    sv_grow(GvSV(errgv), 240);	/* Preallocate - for immediate signals. */
    sv_setpvn(GvSV(errgv), "", 0);
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
#  define SEARCH_EXTS ".bat", ".cmd", NULL
#  define MAX_EXT_LEN 4
#endif
#ifdef OS2
#  define SEARCH_EXTS ".cmd", ".btm", ".bat", ".pl", NULL
#  define MAX_EXT_LEN 4
#endif
#ifdef VMS
#  define SEARCH_EXTS ".pl", ".com", NULL
#  define MAX_EXT_LEN 4
#endif
    /* additional extensions to try in each dir if scriptname not found */
#ifdef SEARCH_EXTS
    char *ext[] = { SEARCH_EXTS };
    int extidx = 0, i = 0;
    char *curext = Nullch;
#else
#  define MAX_EXT_LEN 0
#endif

    /*
     * If dosearch is true and if scriptname does not contain path
     * delimiters, search the PATH for scriptname.
     *
     * If SEARCH_EXTS is also defined, will look for each
     * scriptname{SEARCH_EXTS} whenever scriptname is not found
     * while searching the PATH.
     *
     * Assuming SEARCH_EXTS is C<".foo",".bar",NULL>, PATH search
     * proceeds as follows:
     *   If DOSISH:
     *     + look for ./scriptname{,.foo,.bar}
     *     + search the PATH for scriptname{,.foo,.bar}
     *
     *   If !DOSISH:
     *     + look *only* in the PATH for scriptname{,.foo,.bar} (note
     *       this will not look in '.' if it's not in the PATH)
     */

#ifdef VMS
    if (dosearch) {
	int hasdir, idx = 0, deftypes = 1;
	bool seen_dot = 1;

	hasdir = (strpbrk(scriptname,":[</") != Nullch) ;
	/* The first time through, just add SEARCH_EXTS to whatever we
	 * already have, so we can check for default file types. */
	while (deftypes ||
	       (!hasdir && my_trnlnm("DCL$PATH",tokenbuf,idx++)) )
	{
	    if (deftypes) {
		deftypes = 0;
		*tokenbuf = '\0';
	    }
	    if ((strlen(tokenbuf) + strlen(scriptname)
		 + MAX_EXT_LEN) >= sizeof tokenbuf)
		continue;	/* don't search dir with too-long name */
	    strcat(tokenbuf, scriptname);
#else  /* !VMS */

#ifdef DOSISH
    if (strEQ(scriptname, "-"))
 	dosearch = 0;
    if (dosearch) {		/* Look in '.' first. */
	char *cur = scriptname;
#ifdef SEARCH_EXTS
	if ((curext = strrchr(scriptname,'.')))	/* possible current ext */
	    while (ext[i])
		if (strEQ(ext[i++],curext)) {
		    extidx = -1;		/* already has an ext */
		    break;
		}
	do {
#endif
	    DEBUG_p(PerlIO_printf(Perl_debug_log,
				  "Looking for %s\n",cur));
	    if (Stat(cur,&statbuf) >= 0) {
		dosearch = 0;
		scriptname = cur;
#ifdef SEARCH_EXTS
		break;
#endif
	    }
#ifdef SEARCH_EXTS
	    if (cur == scriptname) {
		len = strlen(scriptname);
		if (len+MAX_EXT_LEN+1 >= sizeof(tokenbuf))
		    break;
		cur = strcpy(tokenbuf, scriptname);
	    }
	} while (extidx >= 0 && ext[extidx]	/* try an extension? */
		 && strcpy(tokenbuf+len, ext[extidx++]));
#endif
    }
#endif

    if (dosearch && !strchr(scriptname, '/')
#ifdef DOSISH
		 && !strchr(scriptname, '\\')
#endif
		 && (s = getenv("PATH"))) {
	bool seen_dot = 0;
	
	bufend = s + strlen(s);
	while (s < bufend) {
#if defined(atarist) || defined(DOSISH)
	    for (len = 0; *s
#  ifdef atarist
		    && *s != ','
#  endif
		    && *s != ';'; len++, s++) {
		if (len < sizeof tokenbuf)
		    tokenbuf[len] = *s;
	    }
	    if (len < sizeof tokenbuf)
		tokenbuf[len] = '\0';
#else  /* ! (atarist || DOSISH) */
	    s = delimcpy(tokenbuf, tokenbuf + sizeof tokenbuf, s, bufend,
			':',
			&len);
#endif /* ! (atarist || DOSISH) */
	    if (s < bufend)
		s++;
	    if (len + 1 + strlen(scriptname) + MAX_EXT_LEN >= sizeof tokenbuf)
		continue;	/* don't search dir with too-long name */
	    if (len
#if defined(atarist) || defined(DOSISH)
		&& tokenbuf[len - 1] != '/'
		&& tokenbuf[len - 1] != '\\'
#endif
	       )
		tokenbuf[len++] = '/';
	    if (len == 2 && tokenbuf[0] == '.')
		seen_dot = 1;
	    (void)strcpy(tokenbuf + len, scriptname);
#endif  /* !VMS */

#ifdef SEARCH_EXTS
	    len = strlen(tokenbuf);
	    if (extidx > 0)	/* reset after previous loop */
		extidx = 0;
	    do {
#endif
	    	DEBUG_p(PerlIO_printf(Perl_debug_log, "Looking for %s\n",tokenbuf));
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
		&& cando(S_IRUSR,TRUE,&statbuf)
#ifndef DOSISH
		&& cando(S_IXUSR,TRUE,&statbuf)
#endif
		)
	    {
		xfound = tokenbuf;              /* bingo! */
		break;
	    }
	    if (!xfailed)
		xfailed = savepv(tokenbuf);
	}
#ifndef DOSISH
	if (!xfound && !seen_dot && !xfailed && (Stat(scriptname,&statbuf) < 0))
#endif
	    seen_dot = 1;			/* Disable message. */
	if (!xfound)
	    croak("Can't %s %s%s%s",
		  (xfailed ? "execute" : "find"),
		  (xfailed ? xfailed : scriptname),
		  (xfailed ? "" : " on PATH"),
		  (xfailed || seen_dot) ? "" : ", '.' not in PATH");
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
	rsfp = PerlIO_fdopen(fdscript,"r");
#if defined(HAS_FCNTL) && defined(F_SETFD)
	if (rsfp)
	    fcntl(PerlIO_fileno(rsfp),F_SETFD,1);  /* ensure close-on-exec */
#endif
    }
    else if (preprocess) {
	char *cpp_cfg = CPPSTDIN;
	SV *cpp = NEWSV(0,0);
	SV *cmd = NEWSV(0,0);

	if (strEQ(cpp_cfg, "cppstdin"))
	    sv_catpvf(cpp, "%s/", BIN_EXP);
	sv_catpv(cpp, cpp_cfg);

	sv_catpv(sv,"-I");
	sv_catpv(sv,PRIVLIB_EXP);

#ifdef MSDOS
	sv_setpvf(cmd, "\
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
 %s | %_ -C %_ %s",
	  (doextract ? "-e \"1,/^#/d\n\"" : ""),
#else
	sv_setpvf(cmd, "\
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
 %s | %_ -C %_ %s",
#ifdef LOC_SED
	  LOC_SED,
#else
	  "sed",
#endif
	  (doextract ? "-e '1,/^#/d\n'" : ""),
#endif
	  scriptname, cpp, sv, CPPMINUS);
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
	rsfp = my_popen(SvPVX(cmd), "r");
	SvREFCNT_dec(cmd);
	SvREFCNT_dec(cpp);
    }
    else if (!*scriptname) {
	forbid_setid("program input from stdin");
	rsfp = PerlIO_stdin();
    }
    else {
	rsfp = PerlIO_open(scriptname,"r");
#if defined(HAS_FCNTL) && defined(F_SETFD)
	if (rsfp)
	    fcntl(PerlIO_fileno(rsfp),F_SETFD,1);  /* ensure close-on-exec */
#endif
    }
    if (e_tmpname) {
	e_fp = rsfp;
    }
    if (!rsfp) {
#ifdef DOSUID
#ifndef IAMSUID		/* in case script is not readable before setuid */
	if (euid && Stat(SvPVX(GvSV(curcop->cop_filegv)),&statbuf) >= 0 &&
	  statbuf.st_mode & (S_ISUID|S_ISGID)) {
	    /* try again */
	    execv(form("%s/sperl%s", BIN_EXP, patchlevel), origargv);
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
    char *s, *s2;

    if (Fstat(PerlIO_fileno(rsfp),&statbuf) < 0)	/* normal stat is insecure */
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
		(void)PerlIO_close(rsfp);
		if (rsfp = my_popen("/bin/mail root","w")) {	/* heh, heh */
		    PerlIO_printf(rsfp,
"User %ld tried to run dev %ld ino %ld in place of dev %ld ino %ld!\n\
(Filename of set-id script was %s, uid %ld gid %ld.)\n\nSincerely,\nperl\n",
			(long)uid,(long)tmpstatbuf.st_dev, (long)tmpstatbuf.st_ino,
			(long)statbuf.st_dev, (long)statbuf.st_ino,
			SvPVX(GvSV(curcop->cop_filegv)),
			(long)statbuf.st_uid, (long)statbuf.st_gid);
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
	if (sv_gets(linestr, rsfp, 0) == Nullch ||
	  strnNE(SvPV(linestr,na),"#!",2) )	/* required even on Sys V */
	    croak("No #! line");
	s = SvPV(linestr,na)+2;
	if (*s == ' ') s++;
	while (!isSPACE(*s)) s++;
	for (s2 = s;  (s2 > SvPV(linestr,na)+2 &&
		       (isDIGIT(s2[-1]) || strchr("._-", s2[-1])));  s2--) ;
	if (strnNE(s2-4,"perl",4) && strnNE(s-9,"perl",4))  /* sanity check */
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
	    (void)PerlIO_close(rsfp);
#ifndef IAMSUID
	    /* try again */
	    execv(form("%s/sperl%s", BIN_EXP, patchlevel), origargv);
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
    PerlIO_rewind(rsfp);
    lseek(PerlIO_fileno(rsfp),(Off_t)0,0);  /* just in case rewind didn't */
    for (which = 1; origargv[which] && origargv[which] != scriptname; which++) ;
    if (!origargv[which])
	croak("Permission denied");
    origargv[which] = savepv(form("/dev/fd/%d/%s",
				  PerlIO_fileno(rsfp), origargv[which]));
#if defined(HAS_FCNTL) && defined(F_SETFD)
    fcntl(PerlIO_fileno(rsfp),F_SETFD,0);	/* ensure no close-on-exec */
#endif
    execv(form("%s/perl%s", BIN_EXP, patchlevel), origargv);	/* try again */
    croak("Can't do setuid\n");
#endif /* IAMSUID */
#else /* !DOSUID */
    if (euid != uid || egid != gid) {	/* (suidperl doesn't exist, in fact) */
#ifndef SETUID_SCRIPTS_ARE_SECURE_NOW
	Fstat(PerlIO_fileno(rsfp),&statbuf);	/* may be either wrapped or real suid */
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
    register char *s, *s2;

    /* skip forward in input to the real script? */

    forbid_setid("-x");
    while (doextract) {
	if ((s = sv_gets(linestr, rsfp, 0)) == Nullch)
	    croak("No Perl script found in input\n");
	if (*s == '#' && s[1] == '!' && (s = instr(s,"perl"))) {
	    PerlIO_ungetc(rsfp, '\n');		/* to keep line count right */
	    doextract = FALSE;
	    while (*s && !(isSPACE (*s) || *s == '#')) s++;
	    s2 = s;
	    while (*s == ' ' || *s == '\t') s++;
	    if (*s++ == '-') {
		while (isDIGIT(s2[-1]) || strchr("-._", s2[-1])) s2--;
		if (strnEQ(s2-4,"perl",4))
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
forbid_setid(s)
char *s;
{
    if (euid != uid)
        croak("No %s allowed while running setuid", s);
    if (egid != gid)
        croak("No %s allowed while running setgid", s);
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
    curstack = newAV();
    mainstack = curstack;		/* remember in case we switch stacks */
    AvREAL_off(curstack);		/* not a real array */
    av_extend(curstack,127);

    stack_base = AvARRAY(curstack);
    stack_sp = stack_base;
    stack_max = stack_base + 127;

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

    /*
     * The following stacks almost certainly should be per-interpreter,
     * but for now they're not.  XXX
     */

    if (markstack) {
	markstack_ptr = markstack;
    } else {
	New(54,markstack,64,I32);
	markstack_ptr = markstack;
	markstack_max = markstack + 64;
    }

    if (scopestack) {
	scopestack_ix = 0;
    } else {
	New(54,scopestack,32,I32);
	scopestack_ix = 0;
	scopestack_max = 32;
    }

    if (savestack) {
	savestack_ix = 0;
    } else {
	New(54,savestack,128,ANY);
	savestack_ix = 0;
	savestack_max = 128;
    }

    if (retstack) {
	retstack_ix = 0;
    } else {
	New(54,retstack,16,OP*);
	retstack_ix = 0;
	retstack_max = 16;
    }
}

static void
nuke_stacks()
{
    Safefree(cxstack);
    Safefree(tmps_stack);
    DEBUG( {
	Safefree(debname);
	Safefree(debdelim);
    } )
}

static PerlIO *tmpfp;  /* moved outside init_lexer() because of UNICOS bug */

static void
init_lexer()
{
    tmpfp = rsfp;
    rsfp = Nullfp;
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
    IoIFP(GvIOp(stdingv)) = PerlIO_stdin();
    tmpgv = gv_fetchpv("stdin",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(GvIOp(stdingv));

    tmpgv = gv_fetchpv("STDOUT",TRUE, SVt_PVIO);
    GvMULTI_on(tmpgv);
    IoOFP(GvIOp(tmpgv)) = IoIFP(GvIOp(tmpgv)) = PerlIO_stdout();
    setdefout(tmpgv);
    tmpgv = gv_fetchpv("stdout",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(GvIOp(defoutgv));

    othergv = gv_fetchpv("STDERR",TRUE, SVt_PVIO);
    GvMULTI_on(othergv);
    IoOFP(GvIOp(othergv)) = IoIFP(GvIOp(othergv)) = PerlIO_stderr();
    tmpgv = gv_fetchpv("stderr",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(GvIOp(othergv));

    statname = NEWSV(66,0);		/* last filename we did stat on */

    if (!osname)
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

    TAINT;
    if (tmpgv = gv_fetchpv("0",TRUE, SVt_PV)) {
	sv_setpv(GvSV(tmpgv),origfilename);
	magicname("0", "0", 1);
    }
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
	hv_magic(hv, envgv, 'E');
#ifndef VMS  /* VMS doesn't have environ array */
	/* Note that if the supplied env parameter is actually a copy
	   of the global environ then it may now point to free'd memory
	   if the environment has been modified since. To avoid this
	   problem we treat env==NULL as meaning 'use the default'
	*/
	if (!env)
	    env = environ;
	if (env != environ)
	    environ[0] = Nullch;
	for (; *env; env++) {
	    if (!(s = strchr(*env,'=')))
		continue;
	    *s++ = '\0';
#ifdef WIN32
	    (void)strupr(*env);
#endif
	    sv = newSVpv(s--,0);
	    (void)hv_store(hv, *env, s - *env, sv, 0);
	    *s = '=';
#if defined(__BORLANDC__) && defined(USE_WIN32_RTL_ENV)
	    /* Sins of the RTL. See note in my_setenv(). */
	    (void)putenv(savepv(*env));
#endif
	}
#endif
#ifdef DYNAMIC_ENV_FETCH
	HvNAME(hv) = savepv(ENV_HV_NAME);
#endif
    }
    TAINT_NOT;
    if (tmpgv = gv_fetchpv("$",TRUE, SVt_PV))
	sv_setiv(GvSV(tmpgv), (IV)getpid());
}

static void
init_perllib()
{
    char *s;
    if (!tainting) {
#ifndef VMS
	s = getenv("PERL5LIB");
	if (s)
	    incpush(s, TRUE);
	else
	    incpush(getenv("PERLLIB"), FALSE);
#else /* VMS */
	/* Treat PERL5?LIB as a possible search list logical name -- the
	 * "natural" VMS idiom for a Unix path string.  We allow each
	 * element to be a set of |-separated directories for compatibility.
	 */
	char buf[256];
	int idx = 0;
	if (my_trnlnm("PERL5LIB",buf,0))
	    do { incpush(buf,TRUE); } while (my_trnlnm("PERL5LIB",buf,++idx));
	else
	    while (my_trnlnm("PERLLIB",buf,idx++)) incpush(buf,FALSE);
#endif /* VMS */
    }

/* Use the ~-expanded versions of APPLLIB (undocumented),
    ARCHLIB PRIVLIB SITEARCH SITELIB and OLDARCHLIB
*/
#ifdef APPLLIB_EXP
    incpush(APPLLIB_EXP, FALSE);
#endif

#ifdef ARCHLIB_EXP
    incpush(ARCHLIB_EXP, FALSE);
#endif
#ifndef PRIVLIB_EXP
#define PRIVLIB_EXP "/usr/local/lib/perl5:/usr/local/lib/perl"
#endif
    incpush(PRIVLIB_EXP, FALSE);

#ifdef SITEARCH_EXP
    incpush(SITEARCH_EXP, FALSE);
#endif
#ifdef SITELIB_EXP
    incpush(SITELIB_EXP, FALSE);
#endif
#ifdef OLDARCHLIB_EXP  /* 5.00[01] compatibility */
    incpush(OLDARCHLIB_EXP, FALSE);
#endif
    
    if (!tainting)
	incpush(".", FALSE);
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
#ifndef PERLLIB_MANGLE
#  define PERLLIB_MANGLE(s,n) (s)
#endif 

static void
incpush(p, addsubdirs)
char *p;
int addsubdirs;
{
    SV *subdir = Nullsv;
    static char *archpat_auto;

    if (!p)
	return;

    if (addsubdirs) {
	subdir = newSV(0);
	if (!archpat_auto) {
	    STRLEN len = (sizeof(ARCHNAME) + strlen(patchlevel)
			  + sizeof("//auto"));
	    New(55, archpat_auto, len, char);
	    sprintf(archpat_auto, "/%s/%s/auto", ARCHNAME, patchlevel);
#ifdef VMS
	for (len = sizeof(ARCHNAME) + 2;
	     archpat_auto[len] != '\0' && archpat_auto[len] != '/'; len++)
		if (archpat_auto[len] == '.') archpat_auto[len] = '_';
#endif
	}
    }

    /* Break at all separators */
    while (p && *p) {
	SV *libdir = newSV(0);
	char *s;

	/* skip any consecutive separators */
	while ( *p == PERLLIB_SEP ) {
	    /* Uncomment the next line for PATH semantics */
	    /* av_push(GvAVn(incgv), newSVpv(".", 1)); */
	    p++;
	}

	if ( (s = strchr(p, PERLLIB_SEP)) != Nullch ) {
	    sv_setpvn(libdir, PERLLIB_MANGLE(p, (STRLEN)(s - p)),
		      (STRLEN)(s - p));
	    p = s + 1;
	}
	else {
	    sv_setpv(libdir, PERLLIB_MANGLE(p, 0));
	    p = Nullch;	/* break out */
	}

	/*
	 * BEFORE pushing libdir onto @INC we may first push version- and
	 * archname-specific sub-directories.
	 */
	if (addsubdirs) {
	    struct stat tmpstatbuf;
#ifdef VMS
	    char *unix;
	    STRLEN len;

	    if ((unix = tounixspec_ts(SvPV(libdir,na),Nullch)) != Nullch) {
		len = strlen(unix);
		while (unix[len-1] == '/') len--;  /* Cosmetic */
		sv_usepvn(libdir,unix,len);
	    }
	    else
		PerlIO_printf(PerlIO_stderr(),
		              "Failed to unixify @INC element \"%s\"\n",
			      SvPV(libdir,na));
#endif
	    /* .../archname/version if -d .../archname/version/auto */
	    sv_setsv(subdir, libdir);
	    sv_catpv(subdir, archpat_auto);
	    if (Stat(SvPVX(subdir), &tmpstatbuf) >= 0 &&
		  S_ISDIR(tmpstatbuf.st_mode))
		av_push(GvAVn(incgv),
			newSVpv(SvPVX(subdir), SvCUR(subdir) - sizeof "auto"));

	    /* .../archname if -d .../archname/auto */
	    sv_insert(subdir, SvCUR(libdir) + sizeof(ARCHNAME),
		      strlen(patchlevel) + 1, "", 0);
	    if (Stat(SvPVX(subdir), &tmpstatbuf) >= 0 &&
		  S_ISDIR(tmpstatbuf.st_mode))
		av_push(GvAVn(incgv),
			newSVpv(SvPVX(subdir), SvCUR(subdir) - sizeof "auto"));
	}

	/* finally push this lib directory on the end of @INC */
	av_push(GvAVn(incgv), libdir);
    }

    SvREFCNT_dec(subdir);
}

void
call_list(oldscope, list)
I32 oldscope;
AV* list;
{
    line_t oldline = curcop->cop_line;
    STRLEN len;
    dJMPENV;
    int ret;

    while (AvFILL(list) >= 0) {
	CV *cv = (CV*)av_shift(list);

	SAVEFREESV(cv);

	JMPENV_PUSH(ret);
	switch (ret) {
	case 0: {
		SV* atsv = GvSV(errgv);
		PUSHMARK(stack_sp);
		perl_call_sv((SV*)cv, G_EVAL|G_DISCARD);
		(void)SvPV(atsv, len);
		if (len) {
		    JMPENV_POP;
		    curcop = &compiling;
		    curcop->cop_line = oldline;
		    if (list == beginav)
			sv_catpv(atsv, "BEGIN failed--compilation aborted");
		    else
			sv_catpv(atsv, "END failed--cleanup aborted");
		    while (scopestack_ix > oldscope)
			LEAVE;
		    croak("%s", SvPVX(atsv));
		}
	    }
	    break;
	case 1:
	    STATUS_ALL_FAILURE;
	    /* FALL THROUGH */
	case 2:
	    /* my_exit() was called */
	    while (scopestack_ix > oldscope)
		LEAVE;
	    FREETMPS;
	    curstash = defstash;
	    if (endav)
		call_list(oldscope, endav);
	    JMPENV_POP;
	    curcop = &compiling;
	    curcop->cop_line = oldline;
	    if (statusvalue) {
		if (list == beginav)
		    croak("BEGIN failed--compilation aborted");
		else
		    croak("END failed--cleanup aborted");
	    }
	    my_exit_jump();
	    /* NOTREACHED */
	case 3:
	    if (!restartop) {
		PerlIO_printf(PerlIO_stderr(), "panic: restartop\n");
		FREETMPS;
		break;
	    }
	    JMPENV_POP;
	    curcop = &compiling;
	    curcop->cop_line = oldline;
	    JMPENV_JUMP(3);
	}
	JMPENV_POP;
    }
}

void
my_exit(status)
U32 status;
{
    switch (status) {
    case 0:
	STATUS_ALL_SUCCESS;
	break;
    case 1:
	STATUS_ALL_FAILURE;
	break;
    default:
	STATUS_NATIVE_SET(status);
	break;
    }
    my_exit_jump();
}

void
my_failure_exit()
{
#ifdef VMS
    if (vaxc$errno & 1) {
	if (STATUS_NATIVE & 1)		/* fortuitiously includes "-1" */
	    STATUS_NATIVE_SET(44);
    }
    else {
	if (!vaxc$errno && errno)	/* unlikely */
	    STATUS_NATIVE_SET(44);
	else
	    STATUS_NATIVE_SET(vaxc$errno);
    }
#else
    if (errno & 255)
	STATUS_POSIX_SET(errno);
    else if (STATUS_POSIX == 0)
	STATUS_POSIX_SET(255);
#endif
    my_exit_jump();
}

static void
my_exit_jump()
{
    register CONTEXT *cx;
    I32 gimme;
    SV **newsp;

    if (e_tmpname) {
	if (e_fp) {
	    PerlIO_close(e_fp);
	    e_fp = Nullfp;
	}
	(void)UNLINK(e_tmpname);
	Safefree(e_tmpname);
	e_tmpname = Nullch;
    }

    if (cxstack_ix >= 0) {
	if (cxstack_ix > 0)
	    dounwind(0);
	POPBLOCK(cx,curpm);
	LEAVE;
    }

    JMPENV_JUMP(2);
}
