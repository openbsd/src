/*    perl.c
 *
 *    Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999,
 *    2000, 2001, 2002, 2003, 2004, by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "A ship then new they built for him/of mithril and of elven glass" --Bilbo
 */

/* This file contains the top-level functions that are used to create, use
 * and destroy a perl interpreter, plus the functions used by XS code to
 * call back into perl. Note that it does not contain the actual main()
 * function of the interpreter; that can be found in perlmain.c
 */

/* PSz 12 Nov 03
 * 
 * Be proud that perl(1) may proclaim:
 *   Setuid Perl scripts are safer than C programs ...
 * Do not abandon (deprecate) suidperl. Do not advocate C wrappers.
 * 
 * The flow was: perl starts, notices script is suid, execs suidperl with same
 * arguments; suidperl opens script, checks many things, sets itself with
 * right UID, execs perl with similar arguments but with script pre-opened on
 * /dev/fd/xxx; perl checks script is as should be and does work. This was
 * insecure: see perlsec(1) for many problems with this approach.
 * 
 * The "correct" flow should be: perl starts, opens script and notices it is
 * suid, checks many things, execs suidperl with similar arguments but with
 * script on /dev/fd/xxx; suidperl checks script and /dev/fd/xxx object are
 * same, checks arguments match #! line, sets itself with right UID, execs
 * perl with same arguments; perl checks many things and does work.
 * 
 * (Opening the script in perl instead of suidperl, we "lose" scripts that
 * are readable to the target UID but not to the invoker. Where did
 * unreadable scripts work anyway?)
 * 
 * For now, suidperl and perl are pretty much the same large and cumbersome
 * program, so suidperl can check its argument list (see comments elsewhere).
 * 
 * References:
 * Original bug report:
 *   http://bugs.perl.org/index.html?req=bug_id&bug_id=20010322.218
 *   http://rt.perl.org/rt2/Ticket/Display.html?id=6511
 * Comments and discussion with Debian:
 *   http://bugs.debian.org/203426
 *   http://bugs.debian.org/220486
 * Debian Security Advisory DSA 431-1 (does not fully fix problem):
 *   http://www.debian.org/security/2004/dsa-431
 * CVE candidate:
 *   http://cve.mitre.org/cgi-bin/cvename.cgi?name=CAN-2003-0618
 * Previous versions of this patch sent to perl5-porters:
 *   http://www.mail-archive.com/perl5-porters@perl.org/msg71953.html
 *   http://www.mail-archive.com/perl5-porters@perl.org/msg75245.html
 *   http://www.mail-archive.com/perl5-porters@perl.org/msg75563.html
 *   http://www.mail-archive.com/perl5-porters@perl.org/msg75635.html
 * 
Paul Szabo - psz@maths.usyd.edu.au  http://www.maths.usyd.edu.au:8000/u/psz/
School of Mathematics and Statistics  University of Sydney   2006  Australia
 * 
 */
/* PSz 13 Nov 03
 * Use truthful, neat, specific error messages.
 * Cannot always hide the truth; security must not depend on doing so.
 */

/* PSz 18 Feb 04
 * Use global(?), thread-local fdscript for easier checks.
 * (I do not understand how we could possibly get a thread race:
 * do not all threads go through the same initialization? Or in
 * fact, are not threads started only after we get the script and
 * so know what to do? Oh well, make things super-safe...)
 */

#include "EXTERN.h"
#define PERL_IN_PERL_C
#include "perl.h"
#include "patchlevel.h"			/* for local_patches */

#ifdef NETWARE
#include "nwutil.h"	
char *nw_get_sitelib(const char *pl);
#endif

/* XXX If this causes problems, set i_unistd=undef in the hint file.  */
#ifdef I_UNISTD
#include <unistd.h>
#endif

#ifdef __BEOS__
#  define HZ 1000000
#endif

#ifndef HZ
#  ifdef CLK_TCK
#    define HZ CLK_TCK
#  else
#    define HZ 60
#  endif
#endif

#if !defined(STANDARD_C) && !defined(HAS_GETENV_PROTOTYPE) && !defined(PERL_MICRO)
char *getenv (char *); /* Usually in <stdlib.h> */
#endif

static I32 read_e_script(pTHX_ int idx, SV *buf_sv, int maxlen);

#ifdef IAMSUID
#ifndef DOSUID
#define DOSUID
#endif
#endif /* IAMSUID */

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
#ifdef DOSUID
#undef DOSUID
#endif
#endif

#if defined(USE_5005THREADS)
#  define INIT_TLS_AND_INTERP \
    STMT_START {				\
	if (!PL_curinterp) {			\
	    PERL_SET_INTERP(my_perl);		\
	    INIT_THREADS;			\
	    ALLOC_THREAD_KEY;			\
	}					\
    } STMT_END
#else
#  if defined(USE_ITHREADS)
#  define INIT_TLS_AND_INTERP \
    STMT_START {				\
	if (!PL_curinterp) {			\
	    PERL_SET_INTERP(my_perl);		\
	    INIT_THREADS;			\
	    ALLOC_THREAD_KEY;			\
	    PERL_SET_THX(my_perl);		\
	    OP_REFCNT_INIT;			\
	    MUTEX_INIT(&PL_dollarzero_mutex);	\
	}					\
	else {					\
	    PERL_SET_THX(my_perl);		\
	}					\
    } STMT_END
#  else
#  define INIT_TLS_AND_INTERP \
    STMT_START {				\
	if (!PL_curinterp) {			\
	    PERL_SET_INTERP(my_perl);		\
	}					\
	PERL_SET_THX(my_perl);			\
    } STMT_END
#  endif
#endif

#ifdef PERL_IMPLICIT_SYS
PerlInterpreter *
perl_alloc_using(struct IPerlMem* ipM, struct IPerlMem* ipMS,
		 struct IPerlMem* ipMP, struct IPerlEnv* ipE,
		 struct IPerlStdIO* ipStd, struct IPerlLIO* ipLIO,
		 struct IPerlDir* ipD, struct IPerlSock* ipS,
		 struct IPerlProc* ipP)
{
    PerlInterpreter *my_perl;
    /* New() needs interpreter, so call malloc() instead */
    my_perl = (PerlInterpreter*)(*ipM->pMalloc)(ipM, sizeof(PerlInterpreter));
    INIT_TLS_AND_INTERP;
    Zero(my_perl, 1, PerlInterpreter);
    PL_Mem = ipM;
    PL_MemShared = ipMS;
    PL_MemParse = ipMP;
    PL_Env = ipE;
    PL_StdIO = ipStd;
    PL_LIO = ipLIO;
    PL_Dir = ipD;
    PL_Sock = ipS;
    PL_Proc = ipP;

    return my_perl;
}
#else

/*
=head1 Embedding Functions

=for apidoc perl_alloc

Allocates a new Perl interpreter.  See L<perlembed>.

=cut
*/

PerlInterpreter *
perl_alloc(void)
{
    PerlInterpreter *my_perl;
#ifdef USE_5005THREADS
    dTHX;
#endif

    /* New() needs interpreter, so call malloc() instead */
    my_perl = (PerlInterpreter*)PerlMem_malloc(sizeof(PerlInterpreter));

    INIT_TLS_AND_INTERP;
    return ZeroD(my_perl, 1, PerlInterpreter);
}
#endif /* PERL_IMPLICIT_SYS */

/*
=for apidoc perl_construct

Initializes a new Perl interpreter.  See L<perlembed>.

=cut
*/

void
perl_construct(pTHXx)
{
#ifdef USE_5005THREADS
#ifndef FAKE_THREADS
    struct perl_thread *thr = NULL;
#endif /* FAKE_THREADS */
#endif /* USE_5005THREADS */

#ifdef MULTIPLICITY
    init_interp();
    PL_perl_destruct_level = 1;
#else
   if (PL_perl_destruct_level > 0)
       init_interp();
#endif
   /* Init the real globals (and main thread)? */
    if (!PL_linestr) {
#ifdef USE_5005THREADS
	MUTEX_INIT(&PL_sv_mutex);
	/*
	 * Safe to use basic SV functions from now on (though
	 * not things like mortals or tainting yet).
	 */
	MUTEX_INIT(&PL_eval_mutex);
	COND_INIT(&PL_eval_cond);
	MUTEX_INIT(&PL_threads_mutex);
	COND_INIT(&PL_nthreads_cond);
#  ifdef EMULATE_ATOMIC_REFCOUNTS
	MUTEX_INIT(&PL_svref_mutex);
#  endif /* EMULATE_ATOMIC_REFCOUNTS */
	
	MUTEX_INIT(&PL_cred_mutex);
	MUTEX_INIT(&PL_sv_lock_mutex);
	MUTEX_INIT(&PL_fdpid_mutex);

	thr = init_main_thread();
#endif /* USE_5005THREADS */

#ifdef PERL_FLEXIBLE_EXCEPTIONS
	PL_protect = MEMBER_TO_FPTR(Perl_default_protect); /* for exceptions */
#endif

	PL_curcop = &PL_compiling;	/* needed by ckWARN, right away */

	PL_linestr = NEWSV(65,79);
	sv_upgrade(PL_linestr,SVt_PVIV);

	if (!SvREADONLY(&PL_sv_undef)) {
	    /* set read-only and try to insure than we wont see REFCNT==0
	       very often */

	    SvREADONLY_on(&PL_sv_undef);
	    SvREFCNT(&PL_sv_undef) = (~(U32)0)/2;

	    sv_setpv(&PL_sv_no,PL_No);
	    /* value lookup in void context - happens to have the side effect
	       of caching the numeric forms.  */
	    SvIV(&PL_sv_no);
	    SvNV(&PL_sv_no);
	    SvREADONLY_on(&PL_sv_no);
	    SvREFCNT(&PL_sv_no) = (~(U32)0)/2;

	    sv_setpv(&PL_sv_yes,PL_Yes);
	    SvIV(&PL_sv_yes);
	    SvNV(&PL_sv_yes);
	    SvREADONLY_on(&PL_sv_yes);
	    SvREFCNT(&PL_sv_yes) = (~(U32)0)/2;

	    SvREADONLY_on(&PL_sv_placeholder);
	    SvREFCNT(&PL_sv_placeholder) = (~(U32)0)/2;
	}

	PL_sighandlerp = Perl_sighandler;
	PL_pidstatus = newHV();
    }

    PL_rs = newSVpvn("\n", 1);

    init_stacks();

    init_ids();
    PL_lex_state = LEX_NOTPARSING;

    JMPENV_BOOTSTRAP;
    STATUS_ALL_SUCCESS;

    init_i18nl10n(1);
    SET_NUMERIC_STANDARD();

    {
	U8 *s;
	PL_patchlevel = NEWSV(0,4);
	(void)SvUPGRADE(PL_patchlevel, SVt_PVNV);
	if (PERL_REVISION > 127 || PERL_VERSION > 127 || PERL_SUBVERSION > 127)
	    SvGROW(PL_patchlevel, UTF8_MAXLEN*3+1);
	s = (U8*)SvPVX(PL_patchlevel);
	/* Build version strings using "native" characters */
	s = uvchr_to_utf8(s, (UV)PERL_REVISION);
	s = uvchr_to_utf8(s, (UV)PERL_VERSION);
	s = uvchr_to_utf8(s, (UV)PERL_SUBVERSION);
	*s = '\0';
	SvCUR_set(PL_patchlevel, s - (U8*)SvPVX(PL_patchlevel));
	SvPOK_on(PL_patchlevel);
	SvNVX(PL_patchlevel) = (NV)PERL_REVISION +
			      ((NV)PERL_VERSION / (NV)1000) +
			      ((NV)PERL_SUBVERSION / (NV)1000000);
	SvNOK_on(PL_patchlevel);	/* dual valued */
	SvUTF8_on(PL_patchlevel);
	SvREADONLY_on(PL_patchlevel);
    }

#if defined(LOCAL_PATCH_COUNT)
    PL_localpatches = local_patches;	/* For possible -v */
#endif

#ifdef HAVE_INTERP_INTERN
    sys_intern_init();
#endif

    PerlIO_init(aTHX);			/* Hook to IO system */

    PL_fdpid = newAV();			/* for remembering popen pids by fd */
    PL_modglobal = newHV();		/* pointers to per-interpreter module globals */
    PL_errors = newSVpvn("",0);
    sv_setpvn(PERL_DEBUG_PAD(0), "", 0);	/* For regex debugging. */
    sv_setpvn(PERL_DEBUG_PAD(1), "", 0);	/* ext/re needs these */
    sv_setpvn(PERL_DEBUG_PAD(2), "", 0);	/* even without DEBUGGING. */
#ifdef USE_ITHREADS
    PL_regex_padav = newAV();
    av_push(PL_regex_padav,(SV*)newAV());    /* First entry is an array of empty elements */
    PL_regex_pad = AvARRAY(PL_regex_padav);
#endif
#ifdef USE_REENTRANT_API
    Perl_reentrant_init(aTHX);
#endif

    /* Note that strtab is a rather special HV.  Assumptions are made
       about not iterating on it, and not adding tie magic to it.
       It is properly deallocated in perl_destruct() */
    PL_strtab = newHV();

#ifdef USE_5005THREADS
    MUTEX_INIT(&PL_strtab_mutex);
#endif
    HvSHAREKEYS_off(PL_strtab);			/* mandatory */
    hv_ksplit(PL_strtab, 512);

#if defined(__DYNAMIC__) && (defined(NeXT) || defined(__NeXT__))
    _dyld_lookup_and_bind
	("__environ", (unsigned long *) &environ_pointer, NULL);
#endif /* environ */

#ifndef PERL_MICRO
#   ifdef  USE_ENVIRON_ARRAY
    PL_origenviron = environ;
#   endif
#endif

    /* Use sysconf(_SC_CLK_TCK) if available, if not
     * available or if the sysconf() fails, use the HZ. */
#if defined(HAS_SYSCONF) && defined(_SC_CLK_TCK)
    PL_clocktick = sysconf(_SC_CLK_TCK);
    if (PL_clocktick <= 0)
#endif
	 PL_clocktick = HZ;

    PL_stashcache = newHV();

    ENTER;
}

/*
=for apidoc nothreadhook

Stub that provides thread hook for perl_destruct when there are
no threads.

=cut
*/

int
Perl_nothreadhook(pTHX)
{
    return 0;
}

/*
=for apidoc perl_destruct

Shuts down a Perl interpreter.  See L<perlembed>.

=cut
*/

int
perl_destruct(pTHXx)
{
    volatile int destruct_level;  /* 0=none, 1=full, 2=full with checks */
    HV *hv;
#ifdef USE_5005THREADS
    Thread t;
    dTHX;
#endif /* USE_5005THREADS */

    /* wait for all pseudo-forked children to finish */
    PERL_WAIT_FOR_CHILDREN;

#ifdef USE_5005THREADS
#ifndef FAKE_THREADS
    /* Pass 1 on any remaining threads: detach joinables, join zombies */
  retry_cleanup:
    MUTEX_LOCK(&PL_threads_mutex);
    DEBUG_S(PerlIO_printf(Perl_debug_log,
			  "perl_destruct: waiting for %d threads...\n",
			  PL_nthreads - 1));
    for (t = thr->next; t != thr; t = t->next) {
	MUTEX_LOCK(&t->mutex);
	switch (ThrSTATE(t)) {
	    AV *av;
	case THRf_ZOMBIE:
	    DEBUG_S(PerlIO_printf(Perl_debug_log,
				  "perl_destruct: joining zombie %p\n", t));
	    ThrSETSTATE(t, THRf_DEAD);
	    MUTEX_UNLOCK(&t->mutex);
	    PL_nthreads--;
	    /*
	     * The SvREFCNT_dec below may take a long time (e.g. av
	     * may contain an object scalar whose destructor gets
	     * called) so we have to unlock threads_mutex and start
	     * all over again.
	     */
	    MUTEX_UNLOCK(&PL_threads_mutex);
	    JOIN(t, &av);
	    SvREFCNT_dec((SV*)av);
	    DEBUG_S(PerlIO_printf(Perl_debug_log,
				  "perl_destruct: joined zombie %p OK\n", t));
	    goto retry_cleanup;
	case THRf_R_JOINABLE:
	    DEBUG_S(PerlIO_printf(Perl_debug_log,
				  "perl_destruct: detaching thread %p\n", t));
	    ThrSETSTATE(t, THRf_R_DETACHED);
	    /*
	     * We unlock threads_mutex and t->mutex in the opposite order
	     * from which we locked them just so that DETACH won't
	     * deadlock if it panics. It's only a breach of good style
	     * not a bug since they are unlocks not locks.
	     */
	    MUTEX_UNLOCK(&PL_threads_mutex);
	    DETACH(t);
	    MUTEX_UNLOCK(&t->mutex);
	    goto retry_cleanup;
	default:
	    DEBUG_S(PerlIO_printf(Perl_debug_log,
				  "perl_destruct: ignoring %p (state %u)\n",
				  t, ThrSTATE(t)));
	    MUTEX_UNLOCK(&t->mutex);
	    /* fall through and out */
	}
    }
    /* We leave the above "Pass 1" loop with threads_mutex still locked */

    /* Pass 2 on remaining threads: wait for the thread count to drop to one */
    while (PL_nthreads > 1)
    {
	DEBUG_S(PerlIO_printf(Perl_debug_log,
			      "perl_destruct: final wait for %d threads\n",
			      PL_nthreads - 1));
	COND_WAIT(&PL_nthreads_cond, &PL_threads_mutex);
    }
    /* At this point, we're the last thread */
    MUTEX_UNLOCK(&PL_threads_mutex);
    DEBUG_S(PerlIO_printf(Perl_debug_log, "perl_destruct: armageddon has arrived\n"));
    MUTEX_DESTROY(&PL_threads_mutex);
    COND_DESTROY(&PL_nthreads_cond);
    PL_nthreads--;
#endif /* !defined(FAKE_THREADS) */
#endif /* USE_5005THREADS */

    destruct_level = PL_perl_destruct_level;
#ifdef DEBUGGING
    {
	char *s;
	if ((s = PerlEnv_getenv("PERL_DESTRUCT_LEVEL"))) {
	    int i = atoi(s);
	    if (destruct_level < i)
		destruct_level = i;
	}
    }
#endif


    if(PL_exit_flags & PERL_EXIT_DESTRUCT_END) {
        dJMPENV;
        int x = 0;

        JMPENV_PUSH(x);
        if (PL_endav && !PL_minus_c)
            call_list(PL_scopestack_ix, PL_endav);
        JMPENV_POP;
    }
    LEAVE;
    FREETMPS;

    /* Need to flush since END blocks can produce output */
    my_fflush_all();

    if (CALL_FPTR(PL_threadhook)(aTHX)) {
        /* Threads hook has vetoed further cleanup */
        return STATUS_NATIVE_EXPORT;
    }

    /* We must account for everything.  */

    /* Destroy the main CV and syntax tree */
    if (PL_main_root) {
	/* ensure comppad/curpad to refer to main's pad */
	if (CvPADLIST(PL_main_cv)) {
	    PAD_SET_CUR_NOSAVE(CvPADLIST(PL_main_cv), 1);
	}
	op_free(PL_main_root);
	PL_main_root = Nullop;
    }
    PL_curcop = &PL_compiling;
    PL_main_start = Nullop;
    SvREFCNT_dec(PL_main_cv);
    PL_main_cv = Nullcv;
    PL_dirty = TRUE;

    /* Tell PerlIO we are about to tear things apart in case
       we have layers which are using resources that should
       be cleaned up now.
     */

    PerlIO_destruct(aTHX);

    if (PL_sv_objcount) {
	/*
	 * Try to destruct global references.  We do this first so that the
	 * destructors and destructees still exist.  Some sv's might remain.
	 * Non-referenced objects are on their own.
	 */
	sv_clean_objs();
	PL_sv_objcount = 0;
    }

    /* unhook hooks which will soon be, or use, destroyed data */
    SvREFCNT_dec(PL_warnhook);
    PL_warnhook = Nullsv;
    SvREFCNT_dec(PL_diehook);
    PL_diehook = Nullsv;

    /* call exit list functions */
    while (PL_exitlistlen-- > 0)
	PL_exitlist[PL_exitlistlen].fn(aTHX_ PL_exitlist[PL_exitlistlen].ptr);

    Safefree(PL_exitlist);

    PL_exitlist = NULL;
    PL_exitlistlen = 0;

    if (destruct_level == 0){

	DEBUG_P(debprofdump());

#if defined(PERLIO_LAYERS)
	/* No more IO - including error messages ! */
	PerlIO_cleanup(aTHX);
#endif

	/* The exit() function will do everything that needs doing. */
        return STATUS_NATIVE_EXPORT;
    }

    /* jettison our possibly duplicated environment */
    /* if PERL_USE_SAFE_PUTENV is defined environ will not have been copied
     * so we certainly shouldn't free it here
     */
#ifndef PERL_MICRO
#if defined(USE_ENVIRON_ARRAY) && !defined(PERL_USE_SAFE_PUTENV)
    if (environ != PL_origenviron && !PL_use_safe_putenv
#ifdef USE_ITHREADS
	/* only main thread can free environ[0] contents */
	&& PL_curinterp == aTHX
#endif
	)
    {
	I32 i;

	for (i = 0; environ[i]; i++)
	    safesysfree(environ[i]);

	/* Must use safesysfree() when working with environ. */
	safesysfree(environ);		

	environ = PL_origenviron;
    }
#endif
#endif /* !PERL_MICRO */

    /* reset so print() ends up where we expect */
    setdefout(Nullgv);

#ifdef USE_ITHREADS
    /* the syntax tree is shared between clones
     * so op_free(PL_main_root) only ReREFCNT_dec's
     * REGEXPs in the parent interpreter
     * we need to manually ReREFCNT_dec for the clones
     */
    {
        I32 i = AvFILLp(PL_regex_padav) + 1;
        SV **ary = AvARRAY(PL_regex_padav);

        while (i) {
            SV *resv = ary[--i];
            REGEXP *re = INT2PTR(REGEXP *,SvIVX(resv));

            if (SvFLAGS(resv) & SVf_BREAK) {
                /* this is PL_reg_curpm, already freed
                 * flag is set in regexec.c:S_regtry
                 */
                SvFLAGS(resv) &= ~SVf_BREAK;
            }
	    else if(SvREPADTMP(resv)) {
	      SvREPADTMP_off(resv);
	    }
            else {
                ReREFCNT_dec(re);
            }
        }
    }
    SvREFCNT_dec(PL_regex_padav);
    PL_regex_padav = Nullav;
    PL_regex_pad = NULL;
#endif

    SvREFCNT_dec((SV*) PL_stashcache);
    PL_stashcache = NULL;

    /* loosen bonds of global variables */

    if(PL_rsfp) {
	(void)PerlIO_close(PL_rsfp);
	PL_rsfp = Nullfp;
    }

    /* Filters for program text */
    SvREFCNT_dec(PL_rsfp_filters);
    PL_rsfp_filters = Nullav;

    /* switches */
    PL_preprocess   = FALSE;
    PL_minus_n      = FALSE;
    PL_minus_p      = FALSE;
    PL_minus_l      = FALSE;
    PL_minus_a      = FALSE;
    PL_minus_F      = FALSE;
    PL_doswitches   = FALSE;
    PL_dowarn       = G_WARN_OFF;
    PL_doextract    = FALSE;
    PL_sawampersand = FALSE;	/* must save all match strings */
    PL_unsafe       = FALSE;

    Safefree(PL_inplace);
    PL_inplace = Nullch;
    SvREFCNT_dec(PL_patchlevel);

    if (PL_e_script) {
	SvREFCNT_dec(PL_e_script);
	PL_e_script = Nullsv;
    }

    PL_perldb = 0;

    /* magical thingies */

    SvREFCNT_dec(PL_ofs_sv);	/* $, */
    PL_ofs_sv = Nullsv;

    SvREFCNT_dec(PL_ors_sv);	/* $\ */
    PL_ors_sv = Nullsv;

    SvREFCNT_dec(PL_rs);	/* $/ */
    PL_rs = Nullsv;

    PL_multiline = 0;		/* $* */
    Safefree(PL_osname);	/* $^O */
    PL_osname = Nullch;

    SvREFCNT_dec(PL_statname);
    PL_statname = Nullsv;
    PL_statgv = Nullgv;

    /* defgv, aka *_ should be taken care of elsewhere */

    /* clean up after study() */
    SvREFCNT_dec(PL_lastscream);
    PL_lastscream = Nullsv;
    Safefree(PL_screamfirst);
    PL_screamfirst = 0;
    Safefree(PL_screamnext);
    PL_screamnext  = 0;

    /* float buffer */
    Safefree(PL_efloatbuf);
    PL_efloatbuf = Nullch;
    PL_efloatsize = 0;

    /* startup and shutdown function lists */
    SvREFCNT_dec(PL_beginav);
    SvREFCNT_dec(PL_beginav_save);
    SvREFCNT_dec(PL_endav);
    SvREFCNT_dec(PL_checkav);
    SvREFCNT_dec(PL_checkav_save);
    SvREFCNT_dec(PL_initav);
    PL_beginav = Nullav;
    PL_beginav_save = Nullav;
    PL_endav = Nullav;
    PL_checkav = Nullav;
    PL_checkav_save = Nullav;
    PL_initav = Nullav;

    /* shortcuts just get cleared */
    PL_envgv = Nullgv;
    PL_incgv = Nullgv;
    PL_hintgv = Nullgv;
    PL_errgv = Nullgv;
    PL_argvgv = Nullgv;
    PL_argvoutgv = Nullgv;
    PL_stdingv = Nullgv;
    PL_stderrgv = Nullgv;
    PL_last_in_gv = Nullgv;
    PL_replgv = Nullgv;
    PL_DBgv = Nullgv;
    PL_DBline = Nullgv;
    PL_DBsub = Nullgv;
    PL_DBsingle = Nullsv;
    PL_DBtrace = Nullsv;
    PL_DBsignal = Nullsv;
    PL_DBcv = Nullcv;
    PL_dbargs = Nullav;
    PL_debstash = Nullhv;

    SvREFCNT_dec(PL_argvout_stack);
    PL_argvout_stack = Nullav;

    SvREFCNT_dec(PL_modglobal);
    PL_modglobal = Nullhv;
    SvREFCNT_dec(PL_preambleav);
    PL_preambleav = Nullav;
    SvREFCNT_dec(PL_subname);
    PL_subname = Nullsv;
    SvREFCNT_dec(PL_linestr);
    PL_linestr = Nullsv;
    SvREFCNT_dec(PL_pidstatus);
    PL_pidstatus = Nullhv;
    SvREFCNT_dec(PL_toptarget);
    PL_toptarget = Nullsv;
    SvREFCNT_dec(PL_bodytarget);
    PL_bodytarget = Nullsv;
    PL_formtarget = Nullsv;

    /* free locale stuff */
#ifdef USE_LOCALE_COLLATE
    Safefree(PL_collation_name);
    PL_collation_name = Nullch;
#endif

#ifdef USE_LOCALE_NUMERIC
    Safefree(PL_numeric_name);
    PL_numeric_name = Nullch;
    SvREFCNT_dec(PL_numeric_radix_sv);
    PL_numeric_radix_sv = Nullsv;
#endif

    /* clear utf8 character classes */
    SvREFCNT_dec(PL_utf8_alnum);
    SvREFCNT_dec(PL_utf8_alnumc);
    SvREFCNT_dec(PL_utf8_ascii);
    SvREFCNT_dec(PL_utf8_alpha);
    SvREFCNT_dec(PL_utf8_space);
    SvREFCNT_dec(PL_utf8_cntrl);
    SvREFCNT_dec(PL_utf8_graph);
    SvREFCNT_dec(PL_utf8_digit);
    SvREFCNT_dec(PL_utf8_upper);
    SvREFCNT_dec(PL_utf8_lower);
    SvREFCNT_dec(PL_utf8_print);
    SvREFCNT_dec(PL_utf8_punct);
    SvREFCNT_dec(PL_utf8_xdigit);
    SvREFCNT_dec(PL_utf8_mark);
    SvREFCNT_dec(PL_utf8_toupper);
    SvREFCNT_dec(PL_utf8_totitle);
    SvREFCNT_dec(PL_utf8_tolower);
    SvREFCNT_dec(PL_utf8_tofold);
    SvREFCNT_dec(PL_utf8_idstart);
    SvREFCNT_dec(PL_utf8_idcont);
    PL_utf8_alnum	= Nullsv;
    PL_utf8_alnumc	= Nullsv;
    PL_utf8_ascii	= Nullsv;
    PL_utf8_alpha	= Nullsv;
    PL_utf8_space	= Nullsv;
    PL_utf8_cntrl	= Nullsv;
    PL_utf8_graph	= Nullsv;
    PL_utf8_digit	= Nullsv;
    PL_utf8_upper	= Nullsv;
    PL_utf8_lower	= Nullsv;
    PL_utf8_print	= Nullsv;
    PL_utf8_punct	= Nullsv;
    PL_utf8_xdigit	= Nullsv;
    PL_utf8_mark	= Nullsv;
    PL_utf8_toupper	= Nullsv;
    PL_utf8_totitle	= Nullsv;
    PL_utf8_tolower	= Nullsv;
    PL_utf8_tofold	= Nullsv;
    PL_utf8_idstart	= Nullsv;
    PL_utf8_idcont	= Nullsv;

    if (!specialWARN(PL_compiling.cop_warnings))
	SvREFCNT_dec(PL_compiling.cop_warnings);
    PL_compiling.cop_warnings = Nullsv;
    if (!specialCopIO(PL_compiling.cop_io))
	SvREFCNT_dec(PL_compiling.cop_io);
    PL_compiling.cop_io = Nullsv;
    CopFILE_free(&PL_compiling);
    CopSTASH_free(&PL_compiling);

    /* Prepare to destruct main symbol table.  */

    hv = PL_defstash;
    PL_defstash = 0;
    SvREFCNT_dec(hv);
    SvREFCNT_dec(PL_curstname);
    PL_curstname = Nullsv;

    /* clear queued errors */
    SvREFCNT_dec(PL_errors);
    PL_errors = Nullsv;

    FREETMPS;
    if (destruct_level >= 2 && ckWARN_d(WARN_INTERNAL)) {
	if (PL_scopestack_ix != 0)
	    Perl_warner(aTHX_ packWARN(WARN_INTERNAL),
	         "Unbalanced scopes: %ld more ENTERs than LEAVEs\n",
		 (long)PL_scopestack_ix);
	if (PL_savestack_ix != 0)
	    Perl_warner(aTHX_ packWARN(WARN_INTERNAL),
		 "Unbalanced saves: %ld more saves than restores\n",
		 (long)PL_savestack_ix);
	if (PL_tmps_floor != -1)
	    Perl_warner(aTHX_ packWARN(WARN_INTERNAL),"Unbalanced tmps: %ld more allocs than frees\n",
		 (long)PL_tmps_floor + 1);
	if (cxstack_ix != -1)
	    Perl_warner(aTHX_ packWARN(WARN_INTERNAL),"Unbalanced context: %ld more PUSHes than POPs\n",
		 (long)cxstack_ix + 1);
    }

    /* Now absolutely destruct everything, somehow or other, loops or no. */
    SvFLAGS(PL_fdpid) |= SVTYPEMASK;		/* don't clean out pid table now */
    SvFLAGS(PL_strtab) |= SVTYPEMASK;		/* don't clean out strtab now */

    /* the 2 is for PL_fdpid and PL_strtab */
    while (PL_sv_count > 2 && sv_clean_all())
	;

    SvFLAGS(PL_fdpid) &= ~SVTYPEMASK;
    SvFLAGS(PL_fdpid) |= SVt_PVAV;
    SvFLAGS(PL_strtab) &= ~SVTYPEMASK;
    SvFLAGS(PL_strtab) |= SVt_PVHV;

    AvREAL_off(PL_fdpid);		/* no surviving entries */
    SvREFCNT_dec(PL_fdpid);		/* needed in io_close() */
    PL_fdpid = Nullav;

#ifdef HAVE_INTERP_INTERN
    sys_intern_clear();
#endif

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
	max = HvMAX(PL_strtab);
	array = HvARRAY(PL_strtab);
	hent = array[0];
	for (;;) {
	    if (hent && ckWARN_d(WARN_INTERNAL)) {
		Perl_warner(aTHX_ packWARN(WARN_INTERNAL),
		     "Unbalanced string table refcount: (%d) for \"%s\"",
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
    SvREFCNT_dec(PL_strtab);

#ifdef USE_ITHREADS
    /* free the pointer table used for cloning */
    ptr_table_free(PL_ptr_table);
    PL_ptr_table = (PTR_TBL_t*)NULL;
#endif

    /* free special SVs */

    SvREFCNT(&PL_sv_yes) = 0;
    sv_clear(&PL_sv_yes);
    SvANY(&PL_sv_yes) = NULL;
    SvFLAGS(&PL_sv_yes) = 0;

    SvREFCNT(&PL_sv_no) = 0;
    sv_clear(&PL_sv_no);
    SvANY(&PL_sv_no) = NULL;
    SvFLAGS(&PL_sv_no) = 0;

    {
        int i;
        for (i=0; i<=2; i++) {
            SvREFCNT(PERL_DEBUG_PAD(i)) = 0;
            sv_clear(PERL_DEBUG_PAD(i));
            SvANY(PERL_DEBUG_PAD(i)) = NULL;
            SvFLAGS(PERL_DEBUG_PAD(i)) = 0;
        }
    }

    if (PL_sv_count != 0 && ckWARN_d(WARN_INTERNAL))
	Perl_warner(aTHX_ packWARN(WARN_INTERNAL),"Scalars leaked: %ld\n", (long)PL_sv_count);

#ifdef DEBUG_LEAKING_SCALARS
    if (PL_sv_count != 0) {
	SV* sva;
	SV* sv;
	register SV* svend;

	for (sva = PL_sv_arenaroot; sva; sva = (SV*)SvANY(sva)) {
	    svend = &sva[SvREFCNT(sva)];
	    for (sv = sva + 1; sv < svend; ++sv) {
		if (SvTYPE(sv) != SVTYPEMASK) {
		    PerlIO_printf(Perl_debug_log, "leaked: sv=0x%p"
			" flags=0x08%"UVxf
			" refcnt=%"UVuf pTHX__FORMAT "\n",
			sv, sv->sv_flags, sv->sv_refcnt pTHX__VALUE);
		}
	    }
	}
    }
#endif
    PL_sv_count = 0;


#if defined(PERLIO_LAYERS)
    /* No more IO - including error messages ! */
    PerlIO_cleanup(aTHX);
#endif

    /* sv_undef needs to stay immortal until after PerlIO_cleanup
       as currently layers use it rather than Nullsv as a marker
       for no arg - and will try and SvREFCNT_dec it.
     */
    SvREFCNT(&PL_sv_undef) = 0;
    SvREADONLY_off(&PL_sv_undef);

    Safefree(PL_origfilename);
    PL_origfilename = Nullch;
    Safefree(PL_reg_start_tmp);
    PL_reg_start_tmp = (char**)NULL;
    PL_reg_start_tmpl = 0;
    if (PL_reg_curpm)
	Safefree(PL_reg_curpm);
    Safefree(PL_reg_poscache);
    free_tied_hv_pool();
    Safefree(PL_op_mask);
    Safefree(PL_psig_ptr);
    PL_psig_ptr = (SV**)NULL;
    Safefree(PL_psig_name);
    PL_psig_name = (SV**)NULL;
    Safefree(PL_bitcount);
    PL_bitcount = Nullch;
    Safefree(PL_psig_pend);
    PL_psig_pend = (int*)NULL;
    PL_formfeed = Nullsv;
    Safefree(PL_ofmt);
    PL_ofmt = Nullch;
    nuke_stacks();
    PL_tainting = FALSE;
    PL_taint_warn = FALSE;
    PL_hints = 0;		/* Reset hints. Should hints be per-interpreter ? */
    PL_debug = 0;

    DEBUG_P(debprofdump());
#ifdef USE_5005THREADS
    MUTEX_DESTROY(&PL_strtab_mutex);
    MUTEX_DESTROY(&PL_sv_mutex);
    MUTEX_DESTROY(&PL_eval_mutex);
    MUTEX_DESTROY(&PL_cred_mutex);
    MUTEX_DESTROY(&PL_fdpid_mutex);
    COND_DESTROY(&PL_eval_cond);
#ifdef EMULATE_ATOMIC_REFCOUNTS
    MUTEX_DESTROY(&PL_svref_mutex);
#endif /* EMULATE_ATOMIC_REFCOUNTS */

    /* As the penultimate thing, free the non-arena SV for thrsv */
    Safefree(SvPVX(PL_thrsv));
    Safefree(SvANY(PL_thrsv));
    Safefree(PL_thrsv);
    PL_thrsv = Nullsv;
#endif /* USE_5005THREADS */

#ifdef USE_REENTRANT_API
    Perl_reentrant_free(aTHX);
#endif

    sv_free_arenas();

    /* As the absolutely last thing, free the non-arena SV for mess() */

    if (PL_mess_sv) {
	/* it could have accumulated taint magic */
	if (SvTYPE(PL_mess_sv) >= SVt_PVMG) {
	    MAGIC* mg;
	    MAGIC* moremagic;
	    for (mg = SvMAGIC(PL_mess_sv); mg; mg = moremagic) {
		moremagic = mg->mg_moremagic;
		if (mg->mg_ptr && mg->mg_type != PERL_MAGIC_regex_global
						&& mg->mg_len >= 0)
		    Safefree(mg->mg_ptr);
		Safefree(mg);
	    }
	}
	/* we know that type >= SVt_PV */
	SvOOK_off(PL_mess_sv);
	Safefree(SvPVX(PL_mess_sv));
	Safefree(SvANY(PL_mess_sv));
	Safefree(PL_mess_sv);
	PL_mess_sv = Nullsv;
    }
    return STATUS_NATIVE_EXPORT;
}

/*
=for apidoc perl_free

Releases a Perl interpreter.  See L<perlembed>.

=cut
*/

void
perl_free(pTHXx)
{
#if defined(WIN32) || defined(NETWARE)
#  if defined(PERL_IMPLICIT_SYS)
#    ifdef NETWARE
    void *host = nw_internal_host;
#    else
    void *host = w32_internal_host;
#    endif
    PerlMem_free(aTHXx);
#    ifdef NETWARE
    nw_delete_internal_host(host);
#    else
    win32_delete_internal_host(host);
#    endif
#  else
    PerlMem_free(aTHXx);
#  endif
#else
    PerlMem_free(aTHXx);
#endif
}

void
Perl_call_atexit(pTHX_ ATEXIT_t fn, void *ptr)
{
    Renew(PL_exitlist, PL_exitlistlen+1, PerlExitListEntry);
    PL_exitlist[PL_exitlistlen].fn = fn;
    PL_exitlist[PL_exitlistlen].ptr = ptr;
    ++PL_exitlistlen;
}

/*
=for apidoc perl_parse

Tells a Perl interpreter to parse a Perl script.  See L<perlembed>.

=cut
*/

int
perl_parse(pTHXx_ XSINIT_t xsinit, int argc, char **argv, char **env)
{
    I32 oldscope;
    int ret;
    dJMPENV;
#ifdef USE_5005THREADS
    dTHX;
#endif

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
#ifdef IAMSUID
#undef IAMSUID
    Perl_croak(aTHX_ "suidperl is no longer needed since the kernel can now execute\n\
setuid perl scripts securely.\n");
#endif /* IAMSUID */
#endif

#if defined(USE_HASH_SEED) || defined(USE_HASH_SEED_EXPLICIT)
    /* [perl #22371] Algorimic Complexity Attack on Perl 5.6.1, 5.8.0
     * This MUST be done before any hash stores or fetches take place.
     * If you set PL_rehash_seed (and assumedly also PL_rehash_seed_set)
     * yourself, it is your responsibility to provide a good random seed!
     * You can also define PERL_HASH_SEED in compile time, see hv.h. */
    if (!PL_rehash_seed_set)
	 PL_rehash_seed = get_hash_seed();
    {
	 char *s = PerlEnv_getenv("PERL_HASH_SEED_DEBUG");

	 if (s) {
	      int i = atoi(s);

	      if (i == 1)
		   PerlIO_printf(Perl_debug_log, "HASH_SEED = %"UVuf"\n",
				 PL_rehash_seed);
	 }
    }
#endif /* #if defined(USE_HASH_SEED) || defined(USE_HASH_SEED_EXPLICIT) */

    PL_origargc = argc;
    PL_origargv = argv;

    {
	/* Set PL_origalen be the sum of the contiguous argv[]
	 * elements plus the size of the env in case that it is
	 * contiguous with the argv[].  This is used in mg.c:Perl_magic_set()
	 * as the maximum modifiable length of $0.  In the worst case
	 * the area we are able to modify is limited to the size of
	 * the original argv[0].  (See below for 'contiguous', though.)
	 * --jhi */
	 char *s = NULL;
	 int i;
	 UV mask =
	   ~(UV)(PTRSIZE == 4 ? 3 : PTRSIZE == 8 ? 7 : PTRSIZE == 16 ? 15 : 0);
         /* Do the mask check only if the args seem like aligned. */
	 UV aligned =
	   (mask < ~(UV)0) && ((PTR2UV(argv[0]) & mask) == PTR2UV(argv[0]));

	 /* See if all the arguments are contiguous in memory.  Note
	  * that 'contiguous' is a loose term because some platforms
	  * align the argv[] and the envp[].  If the arguments look
	  * like non-aligned, assume that they are 'strictly' or
	  * 'traditionally' contiguous.  If the arguments look like
	  * aligned, we just check that they are within aligned
	  * PTRSIZE bytes.  As long as no system has something bizarre
	  * like the argv[] interleaved with some other data, we are
	  * fine.  (Did I just evoke Murphy's Law?)  --jhi */
	 if (PL_origargv && PL_origargc >= 1 && (s = PL_origargv[0])) {
	      while (*s) s++;
	      for (i = 1; i < PL_origargc; i++) {
		   if ((PL_origargv[i] == s + 1
#ifdef OS2
			|| PL_origargv[i] == s + 2
#endif 
			    )
		       ||
		       (aligned &&
			(PL_origargv[i] >  s &&
			 PL_origargv[i] <=
			 INT2PTR(char *, PTR2UV(s + PTRSIZE) & mask)))
			)
		   {
			s = PL_origargv[i];
			while (*s) s++;
		   }
		   else
			break;
	      }
	 }
	 /* Can we grab env area too to be used as the area for $0? */
	 if (PL_origenviron) {
	      if ((PL_origenviron[0] == s + 1
#ifdef OS2
		   || (PL_origenviron[0] == s + 9 && (s += 8))
#endif 
		  )
		  ||
		  (aligned &&
		   (PL_origenviron[0] >  s &&
		    PL_origenviron[0] <=
		    INT2PTR(char *, PTR2UV(s + PTRSIZE) & mask)))
		 )
	      {
#ifndef OS2
		   s = PL_origenviron[0];
		   while (*s) s++;
#endif
		   my_setenv("NoNe  SuCh", Nullch);
		   /* Force copy of environment. */
		   for (i = 1; PL_origenviron[i]; i++) {
			if (PL_origenviron[i] == s + 1
			    ||
			    (aligned &&
			     (PL_origenviron[i] >  s &&
			      PL_origenviron[i] <=
			      INT2PTR(char *, PTR2UV(s + PTRSIZE) & mask)))
			   )
			{
			     s = PL_origenviron[i];
			     while (*s) s++;
			}
			else
			     break;
		   }
	      }
	 }
	 PL_origalen = s - PL_origargv[0];
    }

    if (PL_do_undump) {

	/* Come here if running an undumped a.out. */

	PL_origfilename = savepv(argv[0]);
	PL_do_undump = FALSE;
	cxstack_ix = -1;		/* start label stack again */
	init_ids();
	init_postdump_symbols(argc,argv,env);
	return 0;
    }

    if (PL_main_root) {
	op_free(PL_main_root);
	PL_main_root = Nullop;
    }
    PL_main_start = Nullop;
    SvREFCNT_dec(PL_main_cv);
    PL_main_cv = Nullcv;

    time(&PL_basetime);
    oldscope = PL_scopestack_ix;
    PL_dowarn = G_WARN_OFF;

#ifdef PERL_FLEXIBLE_EXCEPTIONS
    CALLPROTECT(aTHX_ pcur_env, &ret, MEMBER_TO_FPTR(S_vparse_body), env, xsinit);
#else
    JMPENV_PUSH(ret);
#endif
    switch (ret) {
    case 0:
#ifndef PERL_FLEXIBLE_EXCEPTIONS
	parse_body(env,xsinit);
#endif
	if (PL_checkav)
	    call_list(oldscope, PL_checkav);
	ret = 0;
	break;
    case 1:
	STATUS_ALL_FAILURE;
	/* FALL THROUGH */
    case 2:
	/* my_exit() was called */
	while (PL_scopestack_ix > oldscope)
	    LEAVE;
	FREETMPS;
	PL_curstash = PL_defstash;
	if (PL_checkav)
	    call_list(oldscope, PL_checkav);
	ret = STATUS_NATIVE_EXPORT;
	break;
    case 3:
	PerlIO_printf(Perl_error_log, "panic: top_env\n");
	ret = 1;
	break;
    }
    JMPENV_POP;
    return ret;
}

#ifdef PERL_FLEXIBLE_EXCEPTIONS
STATIC void *
S_vparse_body(pTHX_ va_list args)
{
    char **env = va_arg(args, char**);
    XSINIT_t xsinit = va_arg(args, XSINIT_t);

    return parse_body(env, xsinit);
}
#endif

STATIC void *
S_parse_body(pTHX_ char **env, XSINIT_t xsinit)
{
    int argc = PL_origargc;
    char **argv = PL_origargv;
    char *scriptname = NULL;
    VOL bool dosearch = FALSE;
    char *validarg = "";
    register SV *sv;
    register char *s;
    char *cddir = Nullch;

    PL_fdscript = -1;
    PL_suidscript = -1;
    sv_setpvn(PL_linestr,"",0);
    sv = newSVpvn("",0);		/* first used for -I flags */
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
    /*
     * Can we rely on the kernel to start scripts with argv[1] set to
     * contain all #! line switches (the whole line)? (argv[0] is set to
     * the interpreter name, argv[2] to the script name; argv[3] and
     * above may contain other arguments.)
     */
#endif
	s = argv[0]+1;
      reswitch:
	switch (*s) {
	case 'C':
#ifndef PERL_STRICT_CR
	case '\r':
#endif
	case ' ':
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
	case 'W':
	case 'X':
	case 'w':
	    if ((s = moreswitches(s)))
		goto reswitch;
	    break;

	case 't':
	    CHECK_MALLOC_TOO_LATE_FOR('t');
	    if( !PL_tainting ) {
	         PL_taint_warn = TRUE;
	         PL_tainting = TRUE;
	    }
	    s++;
	    goto reswitch;
	case 'T':
	    CHECK_MALLOC_TOO_LATE_FOR('T');
	    PL_tainting = TRUE;
	    PL_taint_warn = FALSE;
	    s++;
	    goto reswitch;

	case 'e':
#ifdef MACOS_TRADITIONAL
	    /* ignore -e for Dev:Pseudo argument */
	    if (argv[1] && !strcmp(argv[1], "Dev:Pseudo"))
		break;
#endif
	    forbid_setid("-e");
	    if (!PL_e_script) {
		PL_e_script = newSVpvn("",0);
		filter_add(read_e_script, NULL);
	    }
	    if (*++s)
		sv_catpv(PL_e_script, s);
	    else if (argv[1]) {
		sv_catpv(PL_e_script, argv[1]);
		argc--,argv++;
	    }
	    else
		Perl_croak(aTHX_ "No code specified for -e");
	    sv_catpv(PL_e_script, "\n");
	    break;

	case 'I':	/* -I handled both here and in moreswitches() */
	    forbid_setid("-I");
	    if (!*++s && (s=argv[1]) != Nullch) {
		argc--,argv++;
	    }
	    if (s && *s) {
		char *p;
		STRLEN len = strlen(s);
		p = savepvn(s, len);
		incpush(p, TRUE, TRUE, FALSE);
		sv_catpvn(sv, "-I", 2);
		sv_catpvn(sv, p, len);
		sv_catpvn(sv, " ", 1);
		Safefree(p);
	    }
	    else
		Perl_croak(aTHX_ "No directory specified for -I");
	    break;
	case 'P':
	    forbid_setid("-P");
	    PL_preprocess = TRUE;
	    s++;
	    goto reswitch;
	case 'S':
	    forbid_setid("-S");
	    dosearch = TRUE;
	    s++;
	    goto reswitch;
	case 'V':
	    if (!PL_preambleav)
		PL_preambleav = newAV();
	    av_push(PL_preambleav, newSVpv("use Config qw(myconfig config_vars)",0));
	    if (*++s != ':')  {
		PL_Sv = newSVpv("print myconfig();",0);
#ifdef VMS
		sv_catpv(PL_Sv,"print \"\\nCharacteristics of this PERLSHR image: \\n\",");
#else
		sv_catpv(PL_Sv,"print \"\\nCharacteristics of this binary (from libperl): \\n\",");
#endif
		sv_catpv(PL_Sv,"\"  Compile-time options:");
#  ifdef DEBUGGING
		sv_catpv(PL_Sv," DEBUGGING");
#  endif
#  ifdef MULTIPLICITY
		sv_catpv(PL_Sv," MULTIPLICITY");
#  endif
#  ifdef USE_5005THREADS
		sv_catpv(PL_Sv," USE_5005THREADS");
#  endif
#  ifdef USE_ITHREADS
		sv_catpv(PL_Sv," USE_ITHREADS");
#  endif
#  ifdef USE_64_BIT_INT
		sv_catpv(PL_Sv," USE_64_BIT_INT");
#  endif
#  ifdef USE_64_BIT_ALL
		sv_catpv(PL_Sv," USE_64_BIT_ALL");
#  endif
#  ifdef USE_LONG_DOUBLE
		sv_catpv(PL_Sv," USE_LONG_DOUBLE");
#  endif
#  ifdef USE_LARGE_FILES
		sv_catpv(PL_Sv," USE_LARGE_FILES");
#  endif
#  ifdef USE_SOCKS
		sv_catpv(PL_Sv," USE_SOCKS");
#  endif
#  ifdef PERL_IMPLICIT_CONTEXT
		sv_catpv(PL_Sv," PERL_IMPLICIT_CONTEXT");
#  endif
#  ifdef PERL_IMPLICIT_SYS
		sv_catpv(PL_Sv," PERL_IMPLICIT_SYS");
#  endif
		sv_catpv(PL_Sv,"\\n\",");

#if defined(LOCAL_PATCH_COUNT)
		if (LOCAL_PATCH_COUNT > 0) {
		    int i;
		    sv_catpv(PL_Sv,"\"  Locally applied patches:\\n\",");
		    for (i = 1; i <= LOCAL_PATCH_COUNT; i++) {
			if (PL_localpatches[i])
			    Perl_sv_catpvf(aTHX_ PL_Sv,"q%c\t%s\n%c,",
				    0, PL_localpatches[i], 0);
		    }
		}
#endif
		Perl_sv_catpvf(aTHX_ PL_Sv,"\"  Built under %s\\n\"",OSNAME);
#ifdef __DATE__
#  ifdef __TIME__
		Perl_sv_catpvf(aTHX_ PL_Sv,",\"  Compiled at %s %s\\n\"",__DATE__,__TIME__);
#  else
		Perl_sv_catpvf(aTHX_ PL_Sv,",\"  Compiled on %s\\n\"",__DATE__);
#  endif
#endif
		sv_catpv(PL_Sv, "; \
$\"=\"\\n    \"; \
@env = map { \"$_=\\\"$ENV{$_}\\\"\" } sort grep {/^PERL/} keys %ENV; ");
#ifdef __CYGWIN__
		sv_catpv(PL_Sv,"\
push @env, \"CYGWIN=\\\"$ENV{CYGWIN}\\\"\";");
#endif
		sv_catpv(PL_Sv, "\
print \"  \\%ENV:\\n    @env\\n\" if @env; \
print \"  \\@INC:\\n    @INC\\n\";");
	    }
	    else {
		PL_Sv = newSVpv("config_vars(qw(",0);
		sv_catpv(PL_Sv, ++s);
		sv_catpv(PL_Sv, "))");
		s += strlen(s);
	    }
	    av_push(PL_preambleav, PL_Sv);
	    scriptname = BIT_BUCKET;	/* don't look for script or read stdin */
	    goto reswitch;
	case 'x':
	    PL_doextract = TRUE;
	    s++;
	    if (*s)
		cddir = s;
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
	    Perl_croak(aTHX_ "Unrecognized switch: -%s  (-h will show valid options)",s);
	}
    }
  switch_end:

    if (
#ifndef SECURE_INTERNAL_GETENV
        !PL_tainting &&
#endif
	(s = PerlEnv_getenv("PERL5OPT")))
    {
    	char *popt = s;
	while (isSPACE(*s))
	    s++;
	if (*s == '-' && *(s+1) == 'T') {
	    CHECK_MALLOC_TOO_LATE_FOR('T');
	    PL_tainting = TRUE;
            PL_taint_warn = FALSE;
	}
	else {
	    char *popt_copy = Nullch;
	    while (s && *s) {
	        char *d;
		while (isSPACE(*s))
		    s++;
		if (*s == '-') {
		    s++;
		    if (isSPACE(*s))
			continue;
		}
		d = s;
		if (!*s)
		    break;
		if (!strchr("DIMUdmtw", *s))
		    Perl_croak(aTHX_ "Illegal switch in PERL5OPT: -%c", *s);
		while (++s && *s) {
		    if (isSPACE(*s)) {
			if (!popt_copy) {
			    popt_copy = SvPVX(sv_2mortal(newSVpv(popt,0)));
			    s = popt_copy + (s - popt);
			    d = popt_copy + (d - popt);
			}
		        *s++ = '\0';
			break;
		    }
		}
		if (*d == 't') {
		    if( !PL_tainting ) {
		        PL_taint_warn = TRUE;
		        PL_tainting = TRUE;
		    }
		} else {
		    moreswitches(d);
		}
	    }
	}
    }

    if (PL_taint_warn && PL_dowarn != G_WARN_ALL_OFF) {
       PL_compiling.cop_warnings = newSVpvn(WARN_TAINTstring, WARNsize);
    }

    if (!scriptname)
	scriptname = argv[0];
    if (PL_e_script) {
	argc++,argv--;
	scriptname = BIT_BUCKET;	/* don't look for script or read stdin */
    }
    else if (scriptname == Nullch) {
#ifdef MSDOS
	if ( PerlLIO_isatty(PerlIO_fileno(PerlIO_stdin())) )
	    moreswitches("h");
#endif
	scriptname = "-";
    }

    init_perllib();

    open_script(scriptname,dosearch,sv);

    validate_suid(validarg, scriptname);

#ifndef PERL_MICRO
#if defined(SIGCHLD) || defined(SIGCLD)
    {
#ifndef SIGCHLD
#  define SIGCHLD SIGCLD
#endif
	Sighandler_t sigstate = rsignal_state(SIGCHLD);
	if (sigstate == SIG_IGN) {
	    if (ckWARN(WARN_SIGNAL))
		Perl_warner(aTHX_ packWARN(WARN_SIGNAL),
			    "Can't ignore signal CHLD, forcing to default");
	    (void)rsignal(SIGCHLD, (Sighandler_t)SIG_DFL);
	}
    }
#endif
#endif

#ifdef MACOS_TRADITIONAL
    if (PL_doextract || gMacPerl_AlwaysExtract) {
#else
    if (PL_doextract) {
#endif
	find_beginning();
	if (cddir && PerlDir_chdir(cddir) < 0)
	    Perl_croak(aTHX_ "Can't chdir to %s",cddir);

    }

    PL_main_cv = PL_compcv = (CV*)NEWSV(1104,0);
    sv_upgrade((SV *)PL_compcv, SVt_PVCV);
    CvUNIQUE_on(PL_compcv);

    CvPADLIST(PL_compcv) = pad_new(0);
#ifdef USE_5005THREADS
    CvOWNER(PL_compcv) = 0;
    New(666, CvMUTEXP(PL_compcv), 1, perl_mutex);
    MUTEX_INIT(CvMUTEXP(PL_compcv));
#endif /* USE_5005THREADS */

    boot_core_PerlIO();
    boot_core_UNIVERSAL();
    boot_core_xsutils();

    if (xsinit)
	(*xsinit)(aTHX);	/* in case linked C routines want magical variables */
#ifndef PERL_MICRO
#if defined(VMS) || defined(WIN32) || defined(DJGPP) || defined(__CYGWIN__) || defined(EPOC)
    init_os_extras();
#endif
#endif

#ifdef USE_SOCKS
#   ifdef HAS_SOCKS5_INIT
    socks5_init(argv[0]);
#   else
    SOCKSinit(argv[0]);
#   endif
#endif

    init_predump_symbols();
    /* init_postdump_symbols not currently designed to be called */
    /* more than once (ENV isn't cleared first, for example)	 */
    /* But running with -u leaves %ENV & @ARGV undefined!    XXX */
    if (!PL_do_undump)
	init_postdump_symbols(argc,argv,env);

    /* PL_unicode is turned on by -C or by $ENV{PERL_UNICODE}.
     * PL_utf8locale is conditionally turned on by
     * locale.c:Perl_init_i18nl10n() if the environment
     * look like the user wants to use UTF-8. */
    if (PL_unicode) {
	 /* Requires init_predump_symbols(). */
	 if (!(PL_unicode & PERL_UNICODE_LOCALE_FLAG) || PL_utf8locale) {
	      IO* io;
	      PerlIO* fp;
	      SV* sv;

	      /* Turn on UTF-8-ness on STDIN, STDOUT, STDERR
	       * and the default open disciplines. */
	      if ((PL_unicode & PERL_UNICODE_STDIN_FLAG) &&
		  PL_stdingv  && (io = GvIO(PL_stdingv)) &&
		  (fp = IoIFP(io)))
		   PerlIO_binmode(aTHX_ fp, IoTYPE(io), 0, ":utf8");
	      if ((PL_unicode & PERL_UNICODE_STDOUT_FLAG) &&
		  PL_defoutgv && (io = GvIO(PL_defoutgv)) &&
		  (fp = IoOFP(io)))
		   PerlIO_binmode(aTHX_ fp, IoTYPE(io), 0, ":utf8");
	      if ((PL_unicode & PERL_UNICODE_STDERR_FLAG) &&
		  PL_stderrgv && (io = GvIO(PL_stderrgv)) &&
		  (fp = IoOFP(io)))
		   PerlIO_binmode(aTHX_ fp, IoTYPE(io), 0, ":utf8");
	      if ((PL_unicode & PERL_UNICODE_INOUT_FLAG) &&
		  (sv = GvSV(gv_fetchpv("\017PEN", TRUE, SVt_PV)))) {
		   U32 in  = PL_unicode & PERL_UNICODE_IN_FLAG;
		   U32 out = PL_unicode & PERL_UNICODE_OUT_FLAG;
		   if (in) {
			if (out)
			     sv_setpvn(sv, ":utf8\0:utf8", 11);
			else
			     sv_setpvn(sv, ":utf8\0", 6);
		   }
		   else if (out)
			sv_setpvn(sv, "\0:utf8", 6);
		   SvSETMAGIC(sv);
	      }
	 }
    }

    if ((s = PerlEnv_getenv("PERL_SIGNALS"))) {
	 if (strEQ(s, "unsafe"))
	      PL_signals |=  PERL_SIGNALS_UNSAFE_FLAG;
	 else if (strEQ(s, "safe"))
	      PL_signals &= ~PERL_SIGNALS_UNSAFE_FLAG;
	 else
	      Perl_croak(aTHX_ "PERL_SIGNALS illegal: \"%s\"", s);
    }

    init_lexer();

    /* now parse the script */

    SETERRNO(0,SS_NORMAL);
    PL_error_count = 0;
#ifdef MACOS_TRADITIONAL
    if (gMacPerl_SyntaxError = (yyparse() || PL_error_count)) {
	if (PL_minus_c)
	    Perl_croak(aTHX_ "%s had compilation errors.\n", MacPerl_MPWFileName(PL_origfilename));
	else {
	    Perl_croak(aTHX_ "Execution of %s aborted due to compilation errors.\n",
		       MacPerl_MPWFileName(PL_origfilename));
	}
    }
#else
    if (yyparse() || PL_error_count) {
	if (PL_minus_c)
	    Perl_croak(aTHX_ "%s had compilation errors.\n", PL_origfilename);
	else {
	    Perl_croak(aTHX_ "Execution of %s aborted due to compilation errors.\n",
		       PL_origfilename);
	}
    }
#endif
    CopLINE_set(PL_curcop, 0);
    PL_curstash = PL_defstash;
    PL_preprocess = FALSE;
    if (PL_e_script) {
	SvREFCNT_dec(PL_e_script);
	PL_e_script = Nullsv;
    }

    if (PL_do_undump)
	my_unexec();

    if (isWARN_ONCE) {
	SAVECOPFILE(PL_curcop);
	SAVECOPLINE(PL_curcop);
	gv_check(PL_defstash);
    }

    LEAVE;
    FREETMPS;

#ifdef MYMALLOC
    if ((s=PerlEnv_getenv("PERL_DEBUG_MSTATS")) && atoi(s) >= 2)
	dump_mstats("after compilation:");
#endif

    ENTER;
    PL_restartop = 0;
    return NULL;
}

/*
=for apidoc perl_run

Tells a Perl interpreter to run.  See L<perlembed>.

=cut
*/

int
perl_run(pTHXx)
{
    I32 oldscope;
    int ret = 0;
    dJMPENV;
#ifdef USE_5005THREADS
    dTHX;
#endif

    oldscope = PL_scopestack_ix;
#ifdef VMS
    VMSISH_HUSHED = 0;
#endif

#ifdef PERL_FLEXIBLE_EXCEPTIONS
 redo_body:
    CALLPROTECT(aTHX_ pcur_env, &ret, MEMBER_TO_FPTR(S_vrun_body), oldscope);
#else
    JMPENV_PUSH(ret);
#endif
    switch (ret) {
    case 1:
	cxstack_ix = -1;		/* start context stack again */
	goto redo_body;
    case 0:				/* normal completion */
#ifndef PERL_FLEXIBLE_EXCEPTIONS
 redo_body:
	run_body(oldscope);
#endif
	/* FALL THROUGH */
    case 2:				/* my_exit() */
	while (PL_scopestack_ix > oldscope)
	    LEAVE;
	FREETMPS;
	PL_curstash = PL_defstash;
	if (!(PL_exit_flags & PERL_EXIT_DESTRUCT_END) &&
	    PL_endav && !PL_minus_c)
	    call_list(oldscope, PL_endav);
#ifdef MYMALLOC
	if (PerlEnv_getenv("PERL_DEBUG_MSTATS"))
	    dump_mstats("after execution:  ");
#endif
	ret = STATUS_NATIVE_EXPORT;
	break;
    case 3:
	if (PL_restartop) {
	    POPSTACK_TO(PL_mainstack);
	    goto redo_body;
	}
	PerlIO_printf(Perl_error_log, "panic: restartop\n");
	FREETMPS;
	ret = 1;
	break;
    }

    JMPENV_POP;
    return ret;
}

#ifdef PERL_FLEXIBLE_EXCEPTIONS
STATIC void *
S_vrun_body(pTHX_ va_list args)
{
    I32 oldscope = va_arg(args, I32);

    return run_body(oldscope);
}
#endif


STATIC void *
S_run_body(pTHX_ I32 oldscope)
{
    DEBUG_r(PerlIO_printf(Perl_debug_log, "%s $` $& $' support.\n",
                    PL_sawampersand ? "Enabling" : "Omitting"));

    if (!PL_restartop) {
	DEBUG_x(dump_all());
	PERL_DEBUG(PerlIO_printf(Perl_debug_log, "\nEXECUTING...\n\n"));
	DEBUG_S(PerlIO_printf(Perl_debug_log, "main thread is 0x%"UVxf"\n",
			      PTR2UV(thr)));

	if (PL_minus_c) {
#ifdef MACOS_TRADITIONAL
	    PerlIO_printf(Perl_error_log, "%s%s syntax OK\n",
		(gMacPerl_ErrorFormat ? "# " : ""),
		MacPerl_MPWFileName(PL_origfilename));
#else
	    PerlIO_printf(Perl_error_log, "%s syntax OK\n", PL_origfilename);
#endif
	    my_exit(0);
	}
	if (PERLDB_SINGLE && PL_DBsingle)
	    sv_setiv(PL_DBsingle, 1);
	if (PL_initav)
	    call_list(oldscope, PL_initav);
    }

    /* do it */

    if (PL_restartop) {
	PL_op = PL_restartop;
	PL_restartop = 0;
	CALLRUNOPS(aTHX);
    }
    else if (PL_main_start) {
	CvDEPTH(PL_main_cv) = 1;
	PL_op = PL_main_start;
	CALLRUNOPS(aTHX);
    }

    my_exit(0);
    /* NOTREACHED */
    return NULL;
}

/*
=head1 SV Manipulation Functions

=for apidoc p||get_sv

Returns the SV of the specified Perl scalar.  If C<create> is set and the
Perl variable does not exist then it will be created.  If C<create> is not
set and the variable does not exist then NULL is returned.

=cut
*/

SV*
Perl_get_sv(pTHX_ const char *name, I32 create)
{
    GV *gv;
#ifdef USE_5005THREADS
    if (name[1] == '\0' && !isALPHA(name[0])) {
	PADOFFSET tmp = find_threadsv(name);
    	if (tmp != NOT_IN_PAD)
	    return THREADSV(tmp);
    }
#endif /* USE_5005THREADS */
    gv = gv_fetchpv(name, create, SVt_PV);
    if (gv)
	return GvSV(gv);
    return Nullsv;
}

/*
=head1 Array Manipulation Functions

=for apidoc p||get_av

Returns the AV of the specified Perl array.  If C<create> is set and the
Perl variable does not exist then it will be created.  If C<create> is not
set and the variable does not exist then NULL is returned.

=cut
*/

AV*
Perl_get_av(pTHX_ const char *name, I32 create)
{
    GV* gv = gv_fetchpv(name, create, SVt_PVAV);
    if (create)
    	return GvAVn(gv);
    if (gv)
	return GvAV(gv);
    return Nullav;
}

/*
=head1 Hash Manipulation Functions

=for apidoc p||get_hv

Returns the HV of the specified Perl hash.  If C<create> is set and the
Perl variable does not exist then it will be created.  If C<create> is not
set and the variable does not exist then NULL is returned.

=cut
*/

HV*
Perl_get_hv(pTHX_ const char *name, I32 create)
{
    GV* gv = gv_fetchpv(name, create, SVt_PVHV);
    if (create)
    	return GvHVn(gv);
    if (gv)
	return GvHV(gv);
    return Nullhv;
}

/*
=head1 CV Manipulation Functions

=for apidoc p||get_cv

Returns the CV of the specified Perl subroutine.  If C<create> is set and
the Perl subroutine does not exist then it will be declared (which has the
same effect as saying C<sub name;>).  If C<create> is not set and the
subroutine does not exist then NULL is returned.

=cut
*/

CV*
Perl_get_cv(pTHX_ const char *name, I32 create)
{
    GV* gv = gv_fetchpv(name, create, SVt_PVCV);
    /* XXX unsafe for threads if eval_owner isn't held */
    /* XXX this is probably not what they think they're getting.
     * It has the same effect as "sub name;", i.e. just a forward
     * declaration! */
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

/*

=head1 Callback Functions

=for apidoc p||call_argv

Performs a callback to the specified Perl sub.  See L<perlcall>.

=cut
*/

I32
Perl_call_argv(pTHX_ const char *sub_name, I32 flags, register char **argv)

          		/* See G_* flags in cop.h */
                     	/* null terminated arg list */
{
    dSP;

    PUSHMARK(SP);
    if (argv) {
	while (*argv) {
	    XPUSHs(sv_2mortal(newSVpv(*argv,0)));
	    argv++;
	}
	PUTBACK;
    }
    return call_pv(sub_name, flags);
}

/*
=for apidoc p||call_pv

Performs a callback to the specified Perl sub.  See L<perlcall>.

=cut
*/

I32
Perl_call_pv(pTHX_ const char *sub_name, I32 flags)
              		/* name of the subroutine */
          		/* See G_* flags in cop.h */
{
    return call_sv((SV*)get_cv(sub_name, TRUE), flags);
}

/*
=for apidoc p||call_method

Performs a callback to the specified Perl method.  The blessed object must
be on the stack.  See L<perlcall>.

=cut
*/

I32
Perl_call_method(pTHX_ const char *methname, I32 flags)
               		/* name of the subroutine */
          		/* See G_* flags in cop.h */
{
    return call_sv(sv_2mortal(newSVpv(methname,0)), flags | G_METHOD);
}

/* May be called with any of a CV, a GV, or an SV containing the name. */
/*
=for apidoc p||call_sv

Performs a callback to the Perl sub whose name is in the SV.  See
L<perlcall>.

=cut
*/

I32
Perl_call_sv(pTHX_ SV *sv, I32 flags)
          		/* See G_* flags in cop.h */
{
    dSP;
    LOGOP myop;		/* fake syntax tree node */
    UNOP method_op;
    I32 oldmark;
    volatile I32 retval = 0;
    I32 oldscope;
    bool oldcatch = CATCH_GET;
    int ret;
    OP* oldop = PL_op;
    dJMPENV;

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
    SAVEOP();
    PL_op = (OP*)&myop;

    EXTEND(PL_stack_sp, 1);
    *++PL_stack_sp = sv;
    oldmark = TOPMARK;
    oldscope = PL_scopestack_ix;

    if (PERLDB_SUB && PL_curstash != PL_debstash
	   /* Handle first BEGIN of -d. */
	  && (PL_DBcv || (PL_DBcv = GvCV(PL_DBsub)))
	   /* Try harder, since this may have been a sighandler, thus
	    * curstash may be meaningless. */
	  && (SvTYPE(sv) != SVt_PVCV || CvSTASH((CV*)sv) != PL_debstash)
	  && !(flags & G_NODEBUG))
	PL_op->op_private |= OPpENTERSUB_DB;

    if (flags & G_METHOD) {
	Zero(&method_op, 1, UNOP);
	method_op.op_next = PL_op;
	method_op.op_ppaddr = PL_ppaddr[OP_METHOD];
	myop.op_ppaddr = PL_ppaddr[OP_ENTERSUB];
	PL_op = (OP*)&method_op;
    }

    if (!(flags & G_EVAL)) {
	CATCH_SET(TRUE);
	call_body((OP*)&myop, FALSE);
	retval = PL_stack_sp - (PL_stack_base + oldmark);
	CATCH_SET(oldcatch);
    }
    else {
	myop.op_other = (OP*)&myop;
	PL_markstack_ptr--;
	/* we're trying to emulate pp_entertry() here */
	{
	    register PERL_CONTEXT *cx;
	    I32 gimme = GIMME_V;
	
	    ENTER;
	    SAVETMPS;
	
	    push_return(Nullop);
	    PUSHBLOCK(cx, (CXt_EVAL|CXp_TRYBLOCK), PL_stack_sp);
	    PUSHEVAL(cx, 0, 0);
	    PL_eval_root = PL_op;             /* Only needed so that goto works right. */
	
	    PL_in_eval = EVAL_INEVAL;
	    if (flags & G_KEEPERR)
		PL_in_eval |= EVAL_KEEPERR;
	    else
		sv_setpv(ERRSV,"");
	}
	PL_markstack_ptr++;

#ifdef PERL_FLEXIBLE_EXCEPTIONS
 redo_body:
	CALLPROTECT(aTHX_ pcur_env, &ret, MEMBER_TO_FPTR(S_vcall_body),
		    (OP*)&myop, FALSE);
#else
	JMPENV_PUSH(ret);
#endif
	switch (ret) {
	case 0:
#ifndef PERL_FLEXIBLE_EXCEPTIONS
 redo_body:
	    call_body((OP*)&myop, FALSE);
#endif
	    retval = PL_stack_sp - (PL_stack_base + oldmark);
	    if (!(flags & G_KEEPERR))
		sv_setpv(ERRSV,"");
	    break;
	case 1:
	    STATUS_ALL_FAILURE;
	    /* FALL THROUGH */
	case 2:
	    /* my_exit() was called */
	    PL_curstash = PL_defstash;
	    FREETMPS;
	    JMPENV_POP;
	    if (PL_statusvalue && !(PL_exit_flags & PERL_EXIT_EXPECTED))
		Perl_croak(aTHX_ "Callback called exit");
	    my_exit_jump();
	    /* NOTREACHED */
	case 3:
	    if (PL_restartop) {
		PL_op = PL_restartop;
		PL_restartop = 0;
		goto redo_body;
	    }
	    PL_stack_sp = PL_stack_base + oldmark;
	    if (flags & G_ARRAY)
		retval = 0;
	    else {
		retval = 1;
		*++PL_stack_sp = &PL_sv_undef;
	    }
	    break;
	}

	if (PL_scopestack_ix > oldscope) {
	    SV **newsp;
	    PMOP *newpm;
	    I32 gimme;
	    register PERL_CONTEXT *cx;
	    I32 optype;

	    POPBLOCK(cx,newpm);
	    POPEVAL(cx);
	    pop_return();
	    PL_curpm = newpm;
	    LEAVE;
	}
	JMPENV_POP;
    }

    if (flags & G_DISCARD) {
	PL_stack_sp = PL_stack_base + oldmark;
	retval = 0;
	FREETMPS;
	LEAVE;
    }
    PL_op = oldop;
    return retval;
}

#ifdef PERL_FLEXIBLE_EXCEPTIONS
STATIC void *
S_vcall_body(pTHX_ va_list args)
{
    OP *myop = va_arg(args, OP*);
    int is_eval = va_arg(args, int);

    call_body(myop, is_eval);
    return NULL;
}
#endif

STATIC void
S_call_body(pTHX_ OP *myop, int is_eval)
{
    if (PL_op == myop) {
	if (is_eval)
	    PL_op = Perl_pp_entereval(aTHX);	/* this doesn't do a POPMARK */
	else
	    PL_op = Perl_pp_entersub(aTHX);	/* this does */
    }
    if (PL_op)
	CALLRUNOPS(aTHX);
}

/* Eval a string. The G_EVAL flag is always assumed. */

/*
=for apidoc p||eval_sv

Tells Perl to C<eval> the string in the SV.

=cut
*/

I32
Perl_eval_sv(pTHX_ SV *sv, I32 flags)

          		/* See G_* flags in cop.h */
{
    dSP;
    UNOP myop;		/* fake syntax tree node */
    volatile I32 oldmark = SP - PL_stack_base;
    volatile I32 retval = 0;
    I32 oldscope;
    int ret;
    OP* oldop = PL_op;
    dJMPENV;

    if (flags & G_DISCARD) {
	ENTER;
	SAVETMPS;
    }

    SAVEOP();
    PL_op = (OP*)&myop;
    Zero(PL_op, 1, UNOP);
    EXTEND(PL_stack_sp, 1);
    *++PL_stack_sp = sv;
    oldscope = PL_scopestack_ix;

    if (!(flags & G_NOARGS))
	myop.op_flags = OPf_STACKED;
    myop.op_next = Nullop;
    myop.op_type = OP_ENTEREVAL;
    myop.op_flags |= ((flags & G_VOID) ? OPf_WANT_VOID :
		      (flags & G_ARRAY) ? OPf_WANT_LIST :
		      OPf_WANT_SCALAR);
    if (flags & G_KEEPERR)
	myop.op_flags |= OPf_SPECIAL;

#ifdef PERL_FLEXIBLE_EXCEPTIONS
 redo_body:
    CALLPROTECT(aTHX_ pcur_env, &ret, MEMBER_TO_FPTR(S_vcall_body),
		(OP*)&myop, TRUE);
#else
    /* fail now; otherwise we could fail after the JMPENV_PUSH but
     * before a PUSHEVAL, which corrupts the stack after a croak */
    TAINT_PROPER("eval_sv()");

    JMPENV_PUSH(ret);
#endif
    switch (ret) {
    case 0:
#ifndef PERL_FLEXIBLE_EXCEPTIONS
 redo_body:
	call_body((OP*)&myop,TRUE);
#endif
	retval = PL_stack_sp - (PL_stack_base + oldmark);
	if (!(flags & G_KEEPERR))
	    sv_setpv(ERRSV,"");
	break;
    case 1:
	STATUS_ALL_FAILURE;
	/* FALL THROUGH */
    case 2:
	/* my_exit() was called */
	PL_curstash = PL_defstash;
	FREETMPS;
	JMPENV_POP;
	if (PL_statusvalue && !(PL_exit_flags & PERL_EXIT_EXPECTED))
	    Perl_croak(aTHX_ "Callback called exit");
	my_exit_jump();
	/* NOTREACHED */
    case 3:
	if (PL_restartop) {
	    PL_op = PL_restartop;
	    PL_restartop = 0;
	    goto redo_body;
	}
	PL_stack_sp = PL_stack_base + oldmark;
	if (flags & G_ARRAY)
	    retval = 0;
	else {
	    retval = 1;
	    *++PL_stack_sp = &PL_sv_undef;
	}
	break;
    }

    JMPENV_POP;
    if (flags & G_DISCARD) {
	PL_stack_sp = PL_stack_base + oldmark;
	retval = 0;
	FREETMPS;
	LEAVE;
    }
    PL_op = oldop;
    return retval;
}

/*
=for apidoc p||eval_pv

Tells Perl to C<eval> the given string and return an SV* result.

=cut
*/

SV*
Perl_eval_pv(pTHX_ const char *p, I32 croak_on_error)
{
    dSP;
    SV* sv = newSVpv(p, 0);

    eval_sv(sv, G_SCALAR);
    SvREFCNT_dec(sv);

    SPAGAIN;
    sv = POPs;
    PUTBACK;

    if (croak_on_error && SvTRUE(ERRSV)) {
	STRLEN n_a;
	Perl_croak(aTHX_ SvPVx(ERRSV, n_a));
    }

    return sv;
}

/* Require a module. */

/*
=head1 Embedding Functions

=for apidoc p||require_pv

Tells Perl to C<require> the file named by the string argument.  It is
analogous to the Perl code C<eval "require '$file'">.  It's even
implemented that way; consider using load_module instead.

=cut */

void
Perl_require_pv(pTHX_ const char *pv)
{
    SV* sv;
    dSP;
    PUSHSTACKi(PERLSI_REQUIRE);
    PUTBACK;
    sv = sv_newmortal();
    sv_setpv(sv, "require '");
    sv_catpv(sv, pv);
    sv_catpv(sv, "'");
    eval_sv(sv, G_DISCARD);
    SPAGAIN;
    POPSTACK;
}

void
Perl_magicname(pTHX_ char *sym, char *name, I32 namlen)
{
    register GV *gv;

    if ((gv = gv_fetchpv(sym,TRUE, SVt_PV)))
	sv_magic(GvSV(gv), (SV*)gv, PERL_MAGIC_sv, name, namlen);
}

STATIC void
S_usage(pTHX_ char *name)		/* XXX move this out into a module ? */
{
    /* This message really ought to be max 23 lines.
     * Removed -h because the user already knows that option. Others? */

    static char *usage_msg[] = {
"-0[octal]       specify record separator (\\0, if no argument)",
"-a              autosplit mode with -n or -p (splits $_ into @F)",
"-C[number/list] enables the listed Unicode features",
"-c              check syntax only (runs BEGIN and CHECK blocks)",
"-d[:debugger]   run program under debugger",
"-D[number/list] set debugging flags (argument is a bit mask or alphabets)",
"-e program      one line of program (several -e's allowed, omit programfile)",
"-F/pattern/     split() pattern for -a switch (//'s are optional)",
"-i[extension]   edit <> files in place (makes backup if extension supplied)",
"-Idirectory     specify @INC/#include directory (several -I's allowed)",
"-l[octal]       enable line ending processing, specifies line terminator",
"-[mM][-]module  execute `use/no module...' before executing program",
"-n              assume 'while (<>) { ... }' loop around program",
"-p              assume loop like -n but print line also, like sed",
"-P              run program through C preprocessor before compilation",
"-s              enable rudimentary parsing for switches after programfile",
"-S              look for programfile using PATH environment variable",
"-t              enable tainting warnings",
"-T              enable tainting checks",
"-u              dump core after parsing program",
"-U              allow unsafe operations",
"-v              print version, subversion (includes VERY IMPORTANT perl info)",
"-V[:variable]   print configuration summary (or a single Config.pm variable)",
"-w              enable many useful warnings (RECOMMENDED)",
"-W              enable all warnings",
"-x[directory]   strip off text before #!perl line and perhaps cd to directory",
"-X              disable all warnings",
"\n",
NULL
};
    char **p = usage_msg;

    PerlIO_printf(PerlIO_stdout(),
		  "\nUsage: %s [switches] [--] [programfile] [arguments]",
		  name);
    while (*p)
	PerlIO_printf(PerlIO_stdout(), "\n  %s", *p++);
}

/* convert a string of -D options (or digits) into an int.
 * sets *s to point to the char after the options */

#ifdef DEBUGGING
int
Perl_get_debug_opts(pTHX_ char **s)
{
  return get_debug_opts_flags(s, 1);
}

int
Perl_get_debug_opts_flags(pTHX_ char **s, int flags)
{
    static char *usage_msgd[] = {
      " Debugging flag values: (see also -d)",
      "  p  Tokenizing and parsing (with v, displays parse stack)",
      "  s  Stack snapshots (with v, displays all stacks)",
      "  l  Context (loop) stack processing",
      "  t  Trace execution",
      "  o  Method and overloading resolution",
      "  c  String/numeric conversions",
      "  P  Print profiling info, preprocessor command for -P, source file input state",
      "  m  Memory allocation",
      "  f  Format processing",
      "  r  Regular expression parsing and execution",
      "  x  Syntax tree dump",
      "  u  Tainting checks",
      "  H  Hash dump -- usurps values()",
      "  X  Scratchpad allocation",
      "  D  Cleaning up",
      "  S  Thread synchronization",
      "  T  Tokenising",
      "  R  Include reference counts of dumped variables (eg when using -Ds)",
      "  J  Do not s,t,P-debug (Jump over) opcodes within package DB",
      "  v  Verbose: use in conjunction with other flags",
      "  C  Copy On Write",
      "  A  Consistency checks on internal structures",
      "  q  quiet - currently only suppresses the 'EXECUTING' message",
      NULL
    };
    int i = 0;
    if (isALPHA(**s)) {
	/* if adding extra options, remember to update DEBUG_MASK */
	static char debopts[] = "psltocPmfrxu HXDSTRJvC";

	for (; isALNUM(**s); (*s)++) {
	    char *d = strchr(debopts,**s);
	    if (d)
		i |= 1 << (d - debopts);
	    else if (ckWARN_d(WARN_DEBUGGING))
	        Perl_warner(aTHX_ packWARN(WARN_DEBUGGING),
		    "invalid option -D%c, use -D'' to see choices\n", **s);
	}
    }
    else if (isDIGIT(**s)) {
	i = atoi(*s);
	for (; isALNUM(**s); (*s)++) ;
    }
    else if (flags & 1) {
      /* Give help.  */
      char **p = usage_msgd;
      while (*p) PerlIO_printf(PerlIO_stdout(), "%s\n", *p++);
    }
#  ifdef EBCDIC
    if ((i & DEBUG_p_FLAG) && ckWARN_d(WARN_DEBUGGING))
	Perl_warner(aTHX_ packWARN(WARN_DEBUGGING),
		"-Dp not implemented on this platform\n");
#  endif
    return i;
}
#endif

/* This routine handles any switches that can be given during run */

char *
Perl_moreswitches(pTHX_ char *s)
{
    STRLEN numlen;
    UV rschar;

    switch (*s) {
    case '0':
    {
	 I32 flags = 0;

	 SvREFCNT_dec(PL_rs);
	 if (s[1] == 'x' && s[2]) {
	      char *e;
	      U8 *tmps;

	      for (s += 2, e = s; *e; e++);
	      numlen = e - s;
	      flags = PERL_SCAN_SILENT_ILLDIGIT;
	      rschar = (U32)grok_hex(s, &numlen, &flags, NULL);
	      if (s + numlen < e) {
		   rschar = 0; /* Grandfather -0xFOO as -0 -xFOO. */
		   numlen = 0;
		   s--;
	      }
	      PL_rs = newSVpvn("", 0);
	      SvGROW(PL_rs, (STRLEN)(UNISKIP(rschar) + 1));
	      tmps = (U8*)SvPVX(PL_rs);
	      uvchr_to_utf8(tmps, rschar);
	      SvCUR_set(PL_rs, UNISKIP(rschar));
	      SvUTF8_on(PL_rs);
	 }
	 else {
	      numlen = 4;
	      rschar = (U32)grok_oct(s, &numlen, &flags, NULL);
	      if (rschar & ~((U8)~0))
		   PL_rs = &PL_sv_undef;
	      else if (!rschar && numlen >= 2)
		   PL_rs = newSVpvn("", 0);
	      else {
		   char ch = (char)rschar;
		   PL_rs = newSVpvn(&ch, 1);
	      }
	 }
	 sv_setsv(get_sv("/", TRUE), PL_rs);
	 return s + numlen;
    }
    case 'C':
        s++;
        PL_unicode = parse_unicode_opts(&s);
	return s;
    case 'F':
	PL_minus_F = TRUE;
	PL_splitstr = ++s;
	while (*s && !isSPACE(*s)) ++s;
	*s = '\0';
	PL_splitstr = savepv(PL_splitstr);
	return s;
    case 'a':
	PL_minus_a = TRUE;
	s++;
	return s;
    case 'c':
	PL_minus_c = TRUE;
	s++;
	return s;
    case 'd':
	forbid_setid("-d");
	s++;

        /* -dt indicates to the debugger that threads will be used */
	if (*s == 't' && !isALNUM(s[1])) {
	    ++s;
	    my_setenv("PERL5DB_THREADED", "1");
	}

	/* The following permits -d:Mod to accepts arguments following an =
	   in the fashion that -MSome::Mod does. */
	if (*s == ':' || *s == '=') {
	    char *start;
	    SV *sv;
	    sv = newSVpv("use Devel::", 0);
	    start = ++s;
	    /* We now allow -d:Module=Foo,Bar */
	    while(isALNUM(*s) || *s==':') ++s;
	    if (*s != '=')
		sv_catpv(sv, start);
	    else {
		sv_catpvn(sv, start, s-start);
		sv_catpv(sv, " split(/,/,q{");
		sv_catpv(sv, ++s);
		sv_catpv(sv, "})");
	    }
	    s += strlen(s);
	    my_setenv("PERL5DB", SvPV(sv, PL_na));
	}
	if (!PL_perldb) {
	    PL_perldb = PERLDB_ALL;
	    init_debugger();
	}
	return s;
    case 'D':
    {	
#ifdef DEBUGGING
	forbid_setid("-D");
	s++;
	PL_debug = get_debug_opts_flags(&s, 1) | DEBUG_TOP_FLAG;
#else /* !DEBUGGING */
	if (ckWARN_d(WARN_DEBUGGING))
	    Perl_warner(aTHX_ packWARN(WARN_DEBUGGING),
	           "Recompile perl with -DDEBUGGING to use -D switch (did you mean -d ?)\n");
	for (s++; isALNUM(*s); s++) ;
#endif
	/*SUPPRESS 530*/
	return s;
    }	
    case 'h':
	usage(PL_origargv[0]);
	my_exit(0);
    case 'i':
	if (PL_inplace)
	    Safefree(PL_inplace);
#if defined(__CYGWIN__) /* do backup extension automagically */
	if (*(s+1) == '\0') {
	PL_inplace = savepv(".bak");
	return s+1;
	}
#endif /* __CYGWIN__ */
	PL_inplace = savepv(s+1);
	/*SUPPRESS 530*/
	for (s = PL_inplace; *s && !isSPACE(*s); s++) ;
	if (*s) {
	    *s++ = '\0';
	    if (*s == '-')	/* Additional switches on #! line. */
	        s++;
	}
	return s;
    case 'I':	/* -I handled both here and in parse_body() */
	forbid_setid("-I");
	++s;
	while (*s && isSPACE(*s))
	    ++s;
	if (*s) {
	    char *e, *p;
	    p = s;
	    /* ignore trailing spaces (possibly followed by other switches) */
	    do {
		for (e = p; *e && !isSPACE(*e); e++) ;
		p = e;
		while (isSPACE(*p))
		    p++;
	    } while (*p && *p != '-');
	    e = savepvn(s, e-s);
	    incpush(e, TRUE, TRUE, FALSE);
	    Safefree(e);
	    s = p;
	    if (*s == '-')
		s++;
	}
	else
	    Perl_croak(aTHX_ "No directory specified for -I");
	return s;
    case 'l':
	PL_minus_l = TRUE;
	s++;
	if (PL_ors_sv) {
	    SvREFCNT_dec(PL_ors_sv);
	    PL_ors_sv = Nullsv;
	}
	if (isDIGIT(*s)) {
            I32 flags = 0;
	    PL_ors_sv = newSVpvn("\n",1);
	    numlen = 3 + (*s == '0');
	    *SvPVX(PL_ors_sv) = (char)grok_oct(s, &numlen, &flags, NULL);
	    s += numlen;
	}
	else {
	    if (RsPARA(PL_rs)) {
		PL_ors_sv = newSVpvn("\n\n",2);
	    }
	    else {
		PL_ors_sv = newSVsv(PL_rs);
	    }
	}
	return s;
    case 'M':
	forbid_setid("-M");	/* XXX ? */
	/* FALL THROUGH */
    case 'm':
	forbid_setid("-m");	/* XXX ? */
	if (*++s) {
	    char *start;
	    SV *sv;
	    char *use = "use ";
	    /* -M-foo == 'no foo'	*/
	    if (*s == '-') { use = "no "; ++s; }
	    sv = newSVpv(use,0);
	    start = s;
	    /* We allow -M'Module qw(Foo Bar)'	*/
	    while(isALNUM(*s) || *s==':') ++s;
	    if (*s != '=') {
		sv_catpv(sv, start);
		if (*(start-1) == 'm') {
		    if (*s != '\0')
			Perl_croak(aTHX_ "Can't use '%c' after -mname", *s);
		    sv_catpv( sv, " ()");
		}
	    } else {
                if (s == start)
                    Perl_croak(aTHX_ "Module name required with -%c option",
			       s[-1]);
		sv_catpvn(sv, start, s-start);
		sv_catpv(sv, " split(/,/,q");
		sv_catpvn(sv, "\0)", 1);        /* Use NUL as q//-delimiter. */
		sv_catpv(sv, ++s);
		sv_catpvn(sv,  "\0)", 2);
	    }
	    s += strlen(s);
	    if (!PL_preambleav)
		PL_preambleav = newAV();
	    av_push(PL_preambleav, sv);
	}
	else
	    Perl_croak(aTHX_ "Missing argument to -%c", *(s-1));
	return s;
    case 'n':
	PL_minus_n = TRUE;
	s++;
	return s;
    case 'p':
	PL_minus_p = TRUE;
	s++;
	return s;
    case 's':
	forbid_setid("-s");
	PL_doswitches = TRUE;
	s++;
	return s;
    case 't':
        if (!PL_tainting)
	    TOO_LATE_FOR('t');
        s++;
        return s;
    case 'T':
	if (!PL_tainting)
	    TOO_LATE_FOR('T');
	s++;
	return s;
    case 'u':
#ifdef MACOS_TRADITIONAL
	Perl_croak(aTHX_ "Believe me, you don't want to use \"-u\" on a Macintosh");
#endif
	PL_do_undump = TRUE;
	s++;
	return s;
    case 'U':
	PL_unsafe = TRUE;
	s++;
	return s;
    case 'v':
#if !defined(DGUX)
	PerlIO_printf(PerlIO_stdout(),
		      Perl_form(aTHX_ "\nThis is perl, v%"VDf" built for %s",
				PL_patchlevel, ARCHNAME));
#else /* DGUX */
/* Adjust verbose output as in the perl that ships with the DG/UX OS from EMC */
	PerlIO_printf(PerlIO_stdout(),
			Perl_form(aTHX_ "\nThis is perl, version %vd\n", PL_patchlevel));
	PerlIO_printf(PerlIO_stdout(),
			Perl_form(aTHX_ "        built under %s at %s %s\n",
					OSNAME, __DATE__, __TIME__));
	PerlIO_printf(PerlIO_stdout(),
			Perl_form(aTHX_ "        OS Specific Release: %s\n",
					OSVERS));
#endif /* !DGUX */

#if defined(LOCAL_PATCH_COUNT)
	if (LOCAL_PATCH_COUNT > 0)
	    PerlIO_printf(PerlIO_stdout(),
			  "\n(with %d registered patch%s, "
			  "see perl -V for more detail)",
			  (int)LOCAL_PATCH_COUNT,
			  (LOCAL_PATCH_COUNT!=1) ? "es" : "");
#endif

	PerlIO_printf(PerlIO_stdout(),
		      "\n\nCopyright 1987-2004, Larry Wall\n");
#ifdef MACOS_TRADITIONAL
	PerlIO_printf(PerlIO_stdout(),
		      "\nMac OS port Copyright 1991-2002, Matthias Neeracher;\n"
		      "maintained by Chris Nandor\n");
#endif
#ifdef MSDOS
	PerlIO_printf(PerlIO_stdout(),
		      "\nMS-DOS port Copyright (c) 1989, 1990, Diomidis Spinellis\n");
#endif
#ifdef DJGPP
	PerlIO_printf(PerlIO_stdout(),
		      "djgpp v2 port (jpl5003c) by Hirofumi Watanabe, 1996\n"
		      "djgpp v2 port (perl5004+) by Laszlo Molnar, 1997-1999\n");
#endif
#ifdef OS2
	PerlIO_printf(PerlIO_stdout(),
		      "\n\nOS/2 port Copyright (c) 1990, 1991, Raymond Chen, Kai Uwe Rommel\n"
		      "Version 5 port Copyright (c) 1994-2002, Andreas Kaiser, Ilya Zakharevich\n");
#endif
#ifdef atarist
	PerlIO_printf(PerlIO_stdout(),
		      "atariST series port, ++jrb  bammi@cadence.com\n");
#endif
#ifdef __BEOS__
	PerlIO_printf(PerlIO_stdout(),
		      "BeOS port Copyright Tom Spindler, 1997-1999\n");
#endif
#ifdef MPE
	PerlIO_printf(PerlIO_stdout(),
		      "MPE/iX port Copyright by Mark Klein and Mark Bixby, 1996-2003\n");
#endif
#ifdef OEMVS
	PerlIO_printf(PerlIO_stdout(),
		      "MVS (OS390) port by Mortice Kern Systems, 1997-1999\n");
#endif
#ifdef __VOS__
	PerlIO_printf(PerlIO_stdout(),
		      "Stratus VOS port by Paul.Green@stratus.com, 1997-2002\n");
#endif
#ifdef __OPEN_VM
	PerlIO_printf(PerlIO_stdout(),
		      "VM/ESA port by Neale Ferguson, 1998-1999\n");
#endif
#ifdef POSIX_BC
	PerlIO_printf(PerlIO_stdout(),
		      "BS2000 (POSIX) port by Start Amadeus GmbH, 1998-1999\n");
#endif
#ifdef __MINT__
	PerlIO_printf(PerlIO_stdout(),
		      "MiNT port by Guido Flohr, 1997-1999\n");
#endif
#ifdef EPOC
	PerlIO_printf(PerlIO_stdout(),
		      "EPOC port by Olaf Flebbe, 1999-2002\n");
#endif
#ifdef UNDER_CE
	PerlIO_printf(PerlIO_stdout(),"WINCE port by Rainer Keuchel, 2001-2002\n");
	PerlIO_printf(PerlIO_stdout(),"Built on " __DATE__ " " __TIME__ "\n\n");
	wce_hitreturn();
#endif
#ifdef BINARY_BUILD_NOTICE
	BINARY_BUILD_NOTICE;
#endif
	PerlIO_printf(PerlIO_stdout(),
		      "\n\
Perl may be copied only under the terms of either the Artistic License or the\n\
GNU General Public License, which may be found in the Perl 5 source kit.\n\n\
Complete documentation for Perl, including FAQ lists, should be found on\n\
this system using `man perl' or `perldoc perl'.  If you have access to the\n\
Internet, point your browser at http://www.perl.org/, the Perl Home Page.\n\n");
	my_exit(0);
    case 'w':
	if (! (PL_dowarn & G_WARN_ALL_MASK))
	    PL_dowarn |= G_WARN_ON;
	s++;
	return s;
    case 'W':
	PL_dowarn = G_WARN_ALL_ON|G_WARN_ON;
        if (!specialWARN(PL_compiling.cop_warnings))
            SvREFCNT_dec(PL_compiling.cop_warnings);
	PL_compiling.cop_warnings = pWARN_ALL ;
	s++;
	return s;
    case 'X':
	PL_dowarn = G_WARN_ALL_OFF;
        if (!specialWARN(PL_compiling.cop_warnings))
            SvREFCNT_dec(PL_compiling.cop_warnings);
	PL_compiling.cop_warnings = pWARN_NONE ;
	s++;
	return s;
    case '*':
    case ' ':
	if (s[1] == '-')	/* Additional switches on #! line. */
	    return s+2;
	break;
    case '-':
    case 0:
#if defined(WIN32) || !defined(PERL_STRICT_CR)
    case '\r':
#endif
    case '\n':
    case '\t':
	break;
#ifdef ALTERNATE_SHEBANG
    case 'S':			/* OS/2 needs -S on "extproc" line. */
	break;
#endif
    case 'P':
	if (PL_preprocess)
	    return s+1;
	/* FALL THROUGH */
    default:
	Perl_croak(aTHX_ "Can't emulate -%.1s on #! line",s);
    }
    return Nullch;
}

/* compliments of Tom Christiansen */

/* unexec() can be found in the Gnu emacs distribution */
/* Known to work with -DUNEXEC and using unexelf.c from GNU emacs-20.2 */

void
Perl_my_unexec(pTHX)
{
#ifdef UNEXEC
    SV*    prog;
    SV*    file;
    int    status = 1;
    extern int etext;

    prog = newSVpv(BIN_EXP, 0);
    sv_catpv(prog, "/perl");
    file = newSVpv(PL_origfilename, 0);
    sv_catpv(file, ".perldump");

    unexec(SvPVX(file), SvPVX(prog), &etext, sbrk(0), 0);
    /* unexec prints msg to stderr in case of failure */
    PerlProc_exit(status);
#else
#  ifdef VMS
#    include <lib$routines.h>
     lib$signal(SS$_DEBUG);  /* ssdef.h #included from vmsish.h */
#  else
    ABORT();		/* for use with undump */
#  endif
#endif
}

/* initialize curinterp */
STATIC void
S_init_interp(pTHX)
{

#ifdef MULTIPLICITY
#  define PERLVAR(var,type)
#  define PERLVARA(var,n,type)
#  if defined(PERL_IMPLICIT_CONTEXT)
#    if defined(USE_5005THREADS)
#      define PERLVARI(var,type,init)		PERL_GET_INTERP->var = init;
#      define PERLVARIC(var,type,init)	PERL_GET_INTERP->var = init;
#    else /* !USE_5005THREADS */
#      define PERLVARI(var,type,init)		aTHX->var = init;
#      define PERLVARIC(var,type,init)	aTHX->var = init;
#    endif /* USE_5005THREADS */
#  else
#    define PERLVARI(var,type,init)	PERL_GET_INTERP->var = init;
#    define PERLVARIC(var,type,init)	PERL_GET_INTERP->var = init;
#  endif
#  include "intrpvar.h"
#  ifndef USE_5005THREADS
#    include "thrdvar.h"
#  endif
#  undef PERLVAR
#  undef PERLVARA
#  undef PERLVARI
#  undef PERLVARIC
#else
#  define PERLVAR(var,type)
#  define PERLVARA(var,n,type)
#  define PERLVARI(var,type,init)	PL_##var = init;
#  define PERLVARIC(var,type,init)	PL_##var = init;
#  include "intrpvar.h"
#  ifndef USE_5005THREADS
#    include "thrdvar.h"
#  endif
#  undef PERLVAR
#  undef PERLVARA
#  undef PERLVARI
#  undef PERLVARIC
#endif

}

STATIC void
S_init_main_stash(pTHX)
{
    GV *gv;

    PL_curstash = PL_defstash = newHV();
    PL_curstname = newSVpvn("main",4);
    gv = gv_fetchpv("main::",TRUE, SVt_PVHV);
    SvREFCNT_dec(GvHV(gv));
    GvHV(gv) = (HV*)SvREFCNT_inc(PL_defstash);
    SvREADONLY_on(gv);
    HvNAME(PL_defstash) = savepv("main");
    PL_incgv = gv_HVadd(gv_AVadd(gv_fetchpv("INC",TRUE, SVt_PVAV)));
    GvMULTI_on(PL_incgv);
    PL_hintgv = gv_fetchpv("\010",TRUE, SVt_PV); /* ^H */
    GvMULTI_on(PL_hintgv);
    PL_defgv = gv_fetchpv("_",TRUE, SVt_PVAV);
    PL_errgv = gv_HVadd(gv_fetchpv("@", TRUE, SVt_PV));
    GvMULTI_on(PL_errgv);
    PL_replgv = gv_fetchpv("\022", TRUE, SVt_PV); /* ^R */
    GvMULTI_on(PL_replgv);
    (void)Perl_form(aTHX_ "%240s","");	/* Preallocate temp - for immediate signals. */
    sv_grow(ERRSV, 240);	/* Preallocate - for immediate signals. */
    sv_setpvn(ERRSV, "", 0);
    PL_curstash = PL_defstash;
    CopSTASH_set(&PL_compiling, PL_defstash);
    PL_debstash = GvHV(gv_fetchpv("DB::", GV_ADDMULTI, SVt_PVHV));
    PL_globalstash = GvHV(gv_fetchpv("CORE::GLOBAL::", GV_ADDMULTI, SVt_PVHV));
    PL_nullstash = GvHV(gv_fetchpv("<none>::", GV_ADDMULTI, SVt_PVHV));
    /* We must init $/ before switches are processed. */
    sv_setpvn(get_sv("/", TRUE), "\n", 1);
}

/* PSz 18 Nov 03  fdscript now global but do not change prototype */
STATIC void
S_open_script(pTHX_ char *scriptname, bool dosearch, SV *sv)
{
#ifndef IAMSUID
    char *quote;
    char *code;
    char *cpp_discard_flag;
    char *perl;
#endif

    PL_fdscript = -1;
    PL_suidscript = -1;

    if (PL_e_script) {
	PL_origfilename = savepv("-e");
    }
    else {
	/* if find_script() returns, it returns a malloc()-ed value */
	PL_origfilename = scriptname = find_script(scriptname, dosearch, NULL, 1);

	if (strnEQ(scriptname, "/dev/fd/", 8) && isDIGIT(scriptname[8]) ) {
	    char *s = scriptname + 8;
	    PL_fdscript = atoi(s);
	    while (isDIGIT(*s))
		s++;
	    if (*s) {
		/* PSz 18 Feb 04
		 * Tell apart "normal" usage of fdscript, e.g.
		 * with bash on FreeBSD:
		 *   perl <( echo '#!perl -DA'; echo 'print "$0\n"')
		 * from usage in suidperl.
		 * Does any "normal" usage leave garbage after the number???
		 * Is it a mistake to use a similar /dev/fd/ construct for
		 * suidperl?
		 */
		PL_suidscript = 1;
		/* PSz 20 Feb 04  
		 * Be supersafe and do some sanity-checks.
		 * Still, can we be sure we got the right thing?
		 */
		if (*s != '/') {
		    Perl_croak(aTHX_ "Wrong syntax (suid) fd script name \"%s\"\n", s);
		}
		if (! *(s+1)) {
		    Perl_croak(aTHX_ "Missing (suid) fd script name\n");
		}
		scriptname = savepv(s + 1);
		Safefree(PL_origfilename);
		PL_origfilename = scriptname;
	    }
	}
    }

    CopFILE_free(PL_curcop);
    CopFILE_set(PL_curcop, PL_origfilename);
    if (strEQ(PL_origfilename,"-"))
	scriptname = "";
    if (PL_fdscript >= 0) {
	PL_rsfp = PerlIO_fdopen(PL_fdscript,PERL_SCRIPT_MODE);
#       if defined(HAS_FCNTL) && defined(F_SETFD)
	    if (PL_rsfp)
                /* ensure close-on-exec */
	        fcntl(PerlIO_fileno(PL_rsfp),F_SETFD,1);
#       endif
    }
#ifdef IAMSUID
    else {
	Perl_croak(aTHX_ "sperl needs fd script\n"
		   "You should not call sperl directly; do you need to "
		   "change a #! line\nfrom sperl to perl?\n");

/* PSz 11 Nov 03
 * Do not open (or do other fancy stuff) while setuid.
 * Perl does the open, and hands script to suidperl on a fd;
 * suidperl only does some checks, sets up UIDs and re-execs
 * perl with that fd as it has always done.
 */
    }
    if (PL_suidscript != 1) {
	Perl_croak(aTHX_ "suidperl needs (suid) fd script\n");
    }
#else /* IAMSUID */
    else if (PL_preprocess) {
	char *cpp_cfg = CPPSTDIN;
	SV *cpp = newSVpvn("",0);
	SV *cmd = NEWSV(0,0);

	if (cpp_cfg[0] == 0) /* PERL_MICRO? */
	     Perl_croak(aTHX_ "Can't run with cpp -P with CPPSTDIN undefined");
	if (strEQ(cpp_cfg, "cppstdin"))
	    Perl_sv_catpvf(aTHX_ cpp, "%s/", BIN_EXP);
	sv_catpv(cpp, cpp_cfg);

#       ifndef VMS
	    sv_catpvn(sv, "-I", 2);
	    sv_catpv(sv,PRIVLIB_EXP);
#       endif

	DEBUG_P(PerlIO_printf(Perl_debug_log,
			      "PL_preprocess: scriptname=\"%s\", cpp=\"%s\", sv=\"%s\", CPPMINUS=\"%s\"\n",
			      scriptname, SvPVX (cpp), SvPVX (sv), CPPMINUS));

#       if defined(MSDOS) || defined(WIN32) || defined(VMS)
            quote = "\"";
#       else
            quote = "'";
#       endif

#       ifdef VMS
            cpp_discard_flag = "";
#       else
            cpp_discard_flag = "-C";
#       endif

#       ifdef OS2
            perl = os2_execname(aTHX);
#       else
            perl = PL_origargv[0];
#       endif


        /* This strips off Perl comments which might interfere with
           the C pre-processor, including #!.  #line directives are
           deliberately stripped to avoid confusion with Perl's version
           of #line.  FWP played some golf with it so it will fit
           into VMS's 255 character buffer.
        */
        if( PL_doextract )
            code = "(1../^#!.*perl/i)|/^\\s*#(?!\\s*((ifn?|un)def|(el|end)?if|define|include|else|error|pragma)\\b)/||!($|=1)||print";
        else
            code = "/^\\s*#(?!\\s*((ifn?|un)def|(el|end)?if|define|include|else|error|pragma)\\b)/||!($|=1)||print";

        Perl_sv_setpvf(aTHX_ cmd, "\
%s -ne%s%s%s %s | %"SVf" %s %"SVf" %s",
                       perl, quote, code, quote, scriptname, cpp,
                       cpp_discard_flag, sv, CPPMINUS);

	PL_doextract = FALSE;

        DEBUG_P(PerlIO_printf(Perl_debug_log,
                              "PL_preprocess: cmd=\"%s\"\n",
                              SvPVX(cmd)));

	PL_rsfp = PerlProc_popen(SvPVX(cmd), "r");
	SvREFCNT_dec(cmd);
	SvREFCNT_dec(cpp);
    }
    else if (!*scriptname) {
	forbid_setid("program input from stdin");
	PL_rsfp = PerlIO_stdin();
    }
    else {
	PL_rsfp = PerlIO_open(scriptname,PERL_SCRIPT_MODE);
#       if defined(HAS_FCNTL) && defined(F_SETFD)
	    if (PL_rsfp)
                /* ensure close-on-exec */
	        fcntl(PerlIO_fileno(PL_rsfp),F_SETFD,1);
#       endif
    }
#endif /* IAMSUID */
    if (!PL_rsfp) {
	/* PSz 16 Sep 03  Keep neat error message */
	Perl_croak(aTHX_ "Can't open perl script \"%s\": %s\n",
		CopFILE(PL_curcop), Strerror(errno));
    }
}

/* Mention
 * I_SYSSTATVFS	HAS_FSTATVFS
 * I_SYSMOUNT
 * I_STATFS	HAS_FSTATFS	HAS_GETFSSTAT
 * I_MNTENT	HAS_GETMNTENT	HAS_HASMNTOPT
 * here so that metaconfig picks them up. */

#ifdef IAMSUID
STATIC int
S_fd_on_nosuid_fs(pTHX_ int fd)
{
/* PSz 27 Feb 04
 * We used to do this as "plain" user (after swapping UIDs with setreuid);
 * but is needed also on machines without setreuid.
 * Seems safe enough to run as root.
 */
    int check_okay = 0; /* able to do all the required sys/libcalls */
    int on_nosuid  = 0; /* the fd is on a nosuid fs */
    /* PSz 12 Nov 03
     * Need to check noexec also: nosuid might not be set, the average
     * sysadmin would say that nosuid is irrelevant once he sets noexec.
     */
    int on_noexec  = 0; /* the fd is on a noexec fs */

/*
 * Preferred order: fstatvfs(), fstatfs(), ustat()+getmnt(), getmntent().
 * fstatvfs() is UNIX98.
 * fstatfs() is 4.3 BSD.
 * ustat()+getmnt() is pre-4.3 BSD.
 * getmntent() is O(number-of-mounted-filesystems) and can hang on
 * an irrelevant filesystem while trying to reach the right one.
 */

#undef FD_ON_NOSUID_CHECK_OKAY  /* found the syscalls to do the check? */

#   if !defined(FD_ON_NOSUID_CHECK_OKAY) && \
        defined(HAS_FSTATVFS)
#   define FD_ON_NOSUID_CHECK_OKAY
    struct statvfs stfs;

    check_okay = fstatvfs(fd, &stfs) == 0;
    on_nosuid  = check_okay && (stfs.f_flag  & ST_NOSUID);
#ifdef ST_NOEXEC
    /* ST_NOEXEC certainly absent on AIX 5.1, and doesn't seem to be documented
       on platforms where it is present.  */
    on_noexec  = check_okay && (stfs.f_flag  & ST_NOEXEC);
#endif
#   endif /* fstatvfs */

#   if !defined(FD_ON_NOSUID_CHECK_OKAY) && \
        defined(PERL_MOUNT_NOSUID)	&& \
        defined(PERL_MOUNT_NOEXEC)	&& \
        defined(HAS_FSTATFS) 		&& \
        defined(HAS_STRUCT_STATFS)	&& \
        defined(HAS_STRUCT_STATFS_F_FLAGS)
#   define FD_ON_NOSUID_CHECK_OKAY
    struct statfs  stfs;

    check_okay = fstatfs(fd, &stfs)  == 0;
    on_nosuid  = check_okay && (stfs.f_flags & PERL_MOUNT_NOSUID);
    on_noexec  = check_okay && (stfs.f_flags & PERL_MOUNT_NOEXEC);
#   endif /* fstatfs */

#   if !defined(FD_ON_NOSUID_CHECK_OKAY) && \
        defined(PERL_MOUNT_NOSUID)	&& \
        defined(PERL_MOUNT_NOEXEC)	&& \
        defined(HAS_FSTAT)		&& \
        defined(HAS_USTAT)		&& \
        defined(HAS_GETMNT)		&& \
        defined(HAS_STRUCT_FS_DATA)	&& \
        defined(NOSTAT_ONE)
#   define FD_ON_NOSUID_CHECK_OKAY
    Stat_t fdst;

    if (fstat(fd, &fdst) == 0) {
        struct ustat us;
        if (ustat(fdst.st_dev, &us) == 0) {
            struct fs_data fsd;
            /* NOSTAT_ONE here because we're not examining fields which
             * vary between that case and STAT_ONE. */
            if (getmnt((int*)0, &fsd, (int)0, NOSTAT_ONE, us.f_fname) == 0) {
                size_t cmplen = sizeof(us.f_fname);
                if (sizeof(fsd.fd_req.path) < cmplen)
                    cmplen = sizeof(fsd.fd_req.path);
                if (strnEQ(fsd.fd_req.path, us.f_fname, cmplen) &&
                    fdst.st_dev == fsd.fd_req.dev) {
                        check_okay = 1;
                        on_nosuid = fsd.fd_req.flags & PERL_MOUNT_NOSUID;
                        on_noexec = fsd.fd_req.flags & PERL_MOUNT_NOEXEC;
                    }
                }
            }
        }
    }
#   endif /* fstat+ustat+getmnt */

#   if !defined(FD_ON_NOSUID_CHECK_OKAY) && \
        defined(HAS_GETMNTENT)		&& \
        defined(HAS_HASMNTOPT)		&& \
        defined(MNTOPT_NOSUID)		&& \
        defined(MNTOPT_NOEXEC)
#   define FD_ON_NOSUID_CHECK_OKAY
    FILE                *mtab = fopen("/etc/mtab", "r");
    struct mntent       *entry;
    Stat_t              stb, fsb;

    if (mtab && (fstat(fd, &stb) == 0)) {
        while (entry = getmntent(mtab)) {
            if (stat(entry->mnt_dir, &fsb) == 0
                && fsb.st_dev == stb.st_dev)
            {
                /* found the filesystem */
                check_okay = 1;
                if (hasmntopt(entry, MNTOPT_NOSUID))
                    on_nosuid = 1;
                if (hasmntopt(entry, MNTOPT_NOEXEC))
                    on_noexec = 1;
                break;
            } /* A single fs may well fail its stat(). */
        }
    }
    if (mtab)
        fclose(mtab);
#   endif /* getmntent+hasmntopt */

    if (!check_okay)
	Perl_croak(aTHX_ "Can't check filesystem of script \"%s\" for nosuid/noexec", PL_origfilename);
    if (on_nosuid)
	Perl_croak(aTHX_ "Setuid script \"%s\" on nosuid filesystem", PL_origfilename);
    if (on_noexec)
	Perl_croak(aTHX_ "Setuid script \"%s\" on noexec filesystem", PL_origfilename);
    return ((!check_okay) || on_nosuid || on_noexec);
}
#endif /* IAMSUID */

STATIC void
S_validate_suid(pTHX_ char *validarg, char *scriptname)
{
#ifdef IAMSUID
    /* int which; */
#endif /* IAMSUID */

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
     * PSz 27 Feb 04
     * Description/comments above do not match current workings:
     *   suidperl must be hardlinked to sperlN.NNN (that is what we exec);
     *   suidperl called with script open and name changed to /dev/fd/N/X;
     *   suidperl croaks if script is not setuid;
     *   making perl setuid would be a huge security risk (and yes, that
     *     would lose any euid we might have had).
     *
     * DOSUID must be defined in both perl and suidperl, and IAMSUID must
     * be defined in suidperl only.  suidperl must be setuid root.  The
     * Configure script will set this up for you if you want it.
     */

#ifdef DOSUID
    char *s, *s2;

    if (PerlLIO_fstat(PerlIO_fileno(PL_rsfp),&PL_statbuf) < 0)	/* normal stat is insecure */
	Perl_croak(aTHX_ "Can't stat script \"%s\"",PL_origfilename);
    if (PL_statbuf.st_mode & (S_ISUID|S_ISGID)) {
	I32 len;
	STRLEN n_a;

#ifdef IAMSUID
	if (PL_fdscript < 0 || PL_suidscript != 1)
	    Perl_croak(aTHX_ "Need (suid) fdscript in suidperl\n");	/* We already checked this */
	/* PSz 11 Nov 03
	 * Since the script is opened by perl, not suidperl, some of these
	 * checks are superfluous. Leaving them in probably does not lower
	 * security(?!).
	 */
	/* PSz 27 Feb 04
	 * Do checks even for systems with no HAS_SETREUID.
	 * We used to swap, then re-swap UIDs with
#ifdef HAS_SETREUID
	    if (setreuid(PL_euid,PL_uid) < 0
		|| PerlProc_getuid() != PL_euid || PerlProc_geteuid() != PL_uid)
		Perl_croak(aTHX_ "Can't swap uid and euid");
#endif
#ifdef HAS_SETREUID
	    if (setreuid(PL_uid,PL_euid) < 0
		|| PerlProc_getuid() != PL_uid || PerlProc_geteuid() != PL_euid)
		Perl_croak(aTHX_ "Can't reswap uid and euid");
#endif
	 */

	/* On this access check to make sure the directories are readable,
	 * there is actually a small window that the user could use to make
	 * filename point to an accessible directory.  So there is a faint
	 * chance that someone could execute a setuid script down in a
	 * non-accessible directory.  I don't know what to do about that.
	 * But I don't think it's too important.  The manual lies when
	 * it says access() is useful in setuid programs.
	 * 
	 * So, access() is pretty useless... but not harmful... do anyway.
	 */
	if (PerlLIO_access(CopFILE(PL_curcop),1)) { /*double check*/
	    Perl_croak(aTHX_ "Can't access() script\n");
	}

	/* If we can swap euid and uid, then we can determine access rights
	 * with a simple stat of the file, and then compare device and
	 * inode to make sure we did stat() on the same file we opened.
	 * Then we just have to make sure he or she can execute it.
	 * 
	 * PSz 24 Feb 04
	 * As the script is opened by perl, not suidperl, we do not need to
	 * care much about access rights.
	 * 
	 * The 'script changed' check is needed, or we can get lied to
	 * about $0 with e.g.
	 *  suidperl /dev/fd/4//bin/x 4<setuidscript
	 * Without HAS_SETREUID, is it safe to stat() as root?
	 * 
	 * Are there any operating systems that pass /dev/fd/xxx for setuid
	 * scripts, as suggested/described in perlsec(1)? Surely they do not
	 * pass the script name as we do, so the "script changed" test would
	 * fail for them... but we never get here with
	 * SETUID_SCRIPTS_ARE_SECURE_NOW defined.
	 * 
	 * This is one place where we must "lie" about return status: not
	 * say if the stat() failed. We are doing this as root, and could
	 * be tricked into reporting existence or not of files that the
	 * "plain" user cannot even see.
	 */
	{
	    Stat_t tmpstatbuf;
	    if (PerlLIO_stat(CopFILE(PL_curcop),&tmpstatbuf) < 0 ||
		tmpstatbuf.st_dev != PL_statbuf.st_dev ||
		tmpstatbuf.st_ino != PL_statbuf.st_ino) {
		Perl_croak(aTHX_ "Setuid script changed\n");
	    }

	}
	if (!cando(S_IXUSR,FALSE,&PL_statbuf))		/* can real uid exec? */
	    Perl_croak(aTHX_ "Real UID cannot exec script\n");

	/* PSz 27 Feb 04
	 * We used to do this check as the "plain" user (after swapping
	 * UIDs). But the check for nosuid and noexec filesystem is needed,
	 * and should be done even without HAS_SETREUID. (Maybe those
	 * operating systems do not have such mount options anyway...)
	 * Seems safe enough to do as root.
	 */
#if !defined(NO_NOSUID_CHECK)
	if (fd_on_nosuid_fs(PerlIO_fileno(PL_rsfp))) {
	    Perl_croak(aTHX_ "Setuid script on nosuid or noexec filesystem\n");
	}
#endif
#endif /* IAMSUID */

	if (!S_ISREG(PL_statbuf.st_mode)) {
	    Perl_croak(aTHX_ "Setuid script not plain file\n");
	}
	if (PL_statbuf.st_mode & S_IWOTH)
	    Perl_croak(aTHX_ "Setuid/gid script is writable by world");
	PL_doswitches = FALSE;		/* -s is insecure in suid */
	/* PSz 13 Nov 03  But -s was caught elsewhere ... so unsetting it here is useless(?!) */
	CopLINE_inc(PL_curcop);
	if (sv_gets(PL_linestr, PL_rsfp, 0) == Nullch ||
	  strnNE(SvPV(PL_linestr,n_a),"#!",2) )	/* required even on Sys V */
	    Perl_croak(aTHX_ "No #! line");
	s = SvPV(PL_linestr,n_a)+2;
	/* PSz 27 Feb 04 */
	/* Sanity check on line length */
	if (strlen(s) < 1 || strlen(s) > 4000)
	    Perl_croak(aTHX_ "Very long #! line");
	/* Allow more than a single space after #! */
	while (isSPACE(*s)) s++;
	/* Sanity check on buffer end */
	while ((*s) && !isSPACE(*s)) s++;
	for (s2 = s;  (s2 > SvPV(PL_linestr,n_a)+2 &&
		       (isDIGIT(s2[-1]) || strchr("._-", s2[-1])));  s2--) ;
	/* Sanity check on buffer start */
	if ( (s2-4 < SvPV(PL_linestr,n_a)+2 || strnNE(s2-4,"perl",4)) &&
	      (s-9 < SvPV(PL_linestr,n_a)+2 || strnNE(s-9,"perl",4)) )
	    Perl_croak(aTHX_ "Not a perl script");
	while (*s == ' ' || *s == '\t') s++;
	/*
	 * #! arg must be what we saw above.  They can invoke it by
	 * mentioning suidperl explicitly, but they may not add any strange
	 * arguments beyond what #! says if they do invoke suidperl that way.
	 */
	/*
	 * The way validarg was set up, we rely on the kernel to start
	 * scripts with argv[1] set to contain all #! line switches (the
	 * whole line).
	 */
	/*
	 * Check that we got all the arguments listed in the #! line (not
	 * just that there are no extraneous arguments). Might not matter
	 * much, as switches from #! line seem to be acted upon (also), and
	 * so may be checked and trapped in perl. But, security checks must
	 * be done in suidperl and not deferred to perl. Note that suidperl
	 * does not get around to parsing (and checking) the switches on
	 * the #! line (but execs perl sooner).
	 * Allow (require) a trailing newline (which may be of two
	 * characters on some architectures?) (but no other trailing
	 * whitespace).
	 */
	len = strlen(validarg);
	if (strEQ(validarg," PHOOEY ") ||
	    strnNE(s,validarg,len) || !isSPACE(s[len]) ||
	    !(strlen(s) == len+1 || (strlen(s) == len+2 && isSPACE(s[len+1]))))
	    Perl_croak(aTHX_ "Args must match #! line");

#ifndef IAMSUID
	if (PL_fdscript < 0 &&
	    PL_euid != PL_uid && (PL_statbuf.st_mode & S_ISUID) &&
	    PL_euid == PL_statbuf.st_uid)
	    if (!PL_do_undump)
		Perl_croak(aTHX_ "YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, OR PUT A C WRAPPER AROUND THIS SCRIPT!\n");
#endif /* IAMSUID */

	if (PL_fdscript < 0 &&
	    PL_euid) {	/* oops, we're not the setuid root perl */
	    /* PSz 18 Feb 04
	     * When root runs a setuid script, we do not go through the same
	     * steps of execing sperl and then perl with fd scripts, but
	     * simply set up UIDs within the same perl invocation; so do
	     * not have the same checks (on options, whatever) that we have
	     * for plain users. No problem really: would have to be a script
	     * that does not actually work for plain users; and if root is
	     * foolish and can be persuaded to run such an unsafe script, he
	     * might run also non-setuid ones, and deserves what he gets.
	     * 
	     * Or, we might drop the PL_euid check above (and rely just on
	     * PL_fdscript to avoid loops), and do the execs
	     * even for root.
	     */
#ifndef IAMSUID
	    int which;
	    /* PSz 11 Nov 03
	     * Pass fd script to suidperl.
	     * Exec suidperl, substituting fd script for scriptname.
	     * Pass script name as "subdir" of fd, which perl will grok;
	     * in fact will use that to distinguish this from "normal"
	     * usage, see comments above.
	     */
	    PerlIO_rewind(PL_rsfp);
	    PerlLIO_lseek(PerlIO_fileno(PL_rsfp),(Off_t)0,0);  /* just in case rewind didn't */
	    /* PSz 27 Feb 04  Sanity checks on scriptname */
	    if ((!scriptname) || (!*scriptname) ) {
		Perl_croak(aTHX_ "No setuid script name\n");
	    }
	    if (*scriptname == '-') {
		Perl_croak(aTHX_ "Setuid script name may not begin with dash\n");
		/* Or we might confuse it with an option when replacing
		 * name in argument list, below (though we do pointer, not
		 * string, comparisons).
		 */
	    }
	    for (which = 1; PL_origargv[which] && PL_origargv[which] != scriptname; which++) ;
	    if (!PL_origargv[which]) {
		Perl_croak(aTHX_ "Can't change argv to have fd script\n");
	    }
	    PL_origargv[which] = savepv(Perl_form(aTHX_ "/dev/fd/%d/%s",
					  PerlIO_fileno(PL_rsfp), PL_origargv[which]));
#if defined(HAS_FCNTL) && defined(F_SETFD)
	    fcntl(PerlIO_fileno(PL_rsfp),F_SETFD,0);	/* ensure no close-on-exec */
#endif
	    PERL_FPU_PRE_EXEC
	    PerlProc_execv(Perl_form(aTHX_ "%s/sperl"PERL_FS_VER_FMT, BIN_EXP,
				     (int)PERL_REVISION, (int)PERL_VERSION,
				     (int)PERL_SUBVERSION), PL_origargv);
	    PERL_FPU_POST_EXEC
#endif /* IAMSUID */
	    Perl_croak(aTHX_ "Can't do setuid (cannot exec sperl)\n");
	}

	if (PL_statbuf.st_mode & S_ISGID && PL_statbuf.st_gid != PL_egid) {
/* PSz 26 Feb 04
 * This seems back to front: we try HAS_SETEGID first; if not available
 * then try HAS_SETREGID; as a last chance we try HAS_SETRESGID. May be OK
 * in the sense that we only want to set EGID; but are there any machines
 * with either of the latter, but not the former? Same with UID, later.
 */
#ifdef HAS_SETEGID
	    (void)setegid(PL_statbuf.st_gid);
#else
#ifdef HAS_SETREGID
           (void)setregid((Gid_t)-1,PL_statbuf.st_gid);
#else
#ifdef HAS_SETRESGID
           (void)setresgid((Gid_t)-1,PL_statbuf.st_gid,(Gid_t)-1);
#else
	    PerlProc_setgid(PL_statbuf.st_gid);
#endif
#endif
#endif
	    if (PerlProc_getegid() != PL_statbuf.st_gid)
		Perl_croak(aTHX_ "Can't do setegid!\n");
	}
	if (PL_statbuf.st_mode & S_ISUID) {
	    if (PL_statbuf.st_uid != PL_euid)
#ifdef HAS_SETEUID
		(void)seteuid(PL_statbuf.st_uid);	/* all that for this */
#else
#ifdef HAS_SETREUID
                (void)setreuid((Uid_t)-1,PL_statbuf.st_uid);
#else
#ifdef HAS_SETRESUID
                (void)setresuid((Uid_t)-1,PL_statbuf.st_uid,(Uid_t)-1);
#else
		PerlProc_setuid(PL_statbuf.st_uid);
#endif
#endif
#endif
	    if (PerlProc_geteuid() != PL_statbuf.st_uid)
		Perl_croak(aTHX_ "Can't do seteuid!\n");
	}
	else if (PL_uid) {			/* oops, mustn't run as root */
#ifdef HAS_SETEUID
          (void)seteuid((Uid_t)PL_uid);
#else
#ifdef HAS_SETREUID
          (void)setreuid((Uid_t)-1,(Uid_t)PL_uid);
#else
#ifdef HAS_SETRESUID
          (void)setresuid((Uid_t)-1,(Uid_t)PL_uid,(Uid_t)-1);
#else
          PerlProc_setuid((Uid_t)PL_uid);
#endif
#endif
#endif
	    if (PerlProc_geteuid() != PL_uid)
		Perl_croak(aTHX_ "Can't do seteuid!\n");
	}
	init_ids();
	if (!cando(S_IXUSR,TRUE,&PL_statbuf))
	    Perl_croak(aTHX_ "Effective UID cannot exec script\n");	/* they can't do this */
    }
#ifdef IAMSUID
    else if (PL_preprocess)	/* PSz 13 Nov 03  Caught elsewhere, useless(?!) here */
	Perl_croak(aTHX_ "-P not allowed for setuid/setgid script\n");
    else if (PL_fdscript < 0 || PL_suidscript != 1)
	/* PSz 13 Nov 03  Caught elsewhere, useless(?!) here */
	Perl_croak(aTHX_ "(suid) fdscript needed in suidperl\n");
    else {
/* PSz 16 Sep 03  Keep neat error message */
	Perl_croak(aTHX_ "Script is not setuid/setgid in suidperl\n");
    }

    /* We absolutely must clear out any saved ids here, so we */
    /* exec the real perl, substituting fd script for scriptname. */
    /* (We pass script name as "subdir" of fd, which perl will grok.) */
    /* 
     * It might be thought that using setresgid and/or setresuid (changed to
     * set the saved IDs) above might obviate the need to exec, and we could
     * go on to "do the perl thing".
     * 
     * Is there such a thing as "saved GID", and is that set for setuid (but
     * not setgid) execution like suidperl? Without exec, it would not be
     * cleared for setuid (but not setgid) scripts (or might need a dummy
     * setresgid).
     * 
     * We need suidperl to do the exact same argument checking that perl
     * does. Thus it cannot be very small; while it could be significantly
     * smaller, it is safer (simpler?) to make it essentially the same
     * binary as perl (but they are not identical). - Maybe could defer that
     * check to the invoked perl, and suidperl be a tiny wrapper instead;
     * but prefer to do thorough checks in suidperl itself. Such deferral
     * would make suidperl security rely on perl, a design no-no.
     * 
     * Setuid things should be short and simple, thus easy to understand and
     * verify. They should do their "own thing", without influence by
     * attackers. It may help if their internal execution flow is fixed,
     * regardless of platform: it may be best to exec anyway.
     * 
     * Suidperl should at least be conceptually simple: a wrapper only,
     * never to do any real perl. Maybe we should put
     * #ifdef IAMSUID
     *         Perl_croak(aTHX_ "Suidperl should never do real perl\n");
     * #endif
     * into the perly bits.
     */
    PerlIO_rewind(PL_rsfp);
    PerlLIO_lseek(PerlIO_fileno(PL_rsfp),(Off_t)0,0);  /* just in case rewind didn't */
    /* PSz 11 Nov 03
     * Keep original arguments: suidperl already has fd script.
     */
/*  for (which = 1; PL_origargv[which] && PL_origargv[which] != scriptname; which++) ;	*/
/*  if (!PL_origargv[which]) {						*/
/*	errno = EPERM;							*/
/*	Perl_croak(aTHX_ "Permission denied\n");			*/
/*  }									*/
/*  PL_origargv[which] = savepv(Perl_form(aTHX_ "/dev/fd/%d/%s",	*/
/*				  PerlIO_fileno(PL_rsfp), PL_origargv[which]));	*/
#if defined(HAS_FCNTL) && defined(F_SETFD)
    fcntl(PerlIO_fileno(PL_rsfp),F_SETFD,0);	/* ensure no close-on-exec */
#endif
    PERL_FPU_PRE_EXEC
    PerlProc_execv(Perl_form(aTHX_ "%s/perl"PERL_FS_VER_FMT, BIN_EXP,
			     (int)PERL_REVISION, (int)PERL_VERSION,
			     (int)PERL_SUBVERSION), PL_origargv);/* try again */
    PERL_FPU_POST_EXEC
    Perl_croak(aTHX_ "Can't do setuid (suidperl cannot exec perl)\n");
#endif /* IAMSUID */
#else /* !DOSUID */
    if (PL_euid != PL_uid || PL_egid != PL_gid) {	/* (suidperl doesn't exist, in fact) */
#ifndef SETUID_SCRIPTS_ARE_SECURE_NOW
	PerlLIO_fstat(PerlIO_fileno(PL_rsfp),&PL_statbuf);	/* may be either wrapped or real suid */
	if ((PL_euid != PL_uid && PL_euid == PL_statbuf.st_uid && PL_statbuf.st_mode & S_ISUID)
	    ||
	    (PL_egid != PL_gid && PL_egid == PL_statbuf.st_gid && PL_statbuf.st_mode & S_ISGID)
	   )
	    if (!PL_do_undump)
		Perl_croak(aTHX_ "YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, PUT A C WRAPPER AROUND THIS SCRIPT, OR USE -u AND UNDUMP!\n");
#endif /* SETUID_SCRIPTS_ARE_SECURE_NOW */
	/* not set-id, must be wrapped */
    }
#endif /* DOSUID */
}

STATIC void
S_find_beginning(pTHX)
{
    register char *s, *s2;
#ifdef MACOS_TRADITIONAL
    int maclines = 0;
#endif

    /* skip forward in input to the real script? */

    forbid_setid("-x");
#ifdef MACOS_TRADITIONAL
    /* Since the Mac OS does not honor #! arguments for us, we do it ourselves */

    while (PL_doextract || gMacPerl_AlwaysExtract) {
	if ((s = sv_gets(PL_linestr, PL_rsfp, 0)) == Nullch) {
	    if (!gMacPerl_AlwaysExtract)
		Perl_croak(aTHX_ "No Perl script found in input\n");

	    if (PL_doextract)			/* require explicit override ? */
		if (!OverrideExtract(PL_origfilename))
		    Perl_croak(aTHX_ "User aborted script\n");
		else
		    PL_doextract = FALSE;

	    /* Pater peccavi, file does not have #! */
	    PerlIO_rewind(PL_rsfp);

	    break;
	}
#else
    while (PL_doextract) {
	if ((s = sv_gets(PL_linestr, PL_rsfp, 0)) == Nullch)
	    Perl_croak(aTHX_ "No Perl script found in input\n");
#endif
	s2 = s;
	if (*s == '#' && s[1] == '!' && ((s = instr(s,"perl")) || (s = instr(s2,"PERL")))) {
	    PerlIO_ungetc(PL_rsfp, '\n');		/* to keep line count right */
	    PL_doextract = FALSE;
	    while (*s && !(isSPACE (*s) || *s == '#')) s++;
	    s2 = s;
	    while (*s == ' ' || *s == '\t') s++;
	    if (*s++ == '-') {
		while (isDIGIT(s2[-1]) || strchr("-._", s2[-1])) s2--;
		if (strnEQ(s2-4,"perl",4))
		    /*SUPPRESS 530*/
		    while ((s = moreswitches(s)))
			;
	    }
#ifdef MACOS_TRADITIONAL
	    /* We are always searching for the #!perl line in MacPerl,
	     * so if we find it, still keep the line count correct
	     * by counting lines we already skipped over
	     */
	    for (; maclines > 0 ; maclines--)
		PerlIO_ungetc(PL_rsfp, '\n');

	    break;

	/* gMacPerl_AlwaysExtract is false in MPW tool */
	} else if (gMacPerl_AlwaysExtract) {
	    ++maclines;
#endif
	}
    }
}


STATIC void
S_init_ids(pTHX)
{
    PL_uid = PerlProc_getuid();
    PL_euid = PerlProc_geteuid();
    PL_gid = PerlProc_getgid();
    PL_egid = PerlProc_getegid();
#ifdef VMS
    PL_uid |= PL_gid << 16;
    PL_euid |= PL_egid << 16;
#endif
    /* Should not happen: */
    CHECK_MALLOC_TAINT(PL_uid && (PL_euid != PL_uid || PL_egid != PL_gid));
    PL_tainting |= (PL_uid && (PL_euid != PL_uid || PL_egid != PL_gid));
    /* BUG */
    /* PSz 27 Feb 04
     * Should go by suidscript, not uid!=euid: why disallow
     * system("ls") in scripts run from setuid things?
     * Or, is this run before we check arguments and set suidscript?
     * What about SETUID_SCRIPTS_ARE_SECURE_NOW: could we use fdscript then?
     * (We never have suidscript, can we be sure to have fdscript?)
     * Or must then go by UID checks? See comments in forbid_setid also.
     */
}

/* This is used very early in the lifetime of the program,
 * before even the options are parsed, so PL_tainting has
 * not been initialized properly.  */
bool
Perl_doing_taint(int argc, char *argv[], char *envp[])
{
#ifndef PERL_IMPLICIT_SYS
    /* If we have PERL_IMPLICIT_SYS we can't call getuid() et alia
     * before we have an interpreter-- and the whole point of this
     * function is to be called at such an early stage.  If you are on
     * a system with PERL_IMPLICIT_SYS but you do have a concept of
     * "tainted because running with altered effective ids', you'll
     * have to add your own checks somewhere in here.  The two most
     * known samples of 'implicitness' are Win32 and NetWare, neither
     * of which has much of concept of 'uids'. */
    int uid  = PerlProc_getuid();
    int euid = PerlProc_geteuid();
    int gid  = PerlProc_getgid();
    int egid = PerlProc_getegid();

#ifdef VMS
    uid  |=  gid << 16;
    euid |= egid << 16;
#endif
    if (uid && (euid != uid || egid != gid))
	return 1;
#endif /* !PERL_IMPLICIT_SYS */
    /* This is a really primitive check; environment gets ignored only
     * if -T are the first chars together; otherwise one gets
     *  "Too late" message. */
    if ( argc > 1 && argv[1][0] == '-'
         && (argv[1][1] == 't' || argv[1][1] == 'T') )
	return 1;
    return 0;
}

STATIC void
S_forbid_setid(pTHX_ char *s)
{
#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
    if (PL_euid != PL_uid)
        Perl_croak(aTHX_ "No %s allowed while running setuid", s);
    if (PL_egid != PL_gid)
        Perl_croak(aTHX_ "No %s allowed while running setgid", s);
#endif /* SETUID_SCRIPTS_ARE_SECURE_NOW */
    /* PSz 29 Feb 04
     * Checks for UID/GID above "wrong": why disallow
     *   perl -e 'print "Hello\n"'
     * from within setuid things?? Simply drop them: replaced by
     * fdscript/suidscript and #ifdef IAMSUID checks below.
     * 
     * This may be too late for command-line switches. Will catch those on
     * the #! line, after finding the script name and setting up
     * fdscript/suidscript. Note that suidperl does not get around to
     * parsing (and checking) the switches on the #! line, but checks that
     * the two sets are identical.
     * 
     * With SETUID_SCRIPTS_ARE_SECURE_NOW, could we use fdscript, also or
     * instead, or would that be "too late"? (We never have suidscript, can
     * we be sure to have fdscript?)
     * 
     * Catch things with suidscript (in descendant of suidperl), even with
     * right UID/GID. Was already checked in suidperl, with #ifdef IAMSUID,
     * below; but I am paranoid.
     * 
     * Also see comments about root running a setuid script, elsewhere.
     */
    if (PL_suidscript >= 0)
        Perl_croak(aTHX_ "No %s allowed with (suid) fdscript", s);
#ifdef IAMSUID
    /* PSz 11 Nov 03  Catch it in suidperl, always! */
    Perl_croak(aTHX_ "No %s allowed in suidperl", s);
#endif /* IAMSUID */
}

void
Perl_init_debugger(pTHX)
{
    HV *ostash = PL_curstash;

    PL_curstash = PL_debstash;
    PL_dbargs = GvAV(gv_AVadd((gv_fetchpv("DB::args", GV_ADDMULTI, SVt_PVAV))));
    AvREAL_off(PL_dbargs);
    PL_DBgv = gv_fetchpv("DB::DB", GV_ADDMULTI, SVt_PVGV);
    PL_DBline = gv_fetchpv("DB::dbline", GV_ADDMULTI, SVt_PVAV);
    PL_DBsub = gv_HVadd(gv_fetchpv("DB::sub", GV_ADDMULTI, SVt_PVHV));
    sv_upgrade(GvSV(PL_DBsub), SVt_IV);	/* IVX accessed if PERLDB_SUB_NN */
    PL_DBsingle = GvSV((gv_fetchpv("DB::single", GV_ADDMULTI, SVt_PV)));
    sv_setiv(PL_DBsingle, 0);
    PL_DBtrace = GvSV((gv_fetchpv("DB::trace", GV_ADDMULTI, SVt_PV)));
    sv_setiv(PL_DBtrace, 0);
    PL_DBsignal = GvSV((gv_fetchpv("DB::signal", GV_ADDMULTI, SVt_PV)));
    sv_setiv(PL_DBsignal, 0);
    PL_curstash = ostash;
}

#ifndef STRESS_REALLOC
#define REASONABLE(size) (size)
#else
#define REASONABLE(size) (1) /* unreasonable */
#endif

void
Perl_init_stacks(pTHX)
{
    /* start with 128-item stack and 8K cxstack */
    PL_curstackinfo = new_stackinfo(REASONABLE(128),
				 REASONABLE(8192/sizeof(PERL_CONTEXT) - 1));
    PL_curstackinfo->si_type = PERLSI_MAIN;
    PL_curstack = PL_curstackinfo->si_stack;
    PL_mainstack = PL_curstack;		/* remember in case we switch stacks */

    PL_stack_base = AvARRAY(PL_curstack);
    PL_stack_sp = PL_stack_base;
    PL_stack_max = PL_stack_base + AvMAX(PL_curstack);

    New(50,PL_tmps_stack,REASONABLE(128),SV*);
    PL_tmps_floor = -1;
    PL_tmps_ix = -1;
    PL_tmps_max = REASONABLE(128);

    New(54,PL_markstack,REASONABLE(32),I32);
    PL_markstack_ptr = PL_markstack;
    PL_markstack_max = PL_markstack + REASONABLE(32);

    SET_MARK_OFFSET;

    New(54,PL_scopestack,REASONABLE(32),I32);
    PL_scopestack_ix = 0;
    PL_scopestack_max = REASONABLE(32);

    New(54,PL_savestack,REASONABLE(128),ANY);
    PL_savestack_ix = 0;
    PL_savestack_max = REASONABLE(128);

    New(54,PL_retstack,REASONABLE(16),OP*);
    PL_retstack_ix = 0;
    PL_retstack_max = REASONABLE(16);
}

#undef REASONABLE

STATIC void
S_nuke_stacks(pTHX)
{
    while (PL_curstackinfo->si_next)
	PL_curstackinfo = PL_curstackinfo->si_next;
    while (PL_curstackinfo) {
	PERL_SI *p = PL_curstackinfo->si_prev;
	/* curstackinfo->si_stack got nuked by sv_free_arenas() */
	Safefree(PL_curstackinfo->si_cxstack);
	Safefree(PL_curstackinfo);
	PL_curstackinfo = p;
    }
    Safefree(PL_tmps_stack);
    Safefree(PL_markstack);
    Safefree(PL_scopestack);
    Safefree(PL_savestack);
    Safefree(PL_retstack);
}

STATIC void
S_init_lexer(pTHX)
{
    PerlIO *tmpfp;
    tmpfp = PL_rsfp;
    PL_rsfp = Nullfp;
    lex_start(PL_linestr);
    PL_rsfp = tmpfp;
    PL_subname = newSVpvn("main",4);
}

STATIC void
S_init_predump_symbols(pTHX)
{
    GV *tmpgv;
    IO *io;

    sv_setpvn(get_sv("\"", TRUE), " ", 1);
    PL_stdingv = gv_fetchpv("STDIN",TRUE, SVt_PVIO);
    GvMULTI_on(PL_stdingv);
    io = GvIOp(PL_stdingv);
    IoTYPE(io) = IoTYPE_RDONLY;
    IoIFP(io) = PerlIO_stdin();
    tmpgv = gv_fetchpv("stdin",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(io);

    tmpgv = gv_fetchpv("STDOUT",TRUE, SVt_PVIO);
    GvMULTI_on(tmpgv);
    io = GvIOp(tmpgv);
    IoTYPE(io) = IoTYPE_WRONLY;
    IoOFP(io) = IoIFP(io) = PerlIO_stdout();
    setdefout(tmpgv);
    tmpgv = gv_fetchpv("stdout",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(io);

    PL_stderrgv = gv_fetchpv("STDERR",TRUE, SVt_PVIO);
    GvMULTI_on(PL_stderrgv);
    io = GvIOp(PL_stderrgv);
    IoTYPE(io) = IoTYPE_WRONLY;
    IoOFP(io) = IoIFP(io) = PerlIO_stderr();
    tmpgv = gv_fetchpv("stderr",TRUE, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = (IO*)SvREFCNT_inc(io);

    PL_statname = NEWSV(66,0);		/* last filename we did stat on */

    if (PL_osname)
    	Safefree(PL_osname);
    PL_osname = savepv(OSNAME);
}

void
Perl_init_argv_symbols(pTHX_ register int argc, register char **argv)
{
    char *s;
    argc--,argv++;	/* skip name of script */
    if (PL_doswitches) {
	for (; argc > 0 && **argv == '-'; argc--,argv++) {
	    if (!argv[0][1])
		break;
	    if (argv[0][1] == '-' && !argv[0][2]) {
		argc--,argv++;
		break;
	    }
	    if ((s = strchr(argv[0], '='))) {
		*s++ = '\0';
		sv_setpv(GvSV(gv_fetchpv(argv[0]+1,TRUE, SVt_PV)),s);
	    }
	    else
		sv_setiv(GvSV(gv_fetchpv(argv[0]+1,TRUE, SVt_PV)),1);
	}
    }
    if ((PL_argvgv = gv_fetchpv("ARGV",TRUE, SVt_PVAV))) {
	GvMULTI_on(PL_argvgv);
	(void)gv_AVadd(PL_argvgv);
	av_clear(GvAVn(PL_argvgv));
	for (; argc > 0; argc--,argv++) {
	    SV *sv = newSVpv(argv[0],0);
	    av_push(GvAVn(PL_argvgv),sv);
	    if (!(PL_unicode & PERL_UNICODE_LOCALE_FLAG) || PL_utf8locale) {
		 if (PL_unicode & PERL_UNICODE_ARGV_FLAG)
		      SvUTF8_on(sv);
	    }
	    if (PL_unicode & PERL_UNICODE_WIDESYSCALLS_FLAG) /* Sarathy? */
		 (void)sv_utf8_decode(sv);
	}
    }
}

#ifdef HAS_PROCSELFEXE
/* This is a function so that we don't hold on to MAXPATHLEN
   bytes of stack longer than necessary
 */
STATIC void
S_procself_val(pTHX_ SV *sv, char *arg0)
{
    char buf[MAXPATHLEN];
    int len = readlink(PROCSELFEXE_PATH, buf, sizeof(buf) - 1);

    /* On Playstation2 Linux V1.0 (kernel 2.2.1) readlink(/proc/self/exe)
       includes a spurious NUL which will cause $^X to fail in system
       or backticks (this will prevent extensions from being built and
       many tests from working). readlink is not meant to add a NUL.
       Normal readlink works fine.
     */
    if (len > 0 && buf[len-1] == '\0') {
      len--;
    }

    /* FreeBSD's implementation is acknowledged to be imperfect, sometimes
       returning the text "unknown" from the readlink rather than the path
       to the executable (or returning an error from the readlink).  Any valid
       path has a '/' in it somewhere, so use that to validate the result.
       See http://www.freebsd.org/cgi/query-pr.cgi?pr=35703
    */
    if (len > 0 && memchr(buf, '/', len)) {
	sv_setpvn(sv,buf,len);
    }
    else {
	sv_setpv(sv,arg0);
    }
}
#endif /* HAS_PROCSELFEXE */

STATIC void
S_init_postdump_symbols(pTHX_ register int argc, register char **argv, register char **env)
{
    char *s;
    SV *sv;
    GV* tmpgv;

    PL_toptarget = NEWSV(0,0);
    sv_upgrade(PL_toptarget, SVt_PVFM);
    sv_setpvn(PL_toptarget, "", 0);
    PL_bodytarget = NEWSV(0,0);
    sv_upgrade(PL_bodytarget, SVt_PVFM);
    sv_setpvn(PL_bodytarget, "", 0);
    PL_formtarget = PL_bodytarget;

    TAINT;

    init_argv_symbols(argc,argv);

    if ((tmpgv = gv_fetchpv("0",TRUE, SVt_PV))) {
#ifdef MACOS_TRADITIONAL
	/* $0 is not majick on a Mac */
	sv_setpv(GvSV(tmpgv),MacPerl_MPWFileName(PL_origfilename));
#else
	sv_setpv(GvSV(tmpgv),PL_origfilename);
	magicname("0", "0", 1);
#endif
    }
    if ((tmpgv = gv_fetchpv("\030",TRUE, SVt_PV))) {/* $^X */
#ifdef HAS_PROCSELFEXE
	S_procself_val(aTHX_ GvSV(tmpgv), PL_origargv[0]);
#else
#ifdef OS2
	sv_setpv(GvSV(tmpgv), os2_execname(aTHX));
#else
	sv_setpv(GvSV(tmpgv),PL_origargv[0]);
#endif
#endif
    }
    if ((PL_envgv = gv_fetchpv("ENV",TRUE, SVt_PVHV))) {
	HV *hv;
	GvMULTI_on(PL_envgv);
	hv = GvHVn(PL_envgv);
	hv_magic(hv, Nullgv, PERL_MAGIC_env);
#ifndef PERL_MICRO
#ifdef USE_ENVIRON_ARRAY
	/* Note that if the supplied env parameter is actually a copy
	   of the global environ then it may now point to free'd memory
	   if the environment has been modified since. To avoid this
	   problem we treat env==NULL as meaning 'use the default'
	*/
	if (!env)
	    env = environ;
	if (env != environ
#  ifdef USE_ITHREADS
	    && PL_curinterp == aTHX
#  endif
	   )
	{
	    environ[0] = Nullch;
	}
	if (env) {
          char** origenv = environ;
	  for (; *env; env++) {
	    if (!(s = strchr(*env,'=')) || s == *env)
		continue;
#if defined(MSDOS) && !defined(DJGPP)
	    *s = '\0';
	    (void)strupr(*env);
	    *s = '=';
#endif
	    sv = newSVpv(s+1, 0);
	    (void)hv_store(hv, *env, s - *env, sv, 0);
	    if (env != environ)
	        mg_set(sv);
	    if (origenv != environ) {
	      /* realloc has shifted us */
	      env = (env - origenv) + environ;
	      origenv = environ;
	    }
	  }
      }
#endif /* USE_ENVIRON_ARRAY */
#endif /* !PERL_MICRO */
    }
    TAINT_NOT;
    if ((tmpgv = gv_fetchpv("$",TRUE, SVt_PV))) {
        SvREADONLY_off(GvSV(tmpgv));
	sv_setiv(GvSV(tmpgv), (IV)PerlProc_getpid());
        SvREADONLY_on(GvSV(tmpgv));
    }
#ifdef THREADS_HAVE_PIDS
    PL_ppid = (IV)getppid();
#endif

    /* touch @F array to prevent spurious warnings 20020415 MJD */
    if (PL_minus_a) {
      (void) get_av("main::F", TRUE | GV_ADDMULTI);
    }
    /* touch @- and @+ arrays to prevent spurious warnings 20020415 MJD */
    (void) get_av("main::-", TRUE | GV_ADDMULTI);
    (void) get_av("main::+", TRUE | GV_ADDMULTI);
}

STATIC void
S_init_perllib(pTHX)
{
    char *s;
    if (!PL_tainting) {
#ifndef VMS
	s = PerlEnv_getenv("PERL5LIB");
	if (s)
	    incpush(s, TRUE, TRUE, TRUE);
	else
	    incpush(PerlEnv_getenv("PERLLIB"), FALSE, FALSE, TRUE);
#else /* VMS */
	/* Treat PERL5?LIB as a possible search list logical name -- the
	 * "natural" VMS idiom for a Unix path string.  We allow each
	 * element to be a set of |-separated directories for compatibility.
	 */
	char buf[256];
	int idx = 0;
	if (my_trnlnm("PERL5LIB",buf,0))
	    do { incpush(buf,TRUE,TRUE,TRUE); } while (my_trnlnm("PERL5LIB",buf,++idx));
	else
	    while (my_trnlnm("PERLLIB",buf,idx++)) incpush(buf,FALSE,FALSE,TRUE);
#endif /* VMS */
    }

/* Use the ~-expanded versions of APPLLIB (undocumented),
    ARCHLIB PRIVLIB SITEARCH SITELIB VENDORARCH and VENDORLIB
*/
#ifdef APPLLIB_EXP
    incpush(APPLLIB_EXP, TRUE, TRUE, TRUE);
#endif

#ifdef ARCHLIB_EXP
    incpush(ARCHLIB_EXP, FALSE, FALSE, TRUE);
#endif
#ifdef MACOS_TRADITIONAL
    {
	Stat_t tmpstatbuf;
    	SV * privdir = NEWSV(55, 0);
	char * macperl = PerlEnv_getenv("MACPERL");
	
	if (!macperl)
	    macperl = "";
	
	Perl_sv_setpvf(aTHX_ privdir, "%slib:", macperl);
	if (PerlLIO_stat(SvPVX(privdir), &tmpstatbuf) >= 0 && S_ISDIR(tmpstatbuf.st_mode))
	    incpush(SvPVX(privdir), TRUE, FALSE, TRUE);
	Perl_sv_setpvf(aTHX_ privdir, "%ssite_perl:", macperl);
	if (PerlLIO_stat(SvPVX(privdir), &tmpstatbuf) >= 0 && S_ISDIR(tmpstatbuf.st_mode))
	    incpush(SvPVX(privdir), TRUE, FALSE, TRUE);
	
   	SvREFCNT_dec(privdir);
    }
    if (!PL_tainting)
	incpush(":", FALSE, FALSE, TRUE);
#else
#ifndef PRIVLIB_EXP
#  define PRIVLIB_EXP "/usr/local/lib/perl5:/usr/local/lib/perl"
#endif
#if defined(WIN32)
    incpush(PRIVLIB_EXP, TRUE, FALSE, TRUE);
#else
    incpush(PRIVLIB_EXP, FALSE, FALSE, TRUE);
#endif

#ifdef SITEARCH_EXP
    /* sitearch is always relative to sitelib on Windows for
     * DLL-based path intuition to work correctly */
#  if !defined(WIN32)
    incpush(SITEARCH_EXP, FALSE, FALSE, TRUE);
#  endif
#endif

#ifdef SITELIB_EXP
#  if defined(WIN32)
    /* this picks up sitearch as well */
    incpush(SITELIB_EXP, TRUE, FALSE, TRUE);
#  else
    incpush(SITELIB_EXP, FALSE, FALSE, TRUE);
#  endif
#endif

#ifdef SITELIB_STEM /* Search for version-specific dirs below here */
    incpush(SITELIB_STEM, FALSE, TRUE, TRUE);
#endif

#ifdef PERL_VENDORARCH_EXP
    /* vendorarch is always relative to vendorlib on Windows for
     * DLL-based path intuition to work correctly */
#  if !defined(WIN32)
    incpush(PERL_VENDORARCH_EXP, FALSE, FALSE, TRUE);
#  endif
#endif

#ifdef PERL_VENDORLIB_EXP
#  if defined(WIN32)
    incpush(PERL_VENDORLIB_EXP, TRUE, FALSE, TRUE);	/* this picks up vendorarch as well */
#  else
    incpush(PERL_VENDORLIB_EXP, FALSE, FALSE, TRUE);
#  endif
#endif

#ifdef PERL_VENDORLIB_STEM /* Search for version-specific dirs below here */
    incpush(PERL_VENDORLIB_STEM, FALSE, TRUE, TRUE);
#endif

#ifdef PERL_OTHERLIBDIRS
    incpush(PERL_OTHERLIBDIRS, TRUE, TRUE, TRUE);
#endif

    if (!PL_tainting)
	incpush(".", FALSE, FALSE, TRUE);
#endif /* MACOS_TRADITIONAL */
}

#if defined(DOSISH) || defined(EPOC)
#    define PERLLIB_SEP ';'
#else
#  if defined(VMS)
#    define PERLLIB_SEP '|'
#  else
#    if defined(MACOS_TRADITIONAL)
#      define PERLLIB_SEP ','
#    else
#      define PERLLIB_SEP ':'
#    endif
#  endif
#endif
#ifndef PERLLIB_MANGLE
#  define PERLLIB_MANGLE(s,n) (s)
#endif

STATIC void
S_incpush(pTHX_ char *p, int addsubdirs, int addoldvers, int usesep)
{
    SV *subdir = Nullsv;

    if (!p || !*p)
	return;

    if (addsubdirs || addoldvers) {
	subdir = sv_newmortal();
    }

    /* Break at all separators */
    while (p && *p) {
	SV *libdir = NEWSV(55,0);
	char *s;

	/* skip any consecutive separators */
	if (usesep) {
	    while ( *p == PERLLIB_SEP ) {
		/* Uncomment the next line for PATH semantics */
		/* av_push(GvAVn(PL_incgv), newSVpvn(".", 1)); */
		p++;
	    }
	}

	if ( usesep && (s = strchr(p, PERLLIB_SEP)) != Nullch ) {
	    sv_setpvn(libdir, PERLLIB_MANGLE(p, (STRLEN)(s - p)),
		      (STRLEN)(s - p));
	    p = s + 1;
	}
	else {
	    sv_setpv(libdir, PERLLIB_MANGLE(p, 0));
	    p = Nullch;	/* break out */
	}
#ifdef MACOS_TRADITIONAL
	if (!strchr(SvPVX(libdir), ':')) {
	    char buf[256];

	    sv_setpv(libdir, MacPerl_CanonDir(SvPVX(libdir), buf, 0));
	}
	if (SvPVX(libdir)[SvCUR(libdir)-1] != ':')
	    sv_catpv(libdir, ":");
#endif

	/*
	 * BEFORE pushing libdir onto @INC we may first push version- and
	 * archname-specific sub-directories.
	 */
	if (addsubdirs || addoldvers) {
#ifdef PERL_INC_VERSION_LIST
	    /* Configure terminates PERL_INC_VERSION_LIST with a NULL */
	    const char *incverlist[] = { PERL_INC_VERSION_LIST };
	    const char **incver;
#endif
	    Stat_t tmpstatbuf;
#ifdef VMS
	    char *unix;
	    STRLEN len;

	    if ((unix = tounixspec_ts(SvPV(libdir,len),Nullch)) != Nullch) {
		len = strlen(unix);
		while (unix[len-1] == '/') len--;  /* Cosmetic */
		sv_usepvn(libdir,unix,len);
	    }
	    else
		PerlIO_printf(Perl_error_log,
		              "Failed to unixify @INC element \"%s\"\n",
			      SvPV(libdir,len));
#endif
	    if (addsubdirs) {
#ifdef MACOS_TRADITIONAL
#define PERL_AV_SUFFIX_FMT	""
#define PERL_ARCH_FMT 		"%s:"
#define PERL_ARCH_FMT_PATH	PERL_FS_VER_FMT PERL_AV_SUFFIX_FMT
#else
#define PERL_AV_SUFFIX_FMT 	"/"
#define PERL_ARCH_FMT 		"/%s"
#define PERL_ARCH_FMT_PATH	PERL_AV_SUFFIX_FMT PERL_FS_VER_FMT
#endif
		/* .../version/archname if -d .../version/archname */
		Perl_sv_setpvf(aTHX_ subdir, "%"SVf PERL_ARCH_FMT_PATH PERL_ARCH_FMT,
				libdir,
			       (int)PERL_REVISION, (int)PERL_VERSION,
			       (int)PERL_SUBVERSION, ARCHNAME);
		if (PerlLIO_stat(SvPVX(subdir), &tmpstatbuf) >= 0 &&
		      S_ISDIR(tmpstatbuf.st_mode))
		    av_push(GvAVn(PL_incgv), newSVsv(subdir));

		/* .../version if -d .../version */
		Perl_sv_setpvf(aTHX_ subdir, "%"SVf PERL_ARCH_FMT_PATH, libdir,
			       (int)PERL_REVISION, (int)PERL_VERSION,
			       (int)PERL_SUBVERSION);
		if (PerlLIO_stat(SvPVX(subdir), &tmpstatbuf) >= 0 &&
		      S_ISDIR(tmpstatbuf.st_mode))
		    av_push(GvAVn(PL_incgv), newSVsv(subdir));

		/* .../archname if -d .../archname */
		Perl_sv_setpvf(aTHX_ subdir, "%"SVf PERL_ARCH_FMT, libdir, ARCHNAME);
		if (PerlLIO_stat(SvPVX(subdir), &tmpstatbuf) >= 0 &&
		      S_ISDIR(tmpstatbuf.st_mode))
		    av_push(GvAVn(PL_incgv), newSVsv(subdir));
	    }

#ifdef PERL_INC_VERSION_LIST
	    if (addoldvers) {
		for (incver = incverlist; *incver; incver++) {
		    /* .../xxx if -d .../xxx */
		    Perl_sv_setpvf(aTHX_ subdir, "%"SVf PERL_ARCH_FMT, libdir, *incver);
		    if (PerlLIO_stat(SvPVX(subdir), &tmpstatbuf) >= 0 &&
			  S_ISDIR(tmpstatbuf.st_mode))
			av_push(GvAVn(PL_incgv), newSVsv(subdir));
		}
	    }
#endif
	}

	/* finally push this lib directory on the end of @INC */
	av_push(GvAVn(PL_incgv), libdir);
    }
}

#ifdef USE_5005THREADS
STATIC struct perl_thread *
S_init_main_thread(pTHX)
{
#if !defined(PERL_IMPLICIT_CONTEXT)
    struct perl_thread *thr;
#endif
    XPV *xpv;

    Newz(53, thr, 1, struct perl_thread);
    PL_curcop = &PL_compiling;
    thr->interp = PERL_GET_INTERP;
    thr->cvcache = newHV();
    thr->threadsv = newAV();
    /* thr->threadsvp is set when find_threadsv is called */
    thr->specific = newAV();
    thr->flags = THRf_R_JOINABLE;
    MUTEX_INIT(&thr->mutex);
    /* Handcraft thrsv similarly to mess_sv */
    New(53, PL_thrsv, 1, SV);
    Newz(53, xpv, 1, XPV);
    SvFLAGS(PL_thrsv) = SVt_PV;
    SvANY(PL_thrsv) = (void*)xpv;
    SvREFCNT(PL_thrsv) = 1 << 30;	/* practically infinite */
    SvPVX(PL_thrsv) = (char*)thr;
    SvCUR_set(PL_thrsv, sizeof(thr));
    SvLEN_set(PL_thrsv, sizeof(thr));
    *SvEND(PL_thrsv) = '\0';	/* in the trailing_nul field */
    thr->oursv = PL_thrsv;
    PL_chopset = " \n-";
    PL_dumpindent = 4;

    MUTEX_LOCK(&PL_threads_mutex);
    PL_nthreads++;
    thr->tid = 0;
    thr->next = thr;
    thr->prev = thr;
    thr->thr_done = 0;
    MUTEX_UNLOCK(&PL_threads_mutex);

#ifdef HAVE_THREAD_INTERN
    Perl_init_thread_intern(thr);
#endif

#ifdef SET_THREAD_SELF
    SET_THREAD_SELF(thr);
#else
    thr->self = pthread_self();
#endif /* SET_THREAD_SELF */
    PERL_SET_THX(thr);

    /*
     * These must come after the thread self setting
     * because sv_setpvn does SvTAINT and the taint
     * fields thread selfness being set.
     */
    PL_toptarget = NEWSV(0,0);
    sv_upgrade(PL_toptarget, SVt_PVFM);
    sv_setpvn(PL_toptarget, "", 0);
    PL_bodytarget = NEWSV(0,0);
    sv_upgrade(PL_bodytarget, SVt_PVFM);
    sv_setpvn(PL_bodytarget, "", 0);
    PL_formtarget = PL_bodytarget;
    thr->errsv = newSVpvn("", 0);
    (void) find_threadsv("@");	/* Ensure $@ is initialised early */

    PL_maxscream = -1;
    PL_peepp = MEMBER_TO_FPTR(Perl_peep);
    PL_regcompp = MEMBER_TO_FPTR(Perl_pregcomp);
    PL_regexecp = MEMBER_TO_FPTR(Perl_regexec_flags);
    PL_regint_start = MEMBER_TO_FPTR(Perl_re_intuit_start);
    PL_regint_string = MEMBER_TO_FPTR(Perl_re_intuit_string);
    PL_regfree = MEMBER_TO_FPTR(Perl_pregfree);
    PL_regindent = 0;
    PL_reginterp_cnt = 0;

    return thr;
}
#endif /* USE_5005THREADS */

void
Perl_call_list(pTHX_ I32 oldscope, AV *paramList)
{
    SV *atsv;
    line_t oldline = CopLINE(PL_curcop);
    CV *cv;
    STRLEN len;
    int ret;
    dJMPENV;

    while (AvFILL(paramList) >= 0) {
	cv = (CV*)av_shift(paramList);
	if (PL_savebegin) {
	    if (paramList == PL_beginav) {
		/* save PL_beginav for compiler */
		if (! PL_beginav_save)
		    PL_beginav_save = newAV();
		av_push(PL_beginav_save, (SV*)cv);
	    }
	    else if (paramList == PL_checkav) {
		/* save PL_checkav for compiler */
		if (! PL_checkav_save)
		    PL_checkav_save = newAV();
		av_push(PL_checkav_save, (SV*)cv);
	    }
	} else {
	    SAVEFREESV(cv);
	}
#ifdef PERL_FLEXIBLE_EXCEPTIONS
	CALLPROTECT(aTHX_ pcur_env, &ret, MEMBER_TO_FPTR(S_vcall_list_body), cv);
#else
	JMPENV_PUSH(ret);
#endif
	switch (ret) {
	case 0:
#ifndef PERL_FLEXIBLE_EXCEPTIONS
	    call_list_body(cv);
#endif
	    atsv = ERRSV;
	    (void)SvPV(atsv, len);
	    if (len) {
		PL_curcop = &PL_compiling;
		CopLINE_set(PL_curcop, oldline);
		if (paramList == PL_beginav)
		    sv_catpv(atsv, "BEGIN failed--compilation aborted");
		else
		    Perl_sv_catpvf(aTHX_ atsv,
				   "%s failed--call queue aborted",
				   paramList == PL_checkav ? "CHECK"
				   : paramList == PL_initav ? "INIT"
				   : "END");
		while (PL_scopestack_ix > oldscope)
		    LEAVE;
		JMPENV_POP;
		Perl_croak(aTHX_ "%"SVf"", atsv);
	    }
	    break;
	case 1:
	    STATUS_ALL_FAILURE;
	    /* FALL THROUGH */
	case 2:
	    /* my_exit() was called */
	    while (PL_scopestack_ix > oldscope)
		LEAVE;
	    FREETMPS;
	    PL_curstash = PL_defstash;
	    PL_curcop = &PL_compiling;
	    CopLINE_set(PL_curcop, oldline);
	    JMPENV_POP;
	    if (PL_statusvalue && !(PL_exit_flags & PERL_EXIT_EXPECTED)) {
		if (paramList == PL_beginav)
		    Perl_croak(aTHX_ "BEGIN failed--compilation aborted");
		else
		    Perl_croak(aTHX_ "%s failed--call queue aborted",
			       paramList == PL_checkav ? "CHECK"
			       : paramList == PL_initav ? "INIT"
			       : "END");
	    }
	    my_exit_jump();
	    /* NOTREACHED */
	case 3:
	    if (PL_restartop) {
		PL_curcop = &PL_compiling;
		CopLINE_set(PL_curcop, oldline);
		JMPENV_JUMP(3);
	    }
	    PerlIO_printf(Perl_error_log, "panic: restartop\n");
	    FREETMPS;
	    break;
	}
	JMPENV_POP;
    }
}

#ifdef PERL_FLEXIBLE_EXCEPTIONS
STATIC void *
S_vcall_list_body(pTHX_ va_list args)
{
    CV *cv = va_arg(args, CV*);
    return call_list_body(cv);
}
#endif

STATIC void *
S_call_list_body(pTHX_ CV *cv)
{
    PUSHMARK(PL_stack_sp);
    call_sv((SV*)cv, G_EVAL|G_DISCARD);
    return NULL;
}

void
Perl_my_exit(pTHX_ U32 status)
{
    DEBUG_S(PerlIO_printf(Perl_debug_log, "my_exit: thread %p, status %lu\n",
			  thr, (unsigned long) status));
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
Perl_my_failure_exit(pTHX)
{
#ifdef VMS
    if (vaxc$errno & 1) {
	if (STATUS_NATIVE & 1)		/* fortuitiously includes "-1" */
	    STATUS_NATIVE_SET(44);
    }
    else {
	if (!vaxc$errno)		/* unlikely */
	    STATUS_NATIVE_SET(44);
	else
	    STATUS_NATIVE_SET(vaxc$errno);
    }
#else
    int exitstatus;
    if (errno & 255)
	STATUS_POSIX_SET(errno);
    else {
	exitstatus = STATUS_POSIX >> 8;
	if (exitstatus & 255)
	    STATUS_POSIX_SET(exitstatus);
	else
	    STATUS_POSIX_SET(255);
    }
#endif
    my_exit_jump();
}

STATIC void
S_my_exit_jump(pTHX)
{
    register PERL_CONTEXT *cx;
    I32 gimme;
    SV **newsp;

    if (PL_e_script) {
	SvREFCNT_dec(PL_e_script);
	PL_e_script = Nullsv;
    }

    POPSTACK_TO(PL_mainstack);
    if (cxstack_ix >= 0) {
	if (cxstack_ix > 0)
	    dounwind(0);
	POPBLOCK(cx,PL_curpm);
	LEAVE;
    }

    JMPENV_JUMP(2);
}

static I32
read_e_script(pTHX_ int idx, SV *buf_sv, int maxlen)
{
    char *p, *nl;
    p  = SvPVX(PL_e_script);
    nl = strchr(p, '\n');
    nl = (nl) ? nl+1 : SvEND(PL_e_script);
    if (nl-p == 0) {
	filter_del(read_e_script);
	return 0;
    }
    sv_catpvn(buf_sv, p, nl-p);
    sv_chop(PL_e_script, nl);
    return 1;
}
