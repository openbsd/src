/*    perl.c
 *
 *    Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001
 *    2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009 by Larry Wall and others
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 *      A ship then new they built for him
 *      of mithril and of elven-glass
 *              --from Bilbo's song of Eärendil
 *
 *     [p.236 of _The Lord of the Rings_, II/i: "Many Meetings"]
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

#ifdef DEBUG_LEAKING_SCALARS_FORK_DUMP
#  ifdef I_SYS_WAIT
#   include <sys/wait.h>
#  endif
#  ifdef I_SYSUIO
#    include <sys/uio.h>
#  endif

union control_un {
  struct cmsghdr cm;
  char control[CMSG_SPACE(sizeof(int))];
};

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

#ifdef DOSUID
#  ifdef IAMSUID
/* Drop scriptname */
#    define validate_suid(validarg, scriptname, fdscript, suidscript, linestr_sv, rsfp) S_validate_suid(aTHX_ validarg, fdscript, suidscript, linestr_sv, rsfp)
#  else
/* Drop suidscript */
#    define validate_suid(validarg, scriptname, fdscript, suidscript, linestr_sv, rsfp) S_validate_suid(aTHX_ validarg, scriptname, fdscript, linestr_sv, rsfp)
#  endif
#else
#  ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
/* Drop everything. Heck, don't even try to call it */
#    define validate_suid(validarg, scriptname, fdscript, suidscript, linestr_sv, rsfp) NOOP
#  else
/* Drop almost everything */
#    define validate_suid(validarg, scriptname, fdscript, suidscript, linestr_sv, rsfp) S_validate_suid(aTHX_ rsfp)
#  endif
#endif

#define CALL_BODY_EVAL(myop) \
    if (PL_op == (myop)) \
	PL_op = PL_ppaddr[OP_ENTEREVAL](aTHX); \
    if (PL_op) \
	CALLRUNOPS(aTHX);

#define CALL_BODY_SUB(myop) \
    if (PL_op == (myop)) \
	PL_op = PL_ppaddr[OP_ENTERSUB](aTHX); \
    if (PL_op) \
	CALLRUNOPS(aTHX);

#define CALL_LIST_BODY(cv) \
    PUSHMARK(PL_stack_sp); \
    call_sv(MUTABLE_SV((cv)), G_EVAL|G_DISCARD);

static void
S_init_tls_and_interp(PerlInterpreter *my_perl)
{
    dVAR;
    if (!PL_curinterp) {			
	PERL_SET_INTERP(my_perl);
#if defined(USE_ITHREADS)
	INIT_THREADS;
	ALLOC_THREAD_KEY;
	PERL_SET_THX(my_perl);
	OP_REFCNT_INIT;
	HINTS_REFCNT_INIT;
	MUTEX_INIT(&PL_dollarzero_mutex);
#  endif
#ifdef PERL_IMPLICIT_CONTEXT
	MUTEX_INIT(&PL_my_ctx_mutex);
#  endif
    }
#if defined(USE_ITHREADS)
    else
#else
    /* This always happens for non-ithreads  */
#endif
    {
	PERL_SET_THX(my_perl);
    }
}


/* these implement the PERL_SYS_INIT, PERL_SYS_INIT3, PERL_SYS_TERM macros */

void
Perl_sys_init(int* argc, char*** argv)
{
    dVAR;

    PERL_ARGS_ASSERT_SYS_INIT;

    PERL_UNUSED_ARG(argc); /* may not be used depending on _BODY macro */
    PERL_UNUSED_ARG(argv);
    PERL_SYS_INIT_BODY(argc, argv);
}

void
Perl_sys_init3(int* argc, char*** argv, char*** env)
{
    dVAR;

    PERL_ARGS_ASSERT_SYS_INIT3;

    PERL_UNUSED_ARG(argc); /* may not be used depending on _BODY macro */
    PERL_UNUSED_ARG(argv);
    PERL_UNUSED_ARG(env);
    PERL_SYS_INIT3_BODY(argc, argv, env);
}

void
Perl_sys_term()
{
    dVAR;
    if (!PL_veto_cleanup) {
	PERL_SYS_TERM_BODY();
    }
}


#ifdef PERL_IMPLICIT_SYS
PerlInterpreter *
perl_alloc_using(struct IPerlMem* ipM, struct IPerlMem* ipMS,
		 struct IPerlMem* ipMP, struct IPerlEnv* ipE,
		 struct IPerlStdIO* ipStd, struct IPerlLIO* ipLIO,
		 struct IPerlDir* ipD, struct IPerlSock* ipS,
		 struct IPerlProc* ipP)
{
    PerlInterpreter *my_perl;

    PERL_ARGS_ASSERT_PERL_ALLOC_USING;

    /* Newx() needs interpreter, so call malloc() instead */
    my_perl = (PerlInterpreter*)(*ipM->pMalloc)(ipM, sizeof(PerlInterpreter));
    S_init_tls_and_interp(my_perl);
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
    INIT_TRACK_MEMPOOL(PL_memory_debug_header, my_perl);

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

    /* Newx() needs interpreter, so call malloc() instead */
    my_perl = (PerlInterpreter*)PerlMem_malloc(sizeof(PerlInterpreter));

    S_init_tls_and_interp(my_perl);
#ifndef PERL_TRACK_MEMPOOL
    return (PerlInterpreter *) ZeroD(my_perl, 1, PerlInterpreter);
#else
    Zero(my_perl, 1, PerlInterpreter);
    INIT_TRACK_MEMPOOL(PL_memory_debug_header, my_perl);
    return my_perl;
#endif
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
    dVAR;

    PERL_ARGS_ASSERT_PERL_CONSTRUCT;

#ifdef MULTIPLICITY
    init_interp();
    PL_perl_destruct_level = 1;
#else
    PERL_UNUSED_ARG(my_perl);
   if (PL_perl_destruct_level > 0)
       init_interp();
#endif
    PL_curcop = &PL_compiling;	/* needed by ckWARN, right away */

    /* set read-only and try to insure than we wont see REFCNT==0
       very often */

    SvREADONLY_on(&PL_sv_undef);
    SvREFCNT(&PL_sv_undef) = (~(U32)0)/2;

    sv_setpv(&PL_sv_no,PL_No);
    /* value lookup in void context - happens to have the side effect
       of caching the numeric forms. However, as &PL_sv_no doesn't contain
       a string that is a valid numer, we have to turn the public flags by
       hand:  */
    SvNV(&PL_sv_no);
    SvIV(&PL_sv_no);
    SvIOK_on(&PL_sv_no);
    SvNOK_on(&PL_sv_no);
    SvREADONLY_on(&PL_sv_no);
    SvREFCNT(&PL_sv_no) = (~(U32)0)/2;

    sv_setpv(&PL_sv_yes,PL_Yes);
    SvNV(&PL_sv_yes);
    SvIV(&PL_sv_yes);
    SvREADONLY_on(&PL_sv_yes);
    SvREFCNT(&PL_sv_yes) = (~(U32)0)/2;

    SvREADONLY_on(&PL_sv_placeholder);
    SvREFCNT(&PL_sv_placeholder) = (~(U32)0)/2;

    PL_sighandlerp = (Sighandler_t) Perl_sighandler;
#ifdef PERL_USES_PL_PIDSTATUS
    PL_pidstatus = newHV();
#endif

    PL_rs = newSVpvs("\n");

    init_stacks();

    init_ids();

    JMPENV_BOOTSTRAP;
    STATUS_ALL_SUCCESS;

    init_i18nl10n(1);
    SET_NUMERIC_STANDARD();

#if defined(LOCAL_PATCH_COUNT)
    PL_localpatches = local_patches;	/* For possible -v */
#endif

#ifdef HAVE_INTERP_INTERN
    sys_intern_init();
#endif

    PerlIO_init(aTHX);			/* Hook to IO system */

    PL_fdpid = newAV();			/* for remembering popen pids by fd */
    PL_modglobal = newHV();		/* pointers to per-interpreter module globals */
    PL_errors = newSVpvs("");
    sv_setpvs(PERL_DEBUG_PAD(0), "");	/* For regex debugging. */
    sv_setpvs(PERL_DEBUG_PAD(1), "");	/* ext/re needs these */
    sv_setpvs(PERL_DEBUG_PAD(2), "");	/* even without DEBUGGING. */
#ifdef USE_ITHREADS
    /* First entry is an array of empty elements */
    Perl_av_create_and_push(aTHX_ &PL_regex_padav,(SV*)newAV());
    PL_regex_pad = AvARRAY(PL_regex_padav);
#endif
#ifdef USE_REENTRANT_API
    Perl_reentrant_init(aTHX);
#endif

    /* Note that strtab is a rather special HV.  Assumptions are made
       about not iterating on it, and not adding tie magic to it.
       It is properly deallocated in perl_destruct() */
    PL_strtab = newHV();

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
     * available or if the sysconf() fails, use the HZ.
     * BeOS has those, but returns the wrong value.
     * The HZ if not originally defined has been by now
     * been defined as CLK_TCK, if available. */
#if defined(HAS_SYSCONF) && defined(_SC_CLK_TCK) && !defined(__BEOS__)
    PL_clocktick = sysconf(_SC_CLK_TCK);
    if (PL_clocktick <= 0)
#endif
	 PL_clocktick = HZ;

    PL_stashcache = newHV();

    PL_patchlevel = newSVpvs("v" PERL_VERSION_STRING);

#ifdef HAS_MMAP
    if (!PL_mmap_page_size) {
#if defined(HAS_SYSCONF) && (defined(_SC_PAGESIZE) || defined(_SC_MMAP_PAGE_SIZE))
      {
	SETERRNO(0, SS_NORMAL);
#   ifdef _SC_PAGESIZE
	PL_mmap_page_size = sysconf(_SC_PAGESIZE);
#   else
	PL_mmap_page_size = sysconf(_SC_MMAP_PAGE_SIZE);
#   endif
	if ((long) PL_mmap_page_size < 0) {
	  if (errno) {
	    SV * const error = ERRSV;
	    SvUPGRADE(error, SVt_PV);
	    Perl_croak(aTHX_ "panic: sysconf: %s", SvPV_nolen_const(error));
	  }
	  else
	    Perl_croak(aTHX_ "panic: sysconf: pagesize unknown");
	}
      }
#else
#   ifdef HAS_GETPAGESIZE
      PL_mmap_page_size = getpagesize();
#   else
#       if defined(I_SYS_PARAM) && defined(PAGESIZE)
      PL_mmap_page_size = PAGESIZE;       /* compiletime, bad */
#       endif
#   endif
#endif
      if (PL_mmap_page_size <= 0)
	Perl_croak(aTHX_ "panic: bad pagesize %" IVdf,
		   (IV) PL_mmap_page_size);
    }
#endif /* HAS_MMAP */

#if defined(HAS_TIMES) && defined(PERL_NEED_TIMESBASE)
    PL_timesbase.tms_utime  = 0;
    PL_timesbase.tms_stime  = 0;
    PL_timesbase.tms_cutime = 0;
    PL_timesbase.tms_cstime = 0;
#endif

    PL_registered_mros = newHV();
    /* Start with 1 bucket, for DFS.  It's unlikely we'll need more.  */
    HvMAX(PL_registered_mros) = 0;

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
    PERL_UNUSED_CONTEXT;
    return 0;
}

#ifdef DEBUG_LEAKING_SCALARS_FORK_DUMP
void
Perl_dump_sv_child(pTHX_ SV *sv)
{
    ssize_t got;
    const int sock = PL_dumper_fd;
    const int debug_fd = PerlIO_fileno(Perl_debug_log);
    union control_un control;
    struct msghdr msg;
    struct iovec vec[2];
    struct cmsghdr *cmptr;
    int returned_errno;
    unsigned char buffer[256];

    PERL_ARGS_ASSERT_DUMP_SV_CHILD;

    if(sock == -1 || debug_fd == -1)
	return;

    PerlIO_flush(Perl_debug_log);

    /* All these shenanigans are to pass a file descriptor over to our child for
       it to dump out to.  We can't let it hold open the file descriptor when it
       forks, as the file descriptor it will dump to can turn out to be one end
       of pipe that some other process will wait on for EOF. (So as it would
       be open, the wait would be forever.)  */

    msg.msg_control = control.control;
    msg.msg_controllen = sizeof(control.control);
    /* We're a connected socket so we don't need a destination  */
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = vec;
    msg.msg_iovlen = 1;

    cmptr = CMSG_FIRSTHDR(&msg);
    cmptr->cmsg_len = CMSG_LEN(sizeof(int));
    cmptr->cmsg_level = SOL_SOCKET;
    cmptr->cmsg_type = SCM_RIGHTS;
    *((int *)CMSG_DATA(cmptr)) = 1;

    vec[0].iov_base = (void*)&sv;
    vec[0].iov_len = sizeof(sv);
    got = sendmsg(sock, &msg, 0);

    if(got < 0) {
	perror("Debug leaking scalars parent sendmsg failed");
	abort();
    }
    if(got < sizeof(sv)) {
	perror("Debug leaking scalars parent short sendmsg");
	abort();
    }

    /* Return protocol is
       int:		errno value
       unsigned char:	length of location string (0 for empty)
       unsigned char*:	string (not terminated)
    */
    vec[0].iov_base = (void*)&returned_errno;
    vec[0].iov_len = sizeof(returned_errno);
    vec[1].iov_base = buffer;
    vec[1].iov_len = 1;

    got = readv(sock, vec, 2);

    if(got < 0) {
	perror("Debug leaking scalars parent read failed");
	PerlIO_flush(PerlIO_stderr());
	abort();
    }
    if(got < sizeof(returned_errno) + 1) {
	perror("Debug leaking scalars parent short read");
	PerlIO_flush(PerlIO_stderr());
	abort();
    }

    if (*buffer) {
	got = read(sock, buffer + 1, *buffer);
	if(got < 0) {
	    perror("Debug leaking scalars parent read 2 failed");
	    PerlIO_flush(PerlIO_stderr());
	    abort();
	}

	if(got < *buffer) {
	    perror("Debug leaking scalars parent short read 2");
	    PerlIO_flush(PerlIO_stderr());
	    abort();
	}
    }

    if (returned_errno || *buffer) {
	Perl_warn(aTHX_ "Debug leaking scalars child failed%s%.*s with errno"
		  " %d: %s", (*buffer ? " at " : ""), (int) *buffer, buffer + 1,
		  returned_errno, strerror(returned_errno));
    }
}
#endif

/*
=for apidoc perl_destruct

Shuts down a Perl interpreter.  See L<perlembed>.

=cut
*/

int
perl_destruct(pTHXx)
{
    dVAR;
    VOL signed char destruct_level;  /* see possible values in intrpvar.h */
    HV *hv;
#ifdef DEBUG_LEAKING_SCALARS_FORK_DUMP
    pid_t child;
#endif

    PERL_ARGS_ASSERT_PERL_DESTRUCT;
#ifndef MULTIPLICITY
    PERL_UNUSED_ARG(my_perl);
#endif

    /* wait for all pseudo-forked children to finish */
    PERL_WAIT_FOR_CHILDREN;

    destruct_level = PL_perl_destruct_level;
#ifdef DEBUGGING
    {
	const char * const s = PerlEnv_getenv("PERL_DESTRUCT_LEVEL");
	if (s) {
            const int i = atoi(s);
	    if (destruct_level < i)
		destruct_level = i;
	}
    }
#endif

    if (PL_exit_flags & PERL_EXIT_DESTRUCT_END) {
        dJMPENV;
        int x = 0;

        JMPENV_PUSH(x);
	PERL_UNUSED_VAR(x);
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
	PL_veto_cleanup = TRUE;
        return STATUS_EXIT;
    }

#ifdef DEBUG_LEAKING_SCALARS_FORK_DUMP
    if (destruct_level != 0) {
	/* Fork here to create a child. Our child's job is to preserve the
	   state of scalars prior to destruction, so that we can instruct it
	   to dump any scalars that we later find have leaked.
	   There's no subtlety in this code - it assumes POSIX, and it doesn't
	   fail gracefully  */
	int fd[2];

	if(socketpair(AF_UNIX, SOCK_STREAM, 0, fd)) {
	    perror("Debug leaking scalars socketpair failed");
	    abort();
	}

	child = fork();
	if(child == -1) {
	    perror("Debug leaking scalars fork failed");
	    abort();
	}
	if (!child) {
	    /* We are the child */
	    const int sock = fd[1];
	    const int debug_fd = PerlIO_fileno(Perl_debug_log);
	    int f;
	    const char *where;
	    /* Our success message is an integer 0, and a char 0  */
	    static const char success[sizeof(int) + 1] = {0};

	    close(fd[0]);

	    /* We need to close all other file descriptors otherwise we end up
	       with interesting hangs, where the parent closes its end of a
	       pipe, and sits waiting for (another) child to terminate. Only
	       that child never terminates, because it never gets EOF, because
	       we also have the far end of the pipe open.  We even need to
	       close the debugging fd, because sometimes it happens to be one
	       end of a pipe, and a process is waiting on the other end for
	       EOF. Normally it would be closed at some point earlier in
	       destruction, but if we happen to cause the pipe to remain open,
	       EOF never occurs, and we get an infinite hang. Hence all the
	       games to pass in a file descriptor if it's actually needed.  */

	    f = sysconf(_SC_OPEN_MAX);
	    if(f < 0) {
		where = "sysconf failed";
		goto abort;
	    }
	    while (f--) {
		if (f == sock)
		    continue;
		close(f);
	    }

	    while (1) {
		SV *target;
		union control_un control;
		struct msghdr msg;
		struct iovec vec[1];
		struct cmsghdr *cmptr;
		ssize_t got;
		int got_fd;

		msg.msg_control = control.control;
		msg.msg_controllen = sizeof(control.control);
		/* We're a connected socket so we don't need a source  */
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = vec;
		msg.msg_iovlen = sizeof(vec)/sizeof(vec[0]);

		vec[0].iov_base = (void*)&target;
		vec[0].iov_len = sizeof(target);
      
		got = recvmsg(sock, &msg, 0);

		if(got == 0)
		    break;
		if(got < 0) {
		    where = "recv failed";
		    goto abort;
		}
		if(got < sizeof(target)) {
		    where = "short recv";
		    goto abort;
		}

		if(!(cmptr = CMSG_FIRSTHDR(&msg))) {
		    where = "no cmsg";
		    goto abort;
		}
		if(cmptr->cmsg_len != CMSG_LEN(sizeof(int))) {
		    where = "wrong cmsg_len";
		    goto abort;
		}
		if(cmptr->cmsg_level != SOL_SOCKET) {
		    where = "wrong cmsg_level";
		    goto abort;
		}
		if(cmptr->cmsg_type != SCM_RIGHTS) {
		    where = "wrong cmsg_type";
		    goto abort;
		}

		got_fd = *(int*)CMSG_DATA(cmptr);
		/* For our last little bit of trickery, put the file descriptor
		   back into Perl_debug_log, as if we never actually closed it
		*/
		if(got_fd != debug_fd) {
		    if (dup2(got_fd, debug_fd) == -1) {
			where = "dup2";
			goto abort;
		    }
		}
		sv_dump(target);

		PerlIO_flush(Perl_debug_log);

		got = write(sock, &success, sizeof(success));

		if(got < 0) {
		    where = "write failed";
		    goto abort;
		}
		if(got < sizeof(success)) {
		    where = "short write";
		    goto abort;
		}
	    }
	    _exit(0);
	abort:
	    {
		int send_errno = errno;
		unsigned char length = (unsigned char) strlen(where);
		struct iovec failure[3] = {
		    {(void*)&send_errno, sizeof(send_errno)},
		    {&length, 1},
		    {(void*)where, length}
		};
		int got = writev(sock, failure, 3);
		/* Bad news travels fast. Faster than data. We'll get a SIGPIPE
		   in the parent if we try to read from the socketpair after the
		   child has exited, even if there was data to read.
		   So sleep a bit to give the parent a fighting chance of
		   reading the data.  */
		sleep(2);
		_exit((got == -1) ? errno : 0);
	    }
	    /* End of child.  */
	}
	PL_dumper_fd = fd[0];
	close(fd[1]);
    }
#endif
    
    /* We must account for everything.  */

    /* Destroy the main CV and syntax tree */
    /* Do this now, because destroying ops can cause new SVs to be generated
       in Perl_pad_swipe, and when running with -DDEBUG_LEAKING_SCALARS they
       PL_curcop to point to a valid op from which the filename structure
       member is copied.  */
    PL_curcop = &PL_compiling;
    if (PL_main_root) {
	/* ensure comppad/curpad to refer to main's pad */
	if (CvPADLIST(PL_main_cv)) {
	    PAD_SET_CUR_NOSAVE(CvPADLIST(PL_main_cv), 1);
	}
	op_free(PL_main_root);
	PL_main_root = NULL;
    }
    PL_main_start = NULL;
    SvREFCNT_dec(PL_main_cv);
    PL_main_cv = NULL;
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
	if (PL_defoutgv && !SvREFCNT(PL_defoutgv))
	    PL_defoutgv = NULL; /* may have been freed */
    }

    /* unhook hooks which will soon be, or use, destroyed data */
    SvREFCNT_dec(PL_warnhook);
    PL_warnhook = NULL;
    SvREFCNT_dec(PL_diehook);
    PL_diehook = NULL;

    /* call exit list functions */
    while (PL_exitlistlen-- > 0)
	PL_exitlist[PL_exitlistlen].fn(aTHX_ PL_exitlist[PL_exitlistlen].ptr);

    Safefree(PL_exitlist);

    PL_exitlist = NULL;
    PL_exitlistlen = 0;

    SvREFCNT_dec(PL_registered_mros);

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

    if (destruct_level == 0) {

	DEBUG_P(debprofdump());

#if defined(PERLIO_LAYERS)
	/* No more IO - including error messages ! */
	PerlIO_cleanup(aTHX);
#endif

	CopFILE_free(&PL_compiling);
	CopSTASH_free(&PL_compiling);

	/* The exit() function will do everything that needs doing. */
        return STATUS_EXIT;
    }

    /* reset so print() ends up where we expect */
    setdefout(NULL);

#ifdef USE_ITHREADS
    /* the syntax tree is shared between clones
     * so op_free(PL_main_root) only ReREFCNT_dec's
     * REGEXPs in the parent interpreter
     * we need to manually ReREFCNT_dec for the clones
     */
    {
        I32 i = AvFILLp(PL_regex_padav) + 1;
        SV * const * const ary = AvARRAY(PL_regex_padav);

        while (i) {
            SV * const resv = ary[--i];

	    if(SvREPADTMP(resv)) {
	      SvREPADTMP_off(resv);
	    }
            else if(SvIOKp(resv)) {
		REGEXP *re = INT2PTR(REGEXP *,SvIVX(resv));
                ReREFCNT_dec(re);
            }
        }
    }
    SvREFCNT_dec(PL_regex_padav);
    PL_regex_padav = NULL;
    PL_regex_pad = NULL;
#endif

    SvREFCNT_dec(MUTABLE_SV(PL_stashcache));
    PL_stashcache = NULL;

    /* loosen bonds of global variables */

    /* XXX can PL_parser still be non-null here? */
    if(PL_parser && PL_parser->rsfp) {
	(void)PerlIO_close(PL_parser->rsfp);
	PL_parser->rsfp = NULL;
    }

    if (PL_minus_F) {
	Safefree(PL_splitstr);
	PL_splitstr = NULL;
    }

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
    PL_inplace = NULL;
    SvREFCNT_dec(PL_patchlevel);

    if (PL_e_script) {
	SvREFCNT_dec(PL_e_script);
	PL_e_script = NULL;
    }

    PL_perldb = 0;

    /* magical thingies */

    SvREFCNT_dec(PL_ofs_sv);	/* $, */
    PL_ofs_sv = NULL;

    SvREFCNT_dec(PL_ors_sv);	/* $\ */
    PL_ors_sv = NULL;

    SvREFCNT_dec(PL_rs);	/* $/ */
    PL_rs = NULL;

    Safefree(PL_osname);	/* $^O */
    PL_osname = NULL;

    SvREFCNT_dec(PL_statname);
    PL_statname = NULL;
    PL_statgv = NULL;

    /* defgv, aka *_ should be taken care of elsewhere */

    /* clean up after study() */
    SvREFCNT_dec(PL_lastscream);
    PL_lastscream = NULL;
    Safefree(PL_screamfirst);
    PL_screamfirst = 0;
    Safefree(PL_screamnext);
    PL_screamnext  = 0;

    /* float buffer */
    Safefree(PL_efloatbuf);
    PL_efloatbuf = NULL;
    PL_efloatsize = 0;

    /* startup and shutdown function lists */
    SvREFCNT_dec(PL_beginav);
    SvREFCNT_dec(PL_beginav_save);
    SvREFCNT_dec(PL_endav);
    SvREFCNT_dec(PL_checkav);
    SvREFCNT_dec(PL_checkav_save);
    SvREFCNT_dec(PL_unitcheckav);
    SvREFCNT_dec(PL_unitcheckav_save);
    SvREFCNT_dec(PL_initav);
    PL_beginav = NULL;
    PL_beginav_save = NULL;
    PL_endav = NULL;
    PL_checkav = NULL;
    PL_checkav_save = NULL;
    PL_unitcheckav = NULL;
    PL_unitcheckav_save = NULL;
    PL_initav = NULL;

    /* shortcuts just get cleared */
    PL_envgv = NULL;
    PL_incgv = NULL;
    PL_hintgv = NULL;
    PL_errgv = NULL;
    PL_argvgv = NULL;
    PL_argvoutgv = NULL;
    PL_stdingv = NULL;
    PL_stderrgv = NULL;
    PL_last_in_gv = NULL;
    PL_replgv = NULL;
    PL_DBgv = NULL;
    PL_DBline = NULL;
    PL_DBsub = NULL;
    PL_DBsingle = NULL;
    PL_DBtrace = NULL;
    PL_DBsignal = NULL;
    PL_DBcv = NULL;
    PL_dbargs = NULL;
    PL_debstash = NULL;

    SvREFCNT_dec(PL_argvout_stack);
    PL_argvout_stack = NULL;

    SvREFCNT_dec(PL_modglobal);
    PL_modglobal = NULL;
    SvREFCNT_dec(PL_preambleav);
    PL_preambleav = NULL;
    SvREFCNT_dec(PL_subname);
    PL_subname = NULL;
#ifdef PERL_USES_PL_PIDSTATUS
    SvREFCNT_dec(PL_pidstatus);
    PL_pidstatus = NULL;
#endif
    SvREFCNT_dec(PL_toptarget);
    PL_toptarget = NULL;
    SvREFCNT_dec(PL_bodytarget);
    PL_bodytarget = NULL;
    PL_formtarget = NULL;

    /* free locale stuff */
#ifdef USE_LOCALE_COLLATE
    Safefree(PL_collation_name);
    PL_collation_name = NULL;
#endif

#ifdef USE_LOCALE_NUMERIC
    Safefree(PL_numeric_name);
    PL_numeric_name = NULL;
    SvREFCNT_dec(PL_numeric_radix_sv);
    PL_numeric_radix_sv = NULL;
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
    PL_utf8_alnum	= NULL;
    PL_utf8_alnumc	= NULL;
    PL_utf8_ascii	= NULL;
    PL_utf8_alpha	= NULL;
    PL_utf8_space	= NULL;
    PL_utf8_cntrl	= NULL;
    PL_utf8_graph	= NULL;
    PL_utf8_digit	= NULL;
    PL_utf8_upper	= NULL;
    PL_utf8_lower	= NULL;
    PL_utf8_print	= NULL;
    PL_utf8_punct	= NULL;
    PL_utf8_xdigit	= NULL;
    PL_utf8_mark	= NULL;
    PL_utf8_toupper	= NULL;
    PL_utf8_totitle	= NULL;
    PL_utf8_tolower	= NULL;
    PL_utf8_tofold	= NULL;
    PL_utf8_idstart	= NULL;
    PL_utf8_idcont	= NULL;

    if (!specialWARN(PL_compiling.cop_warnings))
	PerlMemShared_free(PL_compiling.cop_warnings);
    PL_compiling.cop_warnings = NULL;
    Perl_refcounted_he_free(aTHX_ PL_compiling.cop_hints_hash);
    PL_compiling.cop_hints_hash = NULL;
    CopFILE_free(&PL_compiling);
    CopSTASH_free(&PL_compiling);

    /* Prepare to destruct main symbol table.  */

    hv = PL_defstash;
    PL_defstash = 0;
    SvREFCNT_dec(hv);
    SvREFCNT_dec(PL_curstname);
    PL_curstname = NULL;

    /* clear queued errors */
    SvREFCNT_dec(PL_errors);
    PL_errors = NULL;

    SvREFCNT_dec(PL_isarev);

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

    /* the 2 is for PL_fdpid and PL_strtab */
    while (sv_clean_all() > 2)
	;

    AvREAL_off(PL_fdpid);		/* no surviving entries */
    SvREFCNT_dec(PL_fdpid);		/* needed in io_close() */
    PL_fdpid = NULL;

#ifdef HAVE_INTERP_INTERN
    sys_intern_clear();
#endif

    /* Destruct the global string table. */
    {
	/* Yell and reset the HeVAL() slots that are still holding refcounts,
	 * so that sv_free() won't fail on them.
	 * Now that the global string table is using a single hunk of memory
	 * for both HE and HEK, we either need to explicitly unshare it the
	 * correct way, or actually free things here.
	 */
	I32 riter = 0;
	const I32 max = HvMAX(PL_strtab);
	HE * const * const array = HvARRAY(PL_strtab);
	HE *hent = array[0];

	for (;;) {
	    if (hent && ckWARN_d(WARN_INTERNAL)) {
		HE * const next = HeNEXT(hent);
		Perl_warner(aTHX_ packWARN(WARN_INTERNAL),
		     "Unbalanced string table refcount: (%ld) for \"%s\"",
		     (long)hent->he_valu.hent_refcount, HeKEY(hent));
		Safefree(hent);
		hent = next;
	    }
	    if (!hent) {
		if (++riter > max)
		    break;
		hent = array[riter];
	    }
	}

	Safefree(array);
	HvARRAY(PL_strtab) = 0;
	HvTOTALKEYS(PL_strtab) = 0;
	HvFILL(PL_strtab) = 0;
    }
    SvREFCNT_dec(PL_strtab);

#ifdef USE_ITHREADS
    /* free the pointer tables used for cloning */
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

	for (sva = PL_sv_arenaroot; sva; sva = MUTABLE_SV(SvANY(sva))) {
	    svend = &sva[SvREFCNT(sva)];
	    for (sv = sva + 1; sv < svend; ++sv) {
		if (SvTYPE(sv) != SVTYPEMASK) {
		    PerlIO_printf(Perl_debug_log, "leaked: sv=0x%p"
			" flags=0x%"UVxf
			" refcnt=%"UVuf pTHX__FORMAT "\n"
			"\tallocated at %s:%d %s %s%s\n",
			(void*)sv, (UV)sv->sv_flags, (UV)sv->sv_refcnt
			pTHX__VALUE,
			sv->sv_debug_file ? sv->sv_debug_file : "(unknown)",
			sv->sv_debug_line,
			sv->sv_debug_inpad ? "for" : "by",
			sv->sv_debug_optype ?
			    PL_op_name[sv->sv_debug_optype]: "(none)",
			sv->sv_debug_cloned ? " (cloned)" : ""
		    );
#ifdef DEBUG_LEAKING_SCALARS_FORK_DUMP
		    Perl_dump_sv_child(aTHX_ sv);
#endif
		}
	    }
	}
    }
#ifdef DEBUG_LEAKING_SCALARS_FORK_DUMP
    {
	int status;
	fd_set rset;
	/* Wait for up to 4 seconds for child to terminate.
	   This seems to be the least effort way of timing out on reaping
	   its exit status.  */
	struct timeval waitfor = {4, 0};
	int sock = PL_dumper_fd;

	shutdown(sock, 1);
	FD_ZERO(&rset);
	FD_SET(sock, &rset);
	select(sock + 1, &rset, NULL, NULL, &waitfor);
	waitpid(child, &status, WNOHANG);
	close(sock);
    }
#endif
#endif
#ifdef DEBUG_LEAKING_SCALARS_ABORT
    if (PL_sv_count)
	abort();
#endif
    PL_sv_count = 0;

#ifdef PERL_DEBUG_READONLY_OPS
    free(PL_slabs);
    PL_slabs = NULL;
    PL_slab_count = 0;
#endif

#if defined(PERLIO_LAYERS)
    /* No more IO - including error messages ! */
    PerlIO_cleanup(aTHX);
#endif

    /* sv_undef needs to stay immortal until after PerlIO_cleanup
       as currently layers use it rather than NULL as a marker
       for no arg - and will try and SvREFCNT_dec it.
     */
    SvREFCNT(&PL_sv_undef) = 0;
    SvREADONLY_off(&PL_sv_undef);

    Safefree(PL_origfilename);
    PL_origfilename = NULL;
    Safefree(PL_reg_start_tmp);
    PL_reg_start_tmp = (char**)NULL;
    PL_reg_start_tmpl = 0;
    Safefree(PL_reg_curpm);
    Safefree(PL_reg_poscache);
    free_tied_hv_pool();
    Safefree(PL_op_mask);
    Safefree(PL_psig_ptr);
    PL_psig_ptr = (SV**)NULL;
    Safefree(PL_psig_name);
    PL_psig_name = (SV**)NULL;
    Safefree(PL_bitcount);
    PL_bitcount = NULL;
    Safefree(PL_psig_pend);
    PL_psig_pend = (int*)NULL;
    PL_formfeed = NULL;
    nuke_stacks();
    PL_tainting = FALSE;
    PL_taint_warn = FALSE;
    PL_hints = 0;		/* Reset hints. Should hints be per-interpreter ? */
    PL_debug = 0;

    DEBUG_P(debprofdump());

#ifdef USE_REENTRANT_API
    Perl_reentrant_free(aTHX);
#endif

    sv_free_arenas();

    while (PL_regmatch_slab) {
	regmatch_slab  *s = PL_regmatch_slab;
	PL_regmatch_slab = PL_regmatch_slab->next;
	Safefree(s);
    }

    /* As the absolutely last thing, free the non-arena SV for mess() */

    if (PL_mess_sv) {
	/* we know that type == SVt_PVMG */

	/* it could have accumulated taint magic */
	MAGIC* mg;
	MAGIC* moremagic;
	for (mg = SvMAGIC(PL_mess_sv); mg; mg = moremagic) {
	    moremagic = mg->mg_moremagic;
	    if (mg->mg_ptr && mg->mg_type != PERL_MAGIC_regex_global
		&& mg->mg_len >= 0)
		Safefree(mg->mg_ptr);
	    Safefree(mg);
	}

	/* we know that type >= SVt_PV */
	SvPV_free(PL_mess_sv);
	Safefree(SvANY(PL_mess_sv));
	Safefree(PL_mess_sv);
	PL_mess_sv = NULL;
    }
    return STATUS_EXIT;
}

/*
=for apidoc perl_free

Releases a Perl interpreter.  See L<perlembed>.

=cut
*/

void
perl_free(pTHXx)
{
    dVAR;

    PERL_ARGS_ASSERT_PERL_FREE;

    if (PL_veto_cleanup)
	return;

#ifdef PERL_TRACK_MEMPOOL
    {
	/*
	 * Don't free thread memory if PERL_DESTRUCT_LEVEL is set to a non-zero
	 * value as we're probably hunting memory leaks then
	 */
	const char * const s = PerlEnv_getenv("PERL_DESTRUCT_LEVEL");
	if (!s || atoi(s) == 0) {
	    const U32 old_debug = PL_debug;
	    /* Emulate the PerlHost behaviour of free()ing all memory allocated in this
	       thread at thread exit.  */
	    if (DEBUG_m_TEST) {
		PerlIO_puts(Perl_debug_log, "Disabling memory debugging as we "
			    "free this thread's memory\n");
		PL_debug &= ~ DEBUG_m_FLAG;
	    }
	    while(aTHXx->Imemory_debug_header.next != &(aTHXx->Imemory_debug_header))
		safesysfree(sTHX + (char *)(aTHXx->Imemory_debug_header.next));
	    PL_debug = old_debug;
	}
    }
#endif

#if defined(WIN32) || defined(NETWARE)
#  if defined(PERL_IMPLICIT_SYS)
    {
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
    }
#  else
    PerlMem_free(aTHXx);
#  endif
#else
    PerlMem_free(aTHXx);
#endif
}

#if defined(USE_ITHREADS)
/* provide destructors to clean up the thread key when libperl is unloaded */
#ifndef WIN32 /* handled during DLL_PROCESS_DETACH in win32/perllib.c */

#if defined(__hpux) && !(defined(__ux_version) && __ux_version <= 1020) && !defined(__GNUC__)
#pragma fini "perl_fini"
#elif defined(__sun) && !defined(__GNUC__)
#pragma fini (perl_fini)
#endif

static void
#if defined(__GNUC__)
__attribute__((destructor))
#endif
perl_fini(void)
{
    dVAR;
    if (PL_curinterp  && !PL_veto_cleanup)
	FREE_THREAD_KEY;
}

#endif /* WIN32 */
#endif /* THREADS */

void
Perl_call_atexit(pTHX_ ATEXIT_t fn, void *ptr)
{
    dVAR;
    Renew(PL_exitlist, PL_exitlistlen+1, PerlExitListEntry);
    PL_exitlist[PL_exitlistlen].fn = fn;
    PL_exitlist[PL_exitlistlen].ptr = ptr;
    ++PL_exitlistlen;
}

#ifdef HAS_PROCSELFEXE
/* This is a function so that we don't hold on to MAXPATHLEN
   bytes of stack longer than necessary
 */
STATIC void
S_procself_val(pTHX_ SV *sv, const char *arg0)
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
S_set_caret_X(pTHX) {
    dVAR;
    GV* tmpgv = gv_fetchpvs("\030", GV_ADD|GV_NOTQUAL, SVt_PV); /* $^X */
    if (tmpgv) {
#ifdef HAS_PROCSELFEXE
	S_procself_val(aTHX_ GvSV(tmpgv), PL_origargv[0]);
#else
#ifdef OS2
	sv_setpv(GvSVn(tmpgv), os2_execname(aTHX));
#else
	sv_setpv(GvSVn(tmpgv),PL_origargv[0]);
#endif
#endif
    }
}

/*
=for apidoc perl_parse

Tells a Perl interpreter to parse a Perl script.  See L<perlembed>.

=cut
*/

int
perl_parse(pTHXx_ XSINIT_t xsinit, int argc, char **argv, char **env)
{
    dVAR;
    I32 oldscope;
    int ret;
    dJMPENV;

    PERL_ARGS_ASSERT_PERL_PARSE;
#ifndef MULTIPLICITY
    PERL_UNUSED_ARG(my_perl);
#endif

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW_AND_IAMSUID
    Perl_croak(aTHX_ "suidperl is no longer needed since the kernel can now "
	       "execute\nsetuid perl scripts securely.\n");
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
	const char * const s = PerlEnv_getenv("PERL_HASH_SEED_DEBUG");

	if (s && (atoi(s) == 1))
	    PerlIO_printf(Perl_debug_log, "HASH_SEED = %"UVuf"\n", PL_rehash_seed);
    }
#endif /* #if defined(USE_HASH_SEED) || defined(USE_HASH_SEED_EXPLICIT) */

    PL_origargc = argc;
    PL_origargv = argv;

    if (PL_origalen != 0) {
	PL_origalen = 1; /* don't use old PL_origalen if perl_parse() is called again */
    }
    else {
	/* Set PL_origalen be the sum of the contiguous argv[]
	 * elements plus the size of the env in case that it is
	 * contiguous with the argv[].  This is used in mg.c:Perl_magic_set()
	 * as the maximum modifiable length of $0.  In the worst case
	 * the area we are able to modify is limited to the size of
	 * the original argv[0].  (See below for 'contiguous', though.)
	 * --jhi */
	 const char *s = NULL;
	 int i;
	 const UV mask =
	   ~(UV)(PTRSIZE == 4 ? 3 : PTRSIZE == 8 ? 7 : PTRSIZE == 16 ? 15 : 0);
         /* Do the mask check only if the args seem like aligned. */
	 const UV aligned =
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

#ifndef PERL_USE_SAFE_PUTENV
	 /* Can we grab env area too to be used as the area for $0? */
	 if (s && PL_origenviron && !PL_use_safe_putenv) {
	      if ((PL_origenviron[0] == s + 1)
		  ||
		  (aligned &&
		   (PL_origenviron[0] >  s &&
		    PL_origenviron[0] <=
		    INT2PTR(char *, PTR2UV(s + PTRSIZE) & mask)))
		 )
	      {
#ifndef OS2		/* ENVIRON is read by the kernel too. */
		   s = PL_origenviron[0];
		   while (*s) s++;
#endif
		   my_setenv("NoNe  SuCh", NULL);
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
#endif /* !defined(PERL_USE_SAFE_PUTENV) */

	 PL_origalen = s ? s - PL_origargv[0] + 1 : 0;
    }

    if (PL_do_undump) {

	/* Come here if running an undumped a.out. */

	PL_origfilename = savepv(argv[0]);
	PL_do_undump = FALSE;
	cxstack_ix = -1;		/* start label stack again */
	init_ids();
	assert (!PL_tainted);
	TAINT;
	S_set_caret_X(aTHX);
	TAINT_NOT;
	init_postdump_symbols(argc,argv,env);
	return 0;
    }

    if (PL_main_root) {
	op_free(PL_main_root);
	PL_main_root = NULL;
    }
    PL_main_start = NULL;
    SvREFCNT_dec(PL_main_cv);
    PL_main_cv = NULL;

    time(&PL_basetime);
    oldscope = PL_scopestack_ix;
    PL_dowarn = G_WARN_OFF;

    JMPENV_PUSH(ret);
    switch (ret) {
    case 0:
	parse_body(env,xsinit);
	if (PL_unitcheckav)
	    call_list(oldscope, PL_unitcheckav);
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
	if (PL_unitcheckav)
	    call_list(oldscope, PL_unitcheckav);
	if (PL_checkav)
	    call_list(oldscope, PL_checkav);
	ret = STATUS_EXIT;
	break;
    case 3:
	PerlIO_printf(Perl_error_log, "panic: top_env\n");
	ret = 1;
	break;
    }
    JMPENV_POP;
    return ret;
}

STATIC void *
S_parse_body(pTHX_ char **env, XSINIT_t xsinit)
{
    dVAR;
    PerlIO *rsfp;
    int argc = PL_origargc;
    char **argv = PL_origargv;
    const char *scriptname = NULL;
    VOL bool dosearch = FALSE;
#ifdef DOSUID
    const char *validarg = "";
#endif
    register SV *sv;
    register char c;
    const char *cddir = NULL;
#ifdef USE_SITECUSTOMIZE
    bool minus_f = FALSE;
#endif
    SV *linestr_sv = newSV_type(SVt_PVIV);
    bool add_read_e_script = FALSE;

    SvGROW(linestr_sv, 80);
    sv_setpvs(linestr_sv,"");

    sv = newSVpvs("");		/* first used for -I flags */
    SAVEFREESV(sv);
    init_main_stash();

    {
	const char *s;
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
	switch ((c = *s)) {
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

	case 'E':
	    PL_minus_E = TRUE;
	    /* FALL THROUGH */
	case 'e':
#ifdef MACOS_TRADITIONAL
	    /* ignore -e for Dev:Pseudo argument */
	    if (argv[1] && !strcmp(argv[1], "Dev:Pseudo"))
		break;
#endif
	    forbid_setid('e', FALSE);
	    if (!PL_e_script) {
		PL_e_script = newSVpvs("");
		add_read_e_script = TRUE;
	    }
	    if (*++s)
		sv_catpv(PL_e_script, s);
	    else if (argv[1]) {
		sv_catpv(PL_e_script, argv[1]);
		argc--,argv++;
	    }
	    else
		Perl_croak(aTHX_ "No code specified for -%c", c);
	    sv_catpvs(PL_e_script, "\n");
	    break;

	case 'f':
#ifdef USE_SITECUSTOMIZE
	    minus_f = TRUE;
#endif
	    s++;
	    goto reswitch;

	case 'I':	/* -I handled both here and in moreswitches() */
	    forbid_setid('I', FALSE);
	    if (!*++s && (s=argv[1]) != NULL) {
		argc--,argv++;
	    }
	    if (s && *s) {
		STRLEN len = strlen(s);
		const char * const p = savepvn(s, len);
		incpush(p, TRUE, TRUE, FALSE, FALSE);
		sv_catpvs(sv, "-I");
		sv_catpvn(sv, p, len);
		sv_catpvs(sv, " ");
		Safefree(p);
	    }
	    else
		Perl_croak(aTHX_ "No directory specified for -I");
	    break;
	case 'P':
	    forbid_setid('P', FALSE);
	    PL_preprocess = TRUE;
	    s++;
	    deprecate("-P");
	    goto reswitch;
	case 'S':
	    forbid_setid('S', FALSE);
	    dosearch = TRUE;
	    s++;
	    goto reswitch;
	case 'V':
	    {
		SV *opts_prog;

		Perl_av_create_and_push(aTHX_ &PL_preambleav, newSVpvs("use Config;"));
		if (*++s != ':')  {
		    /* Can't do newSVpvs() as that would involve pre-processor
		       condititionals inside a macro expansion.  */
		    opts_prog = Perl_newSVpv(aTHX_ "$_ = join ' ', sort qw("
#  ifdef DEBUGGING
			     " DEBUGGING"
#  endif
#  ifdef NO_MATHOMS
                            " NO_MATHOMS"
#  endif
#  ifdef PERL_DONT_CREATE_GVSV
			     " PERL_DONT_CREATE_GVSV"
#  endif
#  ifdef PERL_MALLOC_WRAP
			     " PERL_MALLOC_WRAP"
#  endif
#  ifdef PERL_MEM_LOG
			     " PERL_MEM_LOG"
#  endif
#  ifdef PERL_MEM_LOG_ENV
			     " PERL_MEM_LOG_ENV"
#  endif
#  ifdef PERL_MEM_LOG_ENV_FD
			     " PERL_MEM_LOG_ENV_FD"
#  endif
#  ifdef PERL_MEM_LOG_STDERR
			     " PERL_MEM_LOG_STDERR"
#  endif
#  ifdef PERL_MEM_LOG_TIMESTAMP
			     " PERL_MEM_LOG_TIMESTAMP"
#  endif
#  ifdef PERL_USE_DEVEL
			     " PERL_USE_DEVEL"
#  endif
#  ifdef PERL_USE_SAFE_PUTENV
			     " PERL_USE_SAFE_PUTENV"
#  endif
#  ifdef USE_SITECUSTOMIZE
			     " USE_SITECUSTOMIZE"
#  endif	       
#  ifdef USE_FAST_STDIO
			     " USE_FAST_STDIO"
#  endif	       
					     , 0);

		    sv_catpv(opts_prog, PL_bincompat_options);
		    /* Terminate the qw(, and then wrap at 76 columns.  */
		    sv_catpvs(opts_prog, "); s/(?=.{53})(.{1,53}) /$1\\n                        /mg;print Config::myconfig(),");
#ifdef VMS
		    sv_catpvs(opts_prog,"\"\\nCharacteristics of this PERLSHR image: \\n");
#else
		    sv_catpvs(opts_prog,"\"\\nCharacteristics of this binary (from libperl): \\n");
#endif
		    sv_catpvs(opts_prog,"  Compile-time options: $_\\n\",");

#if defined(LOCAL_PATCH_COUNT)
		    if (LOCAL_PATCH_COUNT > 0) {
			int i;
			sv_catpvs(opts_prog,
				 "\"  Locally applied patches:\\n\",");
			for (i = 1; i <= LOCAL_PATCH_COUNT; i++) {
			    if (PL_localpatches[i])
				Perl_sv_catpvf(aTHX_ opts_prog,"q%c\t%s\n%c,",
				    0, PL_localpatches[i], 0);
			}
		    }
#endif
#ifdef __OpenBSD__
		    Perl_sv_catpvf(aTHX_ opts_prog,
				   "\"  Built under OpenBSD\\n\"");
#else
		    Perl_sv_catpvf(aTHX_ opts_prog,
				   "\"  Built under %s\\n",OSNAME);
#ifdef __DATE__
#  ifdef __TIME__
		    sv_catpvs(opts_prog,
			      "  Compiled at " __DATE__ " " __TIME__ "\\n\"");
#  else
		    sv_catpvs(opts_prog, "  Compiled on " __DATE__ "\\n\"");
#  endif
#endif
#endif
		    sv_catpvs(opts_prog, "; $\"=\"\\n    \"; "
			     "@env = map { \"$_=\\\"$ENV{$_}\\\"\" } "
			     "sort grep {/^PERL/} keys %ENV; ");
#ifdef __CYGWIN__
		    sv_catpvs(opts_prog,
			     "push @env, \"CYGWIN=\\\"$ENV{CYGWIN}\\\"\";");
#endif
		    sv_catpvs(opts_prog, 
			     "print \"  \\%ENV:\\n    @env\\n\" if @env;"
			     "print \"  \\@INC:\\n    @INC\\n\";");
		}
		else {
		    ++s;
		    opts_prog = Perl_newSVpvf(aTHX_
					      "Config::config_vars(qw%c%s%c)",
					      0, s, 0);
		    s += strlen(s);
		}
		av_push(PL_preambleav, opts_prog);
		/* don't look for script or read stdin */
		scriptname = BIT_BUCKET;
		goto reswitch;
	    }
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
		s = (char *)"v";
		goto reswitch;
	    }
	    if (strEQ(s, "help")) {
		s = (char *)"h";
		goto reswitch;
	    }
	    s--;
	    /* FALL THROUGH */
	default:
	    Perl_croak(aTHX_ "Unrecognized switch: -%s  (-h will show valid options)",s);
	}
    }
    }

  switch_end:

    {
	char *s;

    if (
#ifndef SECURE_INTERNAL_GETENV
        !PL_tainting &&
#endif
	(s = PerlEnv_getenv("PERL5OPT")))
    {
	while (isSPACE(*s))
	    s++;
	if (*s == '-' && *(s+1) == 'T') {
	    CHECK_MALLOC_TOO_LATE_FOR('T');
	    PL_tainting = TRUE;
            PL_taint_warn = FALSE;
	}
	else {
	    char *popt_copy = NULL;
	    while (s && *s) {
	        const char *d;
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
		if (!strchr("CDIMUdmtw", *s))
		    Perl_croak(aTHX_ "Illegal switch in PERL5OPT: -%c", *s);
		while (++s && *s) {
		    if (isSPACE(*s)) {
			if (!popt_copy) {
			    popt_copy = SvPVX(sv_2mortal(newSVpv(d,0)));
			    s = popt_copy + (s - d);
			    d = popt_copy;
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
    }

#ifdef USE_SITECUSTOMIZE
    if (!minus_f) {
	(void)Perl_av_create_and_unshift_one(aTHX_ &PL_preambleav,
					     Perl_newSVpvf(aTHX_ "BEGIN { do '%s/sitecustomize.pl' }", SITELIB_EXP));
    }
#endif

    if (!scriptname)
	scriptname = argv[0];
    if (PL_e_script) {
	argc++,argv--;
	scriptname = BIT_BUCKET;	/* don't look for script or read stdin */
    }
    else if (scriptname == NULL) {
#ifdef MSDOS
	if ( PerlLIO_isatty(PerlIO_fileno(PerlIO_stdin())) )
	    moreswitches("h");
#endif
	scriptname = "-";
    }

    /* Set $^X early so that it can be used for relocatable paths in @INC  */
    assert (!PL_tainted);
    TAINT;
    S_set_caret_X(aTHX);
    TAINT_NOT;
    init_perllib();

    {
	bool suidscript = FALSE;

#ifdef DOSUID
	const int fdscript =
#endif
	    open_script(scriptname, dosearch, sv, &suidscript, &rsfp);

	validate_suid(validarg, scriptname, fdscript, suidscript,
		linestr_sv, rsfp);

#ifndef PERL_MICRO
#  if defined(SIGCHLD) || defined(SIGCLD)
	{
#  ifndef SIGCHLD
#    define SIGCHLD SIGCLD
#  endif
	    Sighandler_t sigstate = rsignal_state(SIGCHLD);
	    if (sigstate == (Sighandler_t) SIG_IGN) {
		if (ckWARN(WARN_SIGNAL))
		    Perl_warner(aTHX_ packWARN(WARN_SIGNAL),
				"Can't ignore signal CHLD, forcing to default");
		(void)rsignal(SIGCHLD, (Sighandler_t)SIG_DFL);
	    }
	}
#  endif
#endif

	if (PL_doextract
#ifdef MACOS_TRADITIONAL
	    || gMacPerl_AlwaysExtract
#endif
	    ) {

	    /* This will croak if suidscript is true, as -x cannot be used with
	       setuid scripts.  */
	    forbid_setid('x', suidscript);
	    /* Hence you can't get here if suidscript is true */

	    find_beginning(linestr_sv, rsfp);
	    if (cddir && PerlDir_chdir( (char *)cddir ) < 0)
		Perl_croak(aTHX_ "Can't chdir to %s",cddir);
	}
    }

    PL_main_cv = PL_compcv = MUTABLE_CV(newSV_type(SVt_PVCV));
    CvUNIQUE_on(PL_compcv);

    CvPADLIST(PL_compcv) = pad_new(0);

    PL_isarev = newHV();

    boot_core_PerlIO();
    boot_core_UNIVERSAL();
    boot_core_xsutils();
    boot_core_mro();

    if (xsinit)
	(*xsinit)(aTHX);	/* in case linked C routines want magical variables */
#ifndef PERL_MICRO
#if defined(VMS) || defined(WIN32) || defined(DJGPP) || defined(__CYGWIN__) || defined(EPOC) || defined(SYMBIAN)
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

    /* PL_unicode is turned on by -C, or by $ENV{PERL_UNICODE},
     * or explicitly in some platforms.
     * locale.c:Perl_init_i18nl10n() if the environment
     * look like the user wants to use UTF-8. */
#if defined(__SYMBIAN32__)
    PL_unicode = PERL_UNICODE_STD_FLAG; /* See PERL_SYMBIAN_CONSOLE_UTF8. */
#endif
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
		  (sv = GvSV(gv_fetchpvs("\017PEN", GV_ADD|GV_NOTQUAL,
					 SVt_PV)))) {
		   U32 in  = PL_unicode & PERL_UNICODE_IN_FLAG;
		   U32 out = PL_unicode & PERL_UNICODE_OUT_FLAG;
		   if (in) {
			if (out)
			     sv_setpvs(sv, ":utf8\0:utf8");
			else
			     sv_setpvs(sv, ":utf8\0");
		   }
		   else if (out)
			sv_setpvs(sv, "\0:utf8");
		   SvSETMAGIC(sv);
	      }
	 }
    }

    {
	const char *s;
    if ((s = PerlEnv_getenv("PERL_SIGNALS"))) {
	 if (strEQ(s, "unsafe"))
	      PL_signals |=  PERL_SIGNALS_UNSAFE_FLAG;
	 else if (strEQ(s, "safe"))
	      PL_signals &= ~PERL_SIGNALS_UNSAFE_FLAG;
	 else
	      Perl_croak(aTHX_ "PERL_SIGNALS illegal: \"%s\"", s);
    }
    }

#ifdef PERL_MAD
    {
	const char *s;
    if ((s = PerlEnv_getenv("PERL_XMLDUMP"))) {
	PL_madskills = 1;
	PL_minus_c = 1;
	if (!s || !s[0])
	    PL_xmlfp = PerlIO_stdout();
	else {
	    PL_xmlfp = PerlIO_open(s, "w");
	    if (!PL_xmlfp)
		Perl_croak(aTHX_ "Can't open %s", s);
	}
	my_setenv("PERL_XMLDUMP", NULL);	/* hide from subprocs */
    }
    }

    {
	const char *s;
    if ((s = PerlEnv_getenv("PERL_MADSKILLS"))) {
	PL_madskills = atoi(s);
	my_setenv("PERL_MADSKILLS", NULL);	/* hide from subprocs */
    }
    }
#endif

    lex_start(linestr_sv, rsfp, TRUE);
    PL_subname = newSVpvs("main");

    if (add_read_e_script)
	filter_add(read_e_script, NULL);

    /* now parse the script */

    SETERRNO(0,SS_NORMAL);
#ifdef MACOS_TRADITIONAL
    if (gMacPerl_SyntaxError = (yyparse() || PL_parser->error_count)) {
	if (PL_minus_c)
	    Perl_croak(aTHX_ "%s had compilation errors.\n", MacPerl_MPWFileName(PL_origfilename));
	else {
	    Perl_croak(aTHX_ "Execution of %s aborted due to compilation errors.\n",
		       MacPerl_MPWFileName(PL_origfilename));
	}
    }
#else
    if (yyparse() || PL_parser->error_count) {
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
	PL_e_script = NULL;
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
    {
	const char *s;
    if ((s=PerlEnv_getenv("PERL_DEBUG_MSTATS")) && atoi(s) >= 2)
	dump_mstats("after compilation:");
    }
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
    dVAR;
    I32 oldscope;
    int ret = 0;
    dJMPENV;

    PERL_ARGS_ASSERT_PERL_RUN;
#ifndef MULTIPLICITY
    PERL_UNUSED_ARG(my_perl);
#endif

    oldscope = PL_scopestack_ix;
#ifdef VMS
    VMSISH_HUSHED = 0;
#endif

    JMPENV_PUSH(ret);
    switch (ret) {
    case 1:
	cxstack_ix = -1;		/* start context stack again */
	goto redo_body;
    case 0:				/* normal completion */
 redo_body:
	run_body(oldscope);
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
	ret = STATUS_EXIT;
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

STATIC void
S_run_body(pTHX_ I32 oldscope)
{
    dVAR;
    DEBUG_r(PerlIO_printf(Perl_debug_log, "%s $` $& $' support.\n",
                    PL_sawampersand ? "Enabling" : "Omitting"));

    if (!PL_restartop) {
#ifdef PERL_MAD
	if (PL_xmlfp) {
	    xmldump_all();
	    exit(0);	/* less likely to core dump than my_exit(0) */
	}
#endif
	DEBUG_x(dump_all());
#ifdef DEBUGGING
	if (!DEBUG_q_TEST)
	  PERL_DEBUG(PerlIO_printf(Perl_debug_log, "\nEXECUTING...\n\n"));
#endif
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
#ifdef PERL_DEBUG_READONLY_OPS
	Perl_pending_Slabs_to_ro(aTHX);
#endif
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
}

/*
=head1 SV Manipulation Functions

=for apidoc p||get_sv

Returns the SV of the specified Perl scalar.  C<flags> are passed to
C<gv_fetchpv>. If C<GV_ADD> is set and the
Perl variable does not exist then it will be created.  If C<flags> is zero
and the variable does not exist then NULL is returned.

=cut
*/

SV*
Perl_get_sv(pTHX_ const char *name, I32 flags)
{
    GV *gv;

    PERL_ARGS_ASSERT_GET_SV;

    gv = gv_fetchpv(name, flags, SVt_PV);
    if (gv)
	return GvSV(gv);
    return NULL;
}

/*
=head1 Array Manipulation Functions

=for apidoc p||get_av

Returns the AV of the specified Perl array.  C<flags> are passed to
C<gv_fetchpv>. If C<GV_ADD> is set and the
Perl variable does not exist then it will be created.  If C<flags> is zero
and the variable does not exist then NULL is returned.

=cut
*/

AV*
Perl_get_av(pTHX_ const char *name, I32 flags)
{
    GV* const gv = gv_fetchpv(name, flags, SVt_PVAV);

    PERL_ARGS_ASSERT_GET_AV;

    if (flags)
    	return GvAVn(gv);
    if (gv)
	return GvAV(gv);
    return NULL;
}

/*
=head1 Hash Manipulation Functions

=for apidoc p||get_hv

Returns the HV of the specified Perl hash.  C<flags> are passed to
C<gv_fetchpv>. If C<GV_ADD> is set and the
Perl variable does not exist then it will be created.  If C<flags> is zero
and the variable does not exist then NULL is returned.

=cut
*/

HV*
Perl_get_hv(pTHX_ const char *name, I32 flags)
{
    GV* const gv = gv_fetchpv(name, flags, SVt_PVHV);

    PERL_ARGS_ASSERT_GET_HV;

    if (flags)
    	return GvHVn(gv);
    if (gv)
	return GvHV(gv);
    return NULL;
}

/*
=head1 CV Manipulation Functions

=for apidoc p||get_cvn_flags

Returns the CV of the specified Perl subroutine.  C<flags> are passed to
C<gv_fetchpvn_flags>. If C<GV_ADD> is set and the Perl subroutine does not
exist then it will be declared (which has the same effect as saying
C<sub name;>).  If C<GV_ADD> is not set and the subroutine does not exist
then NULL is returned.

=for apidoc p||get_cv

Uses C<strlen> to get the length of C<name>, then calls C<get_cvn_flags>.

=cut
*/

CV*
Perl_get_cvn_flags(pTHX_ const char *name, STRLEN len, I32 flags)
{
    GV* const gv = gv_fetchpvn_flags(name, len, flags, SVt_PVCV);
    /* XXX this is probably not what they think they're getting.
     * It has the same effect as "sub name;", i.e. just a forward
     * declaration! */

    PERL_ARGS_ASSERT_GET_CVN_FLAGS;

    if ((flags & ~GV_NOADD_MASK) && !GvCVu(gv)) {
	SV *const sv = newSVpvn_flags(name, len, flags & SVf_UTF8);
    	return newSUB(start_subparse(FALSE, 0),
		      newSVOP(OP_CONST, 0, sv),
		      NULL, NULL);
    }
    if (gv)
	return GvCVu(gv);
    return NULL;
}

CV*
Perl_get_cv(pTHX_ const char *name, I32 flags)
{
    PERL_ARGS_ASSERT_GET_CV;

    return get_cvn_flags(name, strlen(name), flags);
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
    dVAR;
    dSP;

    PERL_ARGS_ASSERT_CALL_ARGV;

    PUSHMARK(SP);
    if (argv) {
	while (*argv) {
	    mXPUSHs(newSVpv(*argv,0));
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
    PERL_ARGS_ASSERT_CALL_PV;

    return call_sv(MUTABLE_SV(get_cv(sub_name, GV_ADD)), flags);
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
    PERL_ARGS_ASSERT_CALL_METHOD;

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
Perl_call_sv(pTHX_ SV *sv, VOL I32 flags)
          		/* See G_* flags in cop.h */
{
    dVAR; dSP;
    LOGOP myop;		/* fake syntax tree node */
    UNOP method_op;
    I32 oldmark;
    VOL I32 retval = 0;
    I32 oldscope;
    bool oldcatch = CATCH_GET;
    int ret;
    OP* const oldop = PL_op;
    dJMPENV;

    PERL_ARGS_ASSERT_CALL_SV;

    if (flags & G_DISCARD) {
	ENTER;
	SAVETMPS;
    }

    Zero(&myop, 1, LOGOP);
    myop.op_next = NULL;
    if (!(flags & G_NOARGS))
	myop.op_flags |= OPf_STACKED;
    myop.op_flags |= OP_GIMME_REVERSE(flags);
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
	  && (SvTYPE(sv) != SVt_PVCV || CvSTASH((const CV *)sv) != PL_debstash)
	  && !(flags & G_NODEBUG))
	PL_op->op_private |= OPpENTERSUB_DB;

    if (flags & G_METHOD) {
	Zero(&method_op, 1, UNOP);
	method_op.op_next = PL_op;
	method_op.op_ppaddr = PL_ppaddr[OP_METHOD];
	method_op.op_type = OP_METHOD;
	myop.op_ppaddr = PL_ppaddr[OP_ENTERSUB];
	myop.op_type = OP_ENTERSUB;
	PL_op = (OP*)&method_op;
    }

    if (!(flags & G_EVAL)) {
	CATCH_SET(TRUE);
	CALL_BODY_SUB((OP*)&myop);
	retval = PL_stack_sp - (PL_stack_base + oldmark);
	CATCH_SET(oldcatch);
    }
    else {
	myop.op_other = (OP*)&myop;
	PL_markstack_ptr--;
	create_eval_scope(flags|G_FAKINGEVAL);
	PL_markstack_ptr++;

	JMPENV_PUSH(ret);

	switch (ret) {
	case 0:
 redo_body:
	    CALL_BODY_SUB((OP*)&myop);
	    retval = PL_stack_sp - (PL_stack_base + oldmark);
	    if (!(flags & G_KEEPERR)) {
		CLEAR_ERRSV();
	    }
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
	    if ((flags & G_WANT) == G_ARRAY)
		retval = 0;
	    else {
		retval = 1;
		*++PL_stack_sp = &PL_sv_undef;
	    }
	    break;
	}

	if (PL_scopestack_ix > oldscope)
	    delete_eval_scope();
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
    dVAR;
    dSP;
    UNOP myop;		/* fake syntax tree node */
    VOL I32 oldmark = SP - PL_stack_base;
    VOL I32 retval = 0;
    int ret;
    OP* const oldop = PL_op;
    dJMPENV;

    PERL_ARGS_ASSERT_EVAL_SV;

    if (flags & G_DISCARD) {
	ENTER;
	SAVETMPS;
    }

    SAVEOP();
    PL_op = (OP*)&myop;
    Zero(PL_op, 1, UNOP);
    EXTEND(PL_stack_sp, 1);
    *++PL_stack_sp = sv;

    if (!(flags & G_NOARGS))
	myop.op_flags = OPf_STACKED;
    myop.op_next = NULL;
    myop.op_type = OP_ENTEREVAL;
    myop.op_flags |= OP_GIMME_REVERSE(flags);
    if (flags & G_KEEPERR)
	myop.op_flags |= OPf_SPECIAL;

    /* fail now; otherwise we could fail after the JMPENV_PUSH but
     * before a PUSHEVAL, which corrupts the stack after a croak */
    TAINT_PROPER("eval_sv()");

    JMPENV_PUSH(ret);
    switch (ret) {
    case 0:
 redo_body:
	CALL_BODY_EVAL((OP*)&myop);
	retval = PL_stack_sp - (PL_stack_base + oldmark);
	if (!(flags & G_KEEPERR)) {
	    CLEAR_ERRSV();
	}
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
	if ((flags & G_WANT) == G_ARRAY)
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
    dVAR;
    dSP;
    SV* sv = newSVpv(p, 0);

    PERL_ARGS_ASSERT_EVAL_PV;

    eval_sv(sv, G_SCALAR);
    SvREFCNT_dec(sv);

    SPAGAIN;
    sv = POPs;
    PUTBACK;

    if (croak_on_error && SvTRUE(ERRSV)) {
	Perl_croak(aTHX_ "%s", SvPVx_nolen_const(ERRSV));
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
    dVAR;
    dSP;
    SV* sv;

    PERL_ARGS_ASSERT_REQUIRE_PV;

    PUSHSTACKi(PERLSI_REQUIRE);
    PUTBACK;
    sv = Perl_newSVpvf(aTHX_ "require q%c%s%c", 0, pv, 0);
    eval_sv(sv_2mortal(sv), G_DISCARD);
    SPAGAIN;
    POPSTACK;
}

void
Perl_magicname(pTHX_ const char *sym, const char *name, I32 namlen)
{
    register GV * const gv = gv_fetchpv(sym, GV_ADD, SVt_PV);

    PERL_ARGS_ASSERT_MAGICNAME;

    if (gv)
	sv_magic(GvSV(gv), MUTABLE_SV(gv), PERL_MAGIC_sv, name, namlen);
}

STATIC void
S_usage(pTHX_ const char *name)		/* XXX move this out into a module ? */
{
    /* This message really ought to be max 23 lines.
     * Removed -h because the user already knows that option. Others? */

    static const char * const usage_msg[] = {
"-0[octal]         specify record separator (\\0, if no argument)",
"-a                autosplit mode with -n or -p (splits $_ into @F)",
"-C[number/list]   enables the listed Unicode features",
"-c                check syntax only (runs BEGIN and CHECK blocks)",
"-d[:debugger]     run program under debugger",
"-D[number/list]   set debugging flags (argument is a bit mask or alphabets)",
"-e program        one line of program (several -e's allowed, omit programfile)",
"-E program        like -e, but enables all optional features",
"-f                don't do $sitelib/sitecustomize.pl at startup",
"-F/pattern/       split() pattern for -a switch (//'s are optional)",
"-i[extension]     edit <> files in place (makes backup if extension supplied)",
"-Idirectory       specify @INC/#include directory (several -I's allowed)",
"-l[octal]         enable line ending processing, specifies line terminator",
"-[mM][-]module    execute \"use/no module...\" before executing program",
"-n                assume \"while (<>) { ... }\" loop around program",
"-p                assume loop like -n but print line also, like sed",
"-P                run program through C preprocessor before compilation",
"-s                enable rudimentary parsing for switches after programfile",
"-S                look for programfile using PATH environment variable",
"-t                enable tainting warnings",
"-T                enable tainting checks",
"-u                dump core after parsing program",
"-U                allow unsafe operations",
"-v                print version, subversion (includes VERY IMPORTANT perl info)",
"-V[:variable]     print configuration summary (or a single Config.pm variable)",
"-w                enable many useful warnings (RECOMMENDED)",
"-W                enable all warnings",
"-x[directory]     strip off text before #!perl line and perhaps cd to directory",
"-X                disable all warnings",
"\n",
NULL
};
    const char * const *p = usage_msg;

    PERL_ARGS_ASSERT_USAGE;

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
Perl_get_debug_opts(pTHX_ const char **s, bool givehelp)
{
    static const char * const usage_msgd[] = {
      " Debugging flag values: (see also -d)",
      "  p  Tokenizing and parsing (with v, displays parse stack)",
      "  s  Stack snapshots (with v, displays all stacks)",
      "  l  Context (loop) stack processing",
      "  t  Trace execution",
      "  o  Method and overloading resolution",
      "  c  String/numeric conversions",
      "  P  Print profiling info, preprocessor command for -P, source file input state",
      "  m  Memory and SV allocation",
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

    PERL_ARGS_ASSERT_GET_DEBUG_OPTS;

    if (isALPHA(**s)) {
	/* if adding extra options, remember to update DEBUG_MASK */
	static const char debopts[] = "psltocPmfrxuUHXDSTRJvCAq";

	for (; isALNUM(**s); (*s)++) {
	    const char * const d = strchr(debopts,**s);
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
    else if (givehelp) {
      const char *const *p = usage_msgd;
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

const char *
Perl_moreswitches(pTHX_ const char *s)
{
    dVAR;
    UV rschar;
    const char option = *s; /* used to remember option in -m/-M code */

    PERL_ARGS_ASSERT_MORESWITCHES;

    switch (*s) {
    case '0':
    {
	 I32 flags = 0;
	 STRLEN numlen;

	 SvREFCNT_dec(PL_rs);
	 if (s[1] == 'x' && s[2]) {
	      const char *e = s+=2;
	      U8 *tmps;

	      while (*e)
		e++;
	      numlen = e - s;
	      flags = PERL_SCAN_SILENT_ILLDIGIT;
	      rschar = (U32)grok_hex(s, &numlen, &flags, NULL);
	      if (s + numlen < e) {
		   rschar = 0; /* Grandfather -0xFOO as -0 -xFOO. */
		   numlen = 0;
		   s--;
	      }
	      PL_rs = newSVpvs("");
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
		   PL_rs = newSVpvs("");
	      else {
		   char ch = (char)rschar;
		   PL_rs = newSVpvn(&ch, 1);
	      }
	 }
	 sv_setsv(get_sv("/", GV_ADD), PL_rs);
	 return s + numlen;
    }
    case 'C':
        s++;
        PL_unicode = parse_unicode_opts( (const char **)&s );
	if (PL_unicode & PERL_UNICODE_UTF8CACHEASSERT_FLAG)
	    PL_utf8cache = -1;
	return s;
    case 'F':
	PL_minus_F = TRUE;
	PL_splitstr = ++s;
	while (*s && !isSPACE(*s)) ++s;
	PL_splitstr = savepvn(PL_splitstr, s - PL_splitstr);
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
	forbid_setid('d', FALSE);
	s++;

        /* -dt indicates to the debugger that threads will be used */
	if (*s == 't' && !isALNUM(s[1])) {
	    ++s;
	    my_setenv("PERL5DB_THREADED", "1");
	}

	/* The following permits -d:Mod to accepts arguments following an =
	   in the fashion that -MSome::Mod does. */
	if (*s == ':' || *s == '=') {
	    const char *start = ++s;
	    const char *const end = s + strlen(s);
	    SV * const sv = newSVpvs("use Devel::");

	    /* We now allow -d:Module=Foo,Bar */
	    while(isALNUM(*s) || *s==':') ++s;
	    if (*s != '=')
		sv_catpvn(sv, start, end - start);
	    else {
		sv_catpvn(sv, start, s-start);
		/* Don't use NUL as q// delimiter here, this string goes in the
		 * environment. */
		Perl_sv_catpvf(aTHX_ sv, " split(/,/,q{%s});", ++s);
	    }
	    s = end;
	    my_setenv("PERL5DB", SvPV_nolen_const(sv));
	    SvREFCNT_dec(sv);
	}
	if (!PL_perldb) {
	    PL_perldb = PERLDB_ALL;
	    init_debugger();
	}
	return s;
    case 'D':
    {	
#ifdef DEBUGGING
	forbid_setid('D', FALSE);
	s++;
	PL_debug = get_debug_opts( (const char **)&s, 1) | DEBUG_TOP_FLAG;
#else /* !DEBUGGING */
	if (ckWARN_d(WARN_DEBUGGING))
	    Perl_warner(aTHX_ packWARN(WARN_DEBUGGING),
	           "Recompile perl with -DDEBUGGING to use -D switch (did you mean -d ?)\n");
	for (s++; isALNUM(*s); s++) ;
#endif
	return s;
    }	
    case 'h':
	usage(PL_origargv[0]);
	my_exit(0);
    case 'i':
	Safefree(PL_inplace);
#if defined(__CYGWIN__) /* do backup extension automagically */
	if (*(s+1) == '\0') {
	PL_inplace = savepvs(".bak");
	return s+1;
	}
#endif /* __CYGWIN__ */
	{
	    const char * const start = ++s;
	    while (*s && !isSPACE(*s))
		++s;

	    PL_inplace = savepvn(start, s - start);
	}
	if (*s) {
	    ++s;
	    if (*s == '-')	/* Additional switches on #! line. */
		s++;
	}
	return s;
    case 'I':	/* -I handled both here and in parse_body() */
	forbid_setid('I', FALSE);
	++s;
	while (*s && isSPACE(*s))
	    ++s;
	if (*s) {
	    const char *e, *p;
	    p = s;
	    /* ignore trailing spaces (possibly followed by other switches) */
	    do {
		for (e = p; *e && !isSPACE(*e); e++) ;
		p = e;
		while (isSPACE(*p))
		    p++;
	    } while (*p && *p != '-');
	    e = savepvn(s, e-s);
	    incpush(e, TRUE, TRUE, FALSE, FALSE);
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
	    PL_ors_sv = NULL;
	}
	if (isDIGIT(*s)) {
            I32 flags = 0;
	    STRLEN numlen;
	    PL_ors_sv = newSVpvs("\n");
	    numlen = 3 + (*s == '0');
	    *SvPVX(PL_ors_sv) = (char)grok_oct(s, &numlen, &flags, NULL);
	    s += numlen;
	}
	else {
	    if (RsPARA(PL_rs)) {
		PL_ors_sv = newSVpvs("\n\n");
	    }
	    else {
		PL_ors_sv = newSVsv(PL_rs);
	    }
	}
	return s;
    case 'M':
	forbid_setid('M', FALSE);	/* XXX ? */
	/* FALL THROUGH */
    case 'm':
	forbid_setid('m', FALSE);	/* XXX ? */
	if (*++s) {
	    const char *start;
	    const char *end;
	    SV *sv;
	    const char *use = "use ";
	    bool colon = FALSE;
	    /* -M-foo == 'no foo'	*/
	    /* Leading space on " no " is deliberate, to make both
	       possibilities the same length.  */
	    if (*s == '-') { use = " no "; ++s; }
	    sv = newSVpvn(use,4);
	    start = s;
	    /* We allow -M'Module qw(Foo Bar)'	*/
	    while(isALNUM(*s) || *s==':') {
		if( *s++ == ':' ) {
		    if( *s == ':' ) 
			s++;
		    else
			colon = TRUE;
		}
	    }
	    if (s == start)
		Perl_croak(aTHX_ "Module name required with -%c option",
				    option);
	    if (colon) 
		Perl_croak(aTHX_ "Invalid module name %.*s with -%c option: "
				    "contains single ':'",
				    (int)(s - start), start, option);
	    end = s + strlen(s);
	    if (*s != '=') {
		sv_catpvn(sv, start, end - start);
		if (option == 'm') {
		    if (*s != '\0')
			Perl_croak(aTHX_ "Can't use '%c' after -mname", *s);
		    sv_catpvs( sv, " ()");
		}
	    } else {
		sv_catpvn(sv, start, s-start);
		/* Use NUL as q''-delimiter.  */
		sv_catpvs(sv, " split(/,/,q\0");
		++s;
		sv_catpvn(sv, s, end - s);
		sv_catpvs(sv,  "\0)");
	    }
	    s = end;
	    Perl_av_create_and_push(aTHX_ &PL_preambleav, sv);
	}
	else
	    Perl_croak(aTHX_ "Missing argument to -%c", option);
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
	forbid_setid('s', FALSE);
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
	if (!sv_derived_from(PL_patchlevel, "version"))
	    upg_version(PL_patchlevel, TRUE);
#if !defined(DGUX)
	{
	    SV* level= vstringify(PL_patchlevel);
#ifdef PERL_PATCHNUM
#  ifdef PERL_GIT_UNCOMMITTED_CHANGES
	    SV *num = newSVpvs(PERL_PATCHNUM "*");
#  else
	    SV *num = newSVpvs(PERL_PATCHNUM);
#  endif

	    if (sv_len(num)>=sv_len(level) && strnEQ(SvPV_nolen(num),SvPV_nolen(level),sv_len(level))) {
		SvREFCNT_dec(level);
		level= num;
	    } else {
		Perl_sv_catpvf(aTHX_ level, " (%"SVf")", num);
		SvREFCNT_dec(num);
	    }
 #endif
	    PerlIO_printf(PerlIO_stdout(),
		"\nThis is perl, %"SVf
		" built for %s",
		level,
		ARCHNAME);
	    SvREFCNT_dec(level);
	}
#else /* DGUX */
/* Adjust verbose output as in the perl that ships with the DG/UX OS from EMC */
	PerlIO_printf(PerlIO_stdout(),
		Perl_form(aTHX_ "\nThis is perl, %"SVf"\n",
		    SVfARG(vstringify(PL_patchlevel))));
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
			  LOCAL_PATCH_COUNT,
			  (LOCAL_PATCH_COUNT!=1) ? "es" : "");
#endif

	PerlIO_printf(PerlIO_stdout(),
		      "\n\nCopyright 1987-2009, Larry Wall\n");
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
#ifdef __SYMBIAN32__
	PerlIO_printf(PerlIO_stdout(),
		      "Symbian port by Nokia, 2004-2005\n");
#endif
#ifdef BINARY_BUILD_NOTICE
	BINARY_BUILD_NOTICE;
#endif
	PerlIO_printf(PerlIO_stdout(),
		      "\n\
Perl may be copied only under the terms of either the Artistic License or the\n\
GNU General Public License, which may be found in the Perl 5 source kit.\n\n\
Complete documentation for Perl, including FAQ lists, should be found on\n\
this system using \"man perl\" or \"perldoc perl\".  If you have access to the\n\
Internet, point your browser at http://www.perl.org/, the Perl Home Page.\n\n");
	my_exit(0);
    case 'w':
	if (! (PL_dowarn & G_WARN_ALL_MASK)) {
	    PL_dowarn |= G_WARN_ON;
	}
	s++;
	return s;
    case 'W':
	PL_dowarn = G_WARN_ALL_ON|G_WARN_ON;
        if (!specialWARN(PL_compiling.cop_warnings))
            PerlMemShared_free(PL_compiling.cop_warnings);
	PL_compiling.cop_warnings = pWARN_ALL ;
	s++;
	return s;
    case 'X':
	PL_dowarn = G_WARN_ALL_OFF;
        if (!specialWARN(PL_compiling.cop_warnings))
            PerlMemShared_free(PL_compiling.cop_warnings);
	PL_compiling.cop_warnings = pWARN_NONE ;
	s++;
	return s;
    case '*':
    case ' ':
        while( *s == ' ' )
          ++s;
	if (s[0] == '-')	/* Additional switches on #! line. */
	    return s+1;
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
    return NULL;
}

/* compliments of Tom Christiansen */

/* unexec() can be found in the Gnu emacs distribution */
/* Known to work with -DUNEXEC and using unexelf.c from GNU emacs-20.2 */

void
Perl_my_unexec(pTHX)
{
    PERL_UNUSED_CONTEXT;
#ifdef UNEXEC
    SV *    prog = newSVpv(BIN_EXP, 0);
    SV *    file = newSVpv(PL_origfilename, 0);
    int    status = 1;
    extern int etext;

    sv_catpvs(prog, "/perl");
    sv_catpvs(file, ".perldump");

    unexec(SvPVX(file), SvPVX(prog), &etext, sbrk(0), 0);
    /* unexec prints msg to stderr in case of failure */
    PerlProc_exit(status);
#else
#  ifdef VMS
#    include <lib$routines.h>
     lib$signal(SS$_DEBUG);  /* ssdef.h #included from vmsish.h */
#  elif defined(WIN32) || defined(__CYGWIN__)
    Perl_croak(aTHX_ "dump is not supported");
#  else
    ABORT();		/* for use with undump */
#  endif
#endif
}

/* initialize curinterp */
STATIC void
S_init_interp(pTHX)
{
    dVAR;
#ifdef MULTIPLICITY
#  define PERLVAR(var,type)
#  define PERLVARA(var,n,type)
#  if defined(PERL_IMPLICIT_CONTEXT)
#    define PERLVARI(var,type,init)		aTHX->var = init;
#    define PERLVARIC(var,type,init)	aTHX->var = init;
#  else
#    define PERLVARI(var,type,init)	PERL_GET_INTERP->var = init;
#    define PERLVARIC(var,type,init)	PERL_GET_INTERP->var = init;
#  endif
#  include "intrpvar.h"
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
#  undef PERLVAR
#  undef PERLVARA
#  undef PERLVARI
#  undef PERLVARIC
#endif

    /* As these are inside a structure, PERLVARI isn't capable of initialising
       them  */
    PL_reg_oldcurpm = PL_reg_curpm = NULL;
    PL_reg_poscache = PL_reg_starttry = NULL;
}

STATIC void
S_init_main_stash(pTHX)
{
    dVAR;
    GV *gv;

    PL_curstash = PL_defstash = newHV();
    /* We know that the string "main" will be in the global shared string
       table, so it's a small saving to use it rather than allocate another
       8 bytes.  */
    PL_curstname = newSVpvs_share("main");
    gv = gv_fetchpvs("main::", GV_ADD|GV_NOTQUAL, SVt_PVHV);
    /* If we hadn't caused another reference to "main" to be in the shared
       string table above, then it would be worth reordering these two,
       because otherwise all we do is delete "main" from it as a consequence
       of the SvREFCNT_dec, only to add it again with hv_name_set */
    SvREFCNT_dec(GvHV(gv));
    hv_name_set(PL_defstash, "main", 4, 0);
    GvHV(gv) = MUTABLE_HV(SvREFCNT_inc_simple(PL_defstash));
    SvREADONLY_on(gv);
    PL_incgv = gv_HVadd(gv_AVadd(gv_fetchpvs("INC", GV_ADD|GV_NOTQUAL,
					     SVt_PVAV)));
    SvREFCNT_inc_simple_void(PL_incgv); /* Don't allow it to be freed */
    GvMULTI_on(PL_incgv);
    PL_hintgv = gv_fetchpvs("\010", GV_ADD|GV_NOTQUAL, SVt_PV); /* ^H */
    GvMULTI_on(PL_hintgv);
    PL_defgv = gv_fetchpvs("_", GV_ADD|GV_NOTQUAL, SVt_PVAV);
    SvREFCNT_inc_simple_void(PL_defgv);
    PL_errgv = gv_HVadd(gv_fetchpvs("@", GV_ADD|GV_NOTQUAL, SVt_PV));
    SvREFCNT_inc_simple_void(PL_errgv);
    GvMULTI_on(PL_errgv);
    PL_replgv = gv_fetchpvs("\022", GV_ADD|GV_NOTQUAL, SVt_PV); /* ^R */
    GvMULTI_on(PL_replgv);
    (void)Perl_form(aTHX_ "%240s","");	/* Preallocate temp - for immediate signals. */
#ifdef PERL_DONT_CREATE_GVSV
    gv_SVadd(PL_errgv);
#endif
    sv_grow(ERRSV, 240);	/* Preallocate - for immediate signals. */
    CLEAR_ERRSV();
    PL_curstash = PL_defstash;
    CopSTASH_set(&PL_compiling, PL_defstash);
    PL_debstash = GvHV(gv_fetchpvs("DB::", GV_ADDMULTI, SVt_PVHV));
    PL_globalstash = GvHV(gv_fetchpvs("CORE::GLOBAL::", GV_ADDMULTI,
				      SVt_PVHV));
    /* We must init $/ before switches are processed. */
    sv_setpvs(get_sv("/", GV_ADD), "\n");
}

STATIC int
S_open_script(pTHX_ const char *scriptname, bool dosearch, SV *sv,
	      bool *suidscript, PerlIO **rsfpp)
{
#ifndef IAMSUID
    const char *quote;
    const char *code;
    const char *cpp_discard_flag;
    const char *perl;
#endif
    int fdscript = -1;
    dVAR;

    PERL_ARGS_ASSERT_OPEN_SCRIPT;

    if (PL_e_script) {
	PL_origfilename = savepvs("-e");
    }
    else {
	/* if find_script() returns, it returns a malloc()-ed value */
	scriptname = PL_origfilename = find_script(scriptname, dosearch, NULL, 1);

	if (strnEQ(scriptname, "/dev/fd/", 8) && isDIGIT(scriptname[8]) ) {
            const char *s = scriptname + 8;
	    fdscript = atoi(s);
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
		*suidscript = TRUE;
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
		PL_origfilename = (char *)scriptname;
	    }
	}
    }

    CopFILE_free(PL_curcop);
    CopFILE_set(PL_curcop, PL_origfilename);
    if (*PL_origfilename == '-' && PL_origfilename[1] == '\0')
	scriptname = (char *)"";
    if (fdscript >= 0) {
	*rsfpp = PerlIO_fdopen(fdscript,PERL_SCRIPT_MODE);
#       if defined(HAS_FCNTL) && defined(F_SETFD)
	    if (*rsfpp)
                /* ensure close-on-exec */
	        fcntl(PerlIO_fileno(*rsfpp),F_SETFD,1);
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
    if (!*suidscript) {
	Perl_croak(aTHX_ "suidperl needs (suid) fd script\n");
    }
#else /* IAMSUID */
    else if (PL_preprocess) {
	const char * const cpp_cfg = CPPSTDIN;
	SV * const cpp = newSVpvs("");
	SV * const cmd = newSV(0);

	if (cpp_cfg[0] == 0) /* PERL_MICRO? */
	     Perl_croak(aTHX_ "Can't run with cpp -P with CPPSTDIN undefined");
	if (strEQ(cpp_cfg, "cppstdin"))
	    Perl_sv_catpvf(aTHX_ cpp, "%s/", BIN_EXP);
	sv_catpv(cpp, cpp_cfg);

#       ifndef VMS
	    sv_catpvs(sv, "-I");
	    sv_catpv(sv,PRIVLIB_EXP);
#       endif

	DEBUG_P(PerlIO_printf(Perl_debug_log,
			      "PL_preprocess: scriptname=\"%s\", cpp=\"%s\", sv=\"%s\", CPPMINUS=\"%s\"\n",
			      scriptname, SvPVX_const (cpp), SvPVX_const (sv),
			      CPPMINUS));

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
                       perl, quote, code, quote, scriptname, SVfARG(cpp),
                       cpp_discard_flag, SVfARG(sv), CPPMINUS);

	PL_doextract = FALSE;

        DEBUG_P(PerlIO_printf(Perl_debug_log,
                              "PL_preprocess: cmd=\"%s\"\n",
                              SvPVX_const(cmd)));

	*rsfpp = PerlProc_popen((char *)SvPVX_const(cmd), (char *)"r");
	SvREFCNT_dec(cmd);
	SvREFCNT_dec(cpp);
    }
    else if (!*scriptname) {
	forbid_setid(0, *suidscript);
	*rsfpp = PerlIO_stdin();
    }
    else {
#ifdef FAKE_BIT_BUCKET
	/* This hack allows one not to have /dev/null (or BIT_BUCKET as it
	 * is called) and still have the "-e" work.  (Believe it or not,
	 * a /dev/null is required for the "-e" to work because source
	 * filter magic is used to implement it. ) This is *not* a general
	 * replacement for a /dev/null.  What we do here is create a temp
	 * file (an empty file), open up that as the script, and then
	 * immediately close and unlink it.  Close enough for jazz. */ 
#define FAKE_BIT_BUCKET_PREFIX "/tmp/perlnull-"
#define FAKE_BIT_BUCKET_SUFFIX "XXXXXXXX"
#define FAKE_BIT_BUCKET_TEMPLATE FAKE_BIT_BUCKET_PREFIX FAKE_BIT_BUCKET_SUFFIX
	char tmpname[sizeof(FAKE_BIT_BUCKET_TEMPLATE)] = {
	    FAKE_BIT_BUCKET_TEMPLATE
	};
	const char * const err = "Failed to create a fake bit bucket";
	if (strEQ(scriptname, BIT_BUCKET)) {
#ifdef HAS_MKSTEMP /* Hopefully mkstemp() is safe here. */
	    int tmpfd = mkstemp(tmpname);
	    if (tmpfd > -1) {
		scriptname = tmpname;
		close(tmpfd);
	    } else
		Perl_croak(aTHX_ err);
#else
#  ifdef HAS_MKTEMP
	    scriptname = mktemp(tmpname);
	    if (!scriptname)
		Perl_croak(aTHX_ err);
#  endif
#endif
	}
#endif
	*rsfpp = PerlIO_open(scriptname,PERL_SCRIPT_MODE);
#ifdef FAKE_BIT_BUCKET
	if (memEQ(scriptname, FAKE_BIT_BUCKET_PREFIX,
		  sizeof(FAKE_BIT_BUCKET_PREFIX) - 1)
	    && strlen(scriptname) == sizeof(tmpname) - 1) {
	    unlink(scriptname);
	}
	scriptname = BIT_BUCKET;
#endif
#       if defined(HAS_FCNTL) && defined(F_SETFD)
	    if (*rsfpp)
                /* ensure close-on-exec */
	        fcntl(PerlIO_fileno(*rsfpp),F_SETFD,1);
#       endif
    }
#endif /* IAMSUID */
    if (!*rsfpp) {
	/* PSz 16 Sep 03  Keep neat error message */
	if (PL_e_script)
	    Perl_croak(aTHX_ "Can't open "BIT_BUCKET": %s\n", Strerror(errno));
	else
	    Perl_croak(aTHX_ "Can't open perl script \"%s\": %s\n",
		    CopFILE(PL_curcop), Strerror(errno));
    }
    return fdscript;
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

#ifdef DOSUID
STATIC void
S_validate_suid(pTHX_ const char *validarg,
#  ifndef IAMSUID
		const char *scriptname,
#  endif
		int fdscript,
#  ifdef IAMSUID
		bool suidscript,
#  endif
		SV *linestr_sv, PerlIO *rsfp)
{
    dVAR;
    const char *s, *s2;

    PERL_ARGS_ASSERT_VALIDATE_SUID;

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

    if (PerlLIO_fstat(PerlIO_fileno(rsfp),&PL_statbuf) < 0)	/* normal stat is insecure */
	Perl_croak(aTHX_ "Can't stat script \"%s\"",PL_origfilename);
    if (PL_statbuf.st_mode & (S_ISUID|S_ISGID)) {
	I32 len;
	const char *linestr;
	const char *s_end;

#  ifdef IAMSUID
	if (fdscript < 0 || !suidscript)
	    Perl_croak(aTHX_ "Need (suid) fdscript in suidperl\n");	/* We already checked this */
	/* PSz 11 Nov 03
	 * Since the script is opened by perl, not suidperl, some of these
	 * checks are superfluous. Leaving them in probably does not lower
	 * security(?!).
	 */
	/* PSz 27 Feb 04
	 * Do checks even for systems with no HAS_SETREUID.
	 * We used to swap, then re-swap UIDs with
#    ifdef HAS_SETREUID
	    if (setreuid(PL_euid,PL_uid) < 0
		|| PerlProc_getuid() != PL_euid || PerlProc_geteuid() != PL_uid)
		Perl_croak(aTHX_ "Can't swap uid and euid");
#    endif
#    ifdef HAS_SETREUID
	    if (setreuid(PL_uid,PL_euid) < 0
		|| PerlProc_getuid() != PL_uid || PerlProc_geteuid() != PL_euid)
		Perl_croak(aTHX_ "Can't reswap uid and euid");
#    endif
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
#    if !defined(NO_NOSUID_CHECK)
	if (fd_on_nosuid_fs(PerlIO_fileno(rsfp))) {
	    Perl_croak(aTHX_ "Setuid script on nosuid or noexec filesystem\n");
	}
#    endif
#  endif /* IAMSUID */

	if (!S_ISREG(PL_statbuf.st_mode)) {
	    Perl_croak(aTHX_ "Setuid script not plain file\n");
	}
	if (PL_statbuf.st_mode & S_IWOTH)
	    Perl_croak(aTHX_ "Setuid/gid script is writable by world");
	PL_doswitches = FALSE;		/* -s is insecure in suid */
	/* PSz 13 Nov 03  But -s was caught elsewhere ... so unsetting it here is useless(?!) */
	CopLINE_inc(PL_curcop);
	if (sv_gets(linestr_sv, rsfp, 0) == NULL)
	    Perl_croak(aTHX_ "No #! line");
	linestr = SvPV_nolen_const(linestr_sv);
	/* required even on Sys V */
	if (!*linestr || !linestr[1] || strnNE(linestr,"#!",2))
	    Perl_croak(aTHX_ "No #! line");
	linestr += 2;
	s = linestr;
	/* PSz 27 Feb 04 */
	/* Sanity check on line length */
	s_end = s + strlen(s);
	if (s_end == s || (s_end - s) > 4000)
	    Perl_croak(aTHX_ "Very long #! line");
	/* Allow more than a single space after #! */
	while (isSPACE(*s)) s++;
	/* Sanity check on buffer end */
	while ((*s) && !isSPACE(*s)) s++;
	for (s2 = s;  (s2 > linestr &&
		       (isDIGIT(s2[-1]) || s2[-1] == '.' || s2[-1] == '_'
			|| s2[-1] == '-'));  s2--) ;
	/* Sanity check on buffer start */
	if ( (s2-4 < linestr || strnNE(s2-4,"perl",4)) &&
	      (s-9 < linestr || strnNE(s-9,"perl",4)) )
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
	    !((s_end - s) == len+1
	      || ((s_end - s) == len+2 && isSPACE(s[len+1]))))
	    Perl_croak(aTHX_ "Args must match #! line");

#  ifndef IAMSUID
	if (fdscript < 0 &&
	    PL_euid != PL_uid && (PL_statbuf.st_mode & S_ISUID) &&
	    PL_euid == PL_statbuf.st_uid)
	    if (!PL_do_undump)
		Perl_croak(aTHX_ "YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, OR PUT A C WRAPPER AROUND THIS SCRIPT!\n");
#  endif /* IAMSUID */

	if (fdscript < 0 &&
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
	     * fdscript to avoid loops), and do the execs
	     * even for root.
	     */
#  ifndef IAMSUID
	    int which;
	    /* PSz 11 Nov 03
	     * Pass fd script to suidperl.
	     * Exec suidperl, substituting fd script for scriptname.
	     * Pass script name as "subdir" of fd, which perl will grok;
	     * in fact will use that to distinguish this from "normal"
	     * usage, see comments above.
	     */
	    PerlIO_rewind(rsfp);
	    PerlLIO_lseek(PerlIO_fileno(rsfp),(Off_t)0,0);  /* just in case rewind didn't */
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
					  PerlIO_fileno(rsfp), PL_origargv[which]));
#    if defined(HAS_FCNTL) && defined(F_SETFD)
	    fcntl(PerlIO_fileno(rsfp),F_SETFD,0);	/* ensure no close-on-exec */
#    endif
	    PERL_FPU_PRE_EXEC
	    PerlProc_execv(BIN_EXP "/sperl" PERL_FS_VERSION, PL_origargv);
	    PERL_FPU_POST_EXEC
#  endif /* IAMSUID */
	    Perl_croak(aTHX_ "Can't do setuid (cannot exec sperl)\n");
	}

	if (PL_statbuf.st_mode & S_ISGID && PL_statbuf.st_gid != PL_egid) {
/* PSz 26 Feb 04
 * This seems back to front: we try HAS_SETEGID first; if not available
 * then try HAS_SETREGID; as a last chance we try HAS_SETRESGID. May be OK
 * in the sense that we only want to set EGID; but are there any machines
 * with either of the latter, but not the former? Same with UID, later.
 */
#  ifdef HAS_SETEGID
	    (void)setegid(PL_statbuf.st_gid);
#  else
#    ifdef HAS_SETREGID
           (void)setregid((Gid_t)-1,PL_statbuf.st_gid);
#    else
#      ifdef HAS_SETRESGID
           (void)setresgid((Gid_t)-1,PL_statbuf.st_gid,(Gid_t)-1);
#      else
	    PerlProc_setgid(PL_statbuf.st_gid);
#      endif
#    endif
#  endif
	    if (PerlProc_getegid() != PL_statbuf.st_gid)
		Perl_croak(aTHX_ "Can't do setegid!\n");
	}
	if (PL_statbuf.st_mode & S_ISUID) {
	    if (PL_statbuf.st_uid != PL_euid)
#  ifdef HAS_SETEUID
		(void)seteuid(PL_statbuf.st_uid);	/* all that for this */
#  else
#    ifdef HAS_SETREUID
                (void)setreuid((Uid_t)-1,PL_statbuf.st_uid);
#    else
#      ifdef HAS_SETRESUID
                (void)setresuid((Uid_t)-1,PL_statbuf.st_uid,(Uid_t)-1);
#      else
		PerlProc_setuid(PL_statbuf.st_uid);
#      endif
#    endif
#  endif
	    if (PerlProc_geteuid() != PL_statbuf.st_uid)
		Perl_croak(aTHX_ "Can't do seteuid!\n");
	}
	else if (PL_uid) {			/* oops, mustn't run as root */
#  ifdef HAS_SETEUID
          (void)seteuid((Uid_t)PL_uid);
#  else
#    ifdef HAS_SETREUID
          (void)setreuid((Uid_t)-1,(Uid_t)PL_uid);
#    else
#      ifdef HAS_SETRESUID
          (void)setresuid((Uid_t)-1,(Uid_t)PL_uid,(Uid_t)-1);
#      else
          PerlProc_setuid((Uid_t)PL_uid);
#      endif
#    endif
#  endif
	    if (PerlProc_geteuid() != PL_uid)
		Perl_croak(aTHX_ "Can't do seteuid!\n");
	}
	init_ids();
	if (!cando(S_IXUSR,TRUE,&PL_statbuf))
	    Perl_croak(aTHX_ "Effective UID cannot exec script\n");	/* they can't do this */
    }
#  ifdef IAMSUID
    else if (PL_preprocess)	/* PSz 13 Nov 03  Caught elsewhere, useless(?!) here */
	Perl_croak(aTHX_ "-P not allowed for setuid/setgid script\n");
    else if (fdscript < 0 || !suidscript)
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
    PerlIO_rewind(rsfp);
    PerlLIO_lseek(PerlIO_fileno(rsfp),(Off_t)0,0);  /* just in case rewind didn't */
    /* PSz 11 Nov 03
     * Keep original arguments: suidperl already has fd script.
     */
#  if defined(HAS_FCNTL) && defined(F_SETFD)
    fcntl(PerlIO_fileno(rsfp),F_SETFD,0);	/* ensure no close-on-exec */
#  endif
    PERL_FPU_PRE_EXEC
    PerlProc_execv(BIN_EXP "/perl" PERL_FS_VERSION, PL_origargv);/* try again */
    PERL_FPU_POST_EXEC
    Perl_croak(aTHX_ "Can't do setuid (suidperl cannot exec perl)\n");
#  endif /* IAMSUID */
}

#else /* !DOSUID */

#  ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
/* Don't even need this function.  */
#  else
STATIC void
S_validate_suid(pTHX_ PerlIO *rsfp)
{
    PERL_ARGS_ASSERT_VALIDATE_SUID;

    if (PL_euid != PL_uid || PL_egid != PL_gid) {	/* (suidperl doesn't exist, in fact) */
	dVAR;

	PerlLIO_fstat(PerlIO_fileno(rsfp),&PL_statbuf);	/* may be either wrapped or real suid */
	if ((PL_euid != PL_uid && PL_euid == PL_statbuf.st_uid && PL_statbuf.st_mode & S_ISUID)
	    ||
	    (PL_egid != PL_gid && PL_egid == PL_statbuf.st_gid && PL_statbuf.st_mode & S_ISGID)
	   )
	    if (!PL_do_undump)
		Perl_croak(aTHX_ "YOU HAVEN'T DISABLED SET-ID SCRIPTS IN THE KERNEL YET!\n\
FIX YOUR KERNEL, PUT A C WRAPPER AROUND THIS SCRIPT, OR USE -u AND UNDUMP!\n");
	/* not set-id, must be wrapped */
    }
}
#  endif /* SETUID_SCRIPTS_ARE_SECURE_NOW */
#endif /* DOSUID */

STATIC void
S_find_beginning(pTHX_ SV* linestr_sv, PerlIO *rsfp)
{
    dVAR;
    const char *s;
    register const char *s2;
#ifdef MACOS_TRADITIONAL
    int maclines = 0;
#endif

    PERL_ARGS_ASSERT_FIND_BEGINNING;

    /* skip forward in input to the real script? */

#ifdef MACOS_TRADITIONAL
    /* Since the Mac OS does not honor #! arguments for us, we do it ourselves */

    while (PL_doextract || gMacPerl_AlwaysExtract) {
	if ((s = sv_gets(linestr_sv, rsfp, 0)) == NULL) {
	    if (!gMacPerl_AlwaysExtract)
		Perl_croak(aTHX_ "No Perl script found in input\n");

	    if (PL_doextract)			/* require explicit override ? */
		if (!OverrideExtract(PL_origfilename))
		    Perl_croak(aTHX_ "User aborted script\n");
		else
		    PL_doextract = FALSE;

	    /* Pater peccavi, file does not have #! */
	    PerlIO_rewind(rsfp);

	    break;
	}
#else
    while (PL_doextract) {
	if ((s = sv_gets(linestr_sv, rsfp, 0)) == NULL)
	    Perl_croak(aTHX_ "No Perl script found in input\n");
#endif
	s2 = s;
	if (*s == '#' && s[1] == '!' && ((s = instr(s,"perl")) || (s = instr(s2,"PERL")))) {
	    PerlIO_ungetc(rsfp, '\n');		/* to keep line count right */
	    PL_doextract = FALSE;
	    while (*s && !(isSPACE (*s) || *s == '#')) s++;
	    s2 = s;
	    while (*s == ' ' || *s == '\t') s++;
	    if (*s++ == '-') {
		while (isDIGIT(s2[-1]) || s2[-1] == '-' || s2[-1] == '.'
		       || s2[-1] == '_') s2--;
		if (strnEQ(s2-4,"perl",4))
		    while ((s = moreswitches(s)))
			;
	    }
#ifdef MACOS_TRADITIONAL
	    /* We are always searching for the #!perl line in MacPerl,
	     * so if we find it, still keep the line count correct
	     * by counting lines we already skipped over
	     */
	    for (; maclines > 0 ; maclines--)
		PerlIO_ungetc(rsfp, '\n');

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
    dVAR;
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
    (void)envp;

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

/* Passing the flag as a single char rather than a string is a slight space
   optimisation.  The only message that isn't /^-.$/ is
   "program input from stdin", which is substituted in place of '\0', which
   could never be a command line flag.  */
STATIC void
S_forbid_setid(pTHX_ const char flag, const bool suidscript) /* g */
{
    dVAR;
    char string[3] = "-x";
    const char *message = "program input from stdin";

    if (flag) {
	string[1] = flag;
	message = string;
    }

#ifdef SETUID_SCRIPTS_ARE_SECURE_NOW
    if (PL_euid != PL_uid)
        Perl_croak(aTHX_ "No %s allowed while running setuid", message);
    if (PL_egid != PL_gid)
        Perl_croak(aTHX_ "No %s allowed while running setgid", message);
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
    if (suidscript)
        Perl_croak(aTHX_ "No %s allowed with (suid) fdscript", message);
#ifdef IAMSUID
    /* PSz 11 Nov 03  Catch it in suidperl, always! */
    Perl_croak(aTHX_ "No %s allowed in suidperl", message);
#endif /* IAMSUID */
}

void
Perl_init_debugger(pTHX)
{
    dVAR;
    HV * const ostash = PL_curstash;

    PL_curstash = PL_debstash;
    PL_dbargs = GvAV(gv_AVadd((gv_fetchpvs("DB::args", GV_ADDMULTI,
					   SVt_PVAV))));
    AvREAL_off(PL_dbargs);
    PL_DBgv = gv_fetchpvs("DB::DB", GV_ADDMULTI, SVt_PVGV);
    PL_DBline = gv_fetchpvs("DB::dbline", GV_ADDMULTI, SVt_PVAV);
    PL_DBsub = gv_HVadd(gv_fetchpvs("DB::sub", GV_ADDMULTI, SVt_PVHV));
    PL_DBsingle = GvSV((gv_fetchpvs("DB::single", GV_ADDMULTI, SVt_PV)));
    sv_setiv(PL_DBsingle, 0);
    PL_DBtrace = GvSV((gv_fetchpvs("DB::trace", GV_ADDMULTI, SVt_PV)));
    sv_setiv(PL_DBtrace, 0);
    PL_DBsignal = GvSV((gv_fetchpvs("DB::signal", GV_ADDMULTI, SVt_PV)));
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
    dVAR;
    /* start with 128-item stack and 8K cxstack */
    PL_curstackinfo = new_stackinfo(REASONABLE(128),
				 REASONABLE(8192/sizeof(PERL_CONTEXT) - 1));
    PL_curstackinfo->si_type = PERLSI_MAIN;
    PL_curstack = PL_curstackinfo->si_stack;
    PL_mainstack = PL_curstack;		/* remember in case we switch stacks */

    PL_stack_base = AvARRAY(PL_curstack);
    PL_stack_sp = PL_stack_base;
    PL_stack_max = PL_stack_base + AvMAX(PL_curstack);

    Newx(PL_tmps_stack,REASONABLE(128),SV*);
    PL_tmps_floor = -1;
    PL_tmps_ix = -1;
    PL_tmps_max = REASONABLE(128);

    Newx(PL_markstack,REASONABLE(32),I32);
    PL_markstack_ptr = PL_markstack;
    PL_markstack_max = PL_markstack + REASONABLE(32);

    SET_MARK_OFFSET;

    Newx(PL_scopestack,REASONABLE(32),I32);
    PL_scopestack_ix = 0;
    PL_scopestack_max = REASONABLE(32);

    Newx(PL_savestack,REASONABLE(128),ANY);
    PL_savestack_ix = 0;
    PL_savestack_max = REASONABLE(128);
}

#undef REASONABLE

STATIC void
S_nuke_stacks(pTHX)
{
    dVAR;
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
}


STATIC void
S_init_predump_symbols(pTHX)
{
    dVAR;
    GV *tmpgv;
    IO *io;

    sv_setpvs(get_sv("\"", GV_ADD), " ");
    PL_stdingv = gv_fetchpvs("STDIN", GV_ADD|GV_NOTQUAL, SVt_PVIO);
    GvMULTI_on(PL_stdingv);
    io = GvIOp(PL_stdingv);
    IoTYPE(io) = IoTYPE_RDONLY;
    IoIFP(io) = PerlIO_stdin();
    tmpgv = gv_fetchpvs("stdin", GV_ADD|GV_NOTQUAL, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = MUTABLE_IO(SvREFCNT_inc_simple(io));

    tmpgv = gv_fetchpvs("STDOUT", GV_ADD|GV_NOTQUAL, SVt_PVIO);
    GvMULTI_on(tmpgv);
    io = GvIOp(tmpgv);
    IoTYPE(io) = IoTYPE_WRONLY;
    IoOFP(io) = IoIFP(io) = PerlIO_stdout();
    setdefout(tmpgv);
    tmpgv = gv_fetchpvs("stdout", GV_ADD|GV_NOTQUAL, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = MUTABLE_IO(SvREFCNT_inc_simple(io));

    PL_stderrgv = gv_fetchpvs("STDERR", GV_ADD|GV_NOTQUAL, SVt_PVIO);
    GvMULTI_on(PL_stderrgv);
    io = GvIOp(PL_stderrgv);
    IoTYPE(io) = IoTYPE_WRONLY;
    IoOFP(io) = IoIFP(io) = PerlIO_stderr();
    tmpgv = gv_fetchpvs("stderr", GV_ADD|GV_NOTQUAL, SVt_PV);
    GvMULTI_on(tmpgv);
    GvIOp(tmpgv) = MUTABLE_IO(SvREFCNT_inc_simple(io));

    PL_statname = newSV(0);		/* last filename we did stat on */

    Safefree(PL_osname);
    PL_osname = savepv(OSNAME);
}

void
Perl_init_argv_symbols(pTHX_ register int argc, register char **argv)
{
    dVAR;

    PERL_ARGS_ASSERT_INIT_ARGV_SYMBOLS;

    argc--,argv++;	/* skip name of script */
    if (PL_doswitches) {
	for (; argc > 0 && **argv == '-'; argc--,argv++) {
	    char *s;
	    if (!argv[0][1])
		break;
	    if (argv[0][1] == '-' && !argv[0][2]) {
		argc--,argv++;
		break;
	    }
	    if ((s = strchr(argv[0], '='))) {
		const char *const start_name = argv[0] + 1;
		sv_setpv(GvSV(gv_fetchpvn_flags(start_name, s - start_name,
						TRUE, SVt_PV)), s + 1);
	    }
	    else
		sv_setiv(GvSV(gv_fetchpv(argv[0]+1, GV_ADD, SVt_PV)),1);
	}
    }
    if ((PL_argvgv = gv_fetchpvs("ARGV", GV_ADD|GV_NOTQUAL, SVt_PVAV))) {
	GvMULTI_on(PL_argvgv);
	(void)gv_AVadd(PL_argvgv);
	av_clear(GvAVn(PL_argvgv));
	for (; argc > 0; argc--,argv++) {
	    SV * const sv = newSVpv(argv[0],0);
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

STATIC void
S_init_postdump_symbols(pTHX_ register int argc, register char **argv, register char **env)
{
    dVAR;
    GV* tmpgv;

    PERL_ARGS_ASSERT_INIT_POSTDUMP_SYMBOLS;

    PL_toptarget = newSV_type(SVt_PVFM);
    sv_setpvs(PL_toptarget, "");
    PL_bodytarget = newSV_type(SVt_PVFM);
    sv_setpvs(PL_bodytarget, "");
    PL_formtarget = PL_bodytarget;

    TAINT;

    init_argv_symbols(argc,argv);

    if ((tmpgv = gv_fetchpvs("0", GV_ADD|GV_NOTQUAL, SVt_PV))) {
#ifdef MACOS_TRADITIONAL
	/* $0 is not majick on a Mac */
	sv_setpv(GvSV(tmpgv),MacPerl_MPWFileName(PL_origfilename));
#else
	sv_setpv(GvSV(tmpgv),PL_origfilename);
	magicname("0", "0", 1);
#endif
    }
    if ((PL_envgv = gv_fetchpvs("ENV", GV_ADD|GV_NOTQUAL, SVt_PVHV))) {
	HV *hv;
	bool env_is_not_environ;
	GvMULTI_on(PL_envgv);
	hv = GvHVn(PL_envgv);
	hv_magic(hv, NULL, PERL_MAGIC_env);
#ifndef PERL_MICRO
#ifdef USE_ENVIRON_ARRAY
	/* Note that if the supplied env parameter is actually a copy
	   of the global environ then it may now point to free'd memory
	   if the environment has been modified since. To avoid this
	   problem we treat env==NULL as meaning 'use the default'
	*/
	if (!env)
	    env = environ;
	env_is_not_environ = env != environ;
	if (env_is_not_environ
#  ifdef USE_ITHREADS
	    && PL_curinterp == aTHX
#  endif
	   )
	{
	    environ[0] = NULL;
	}
	if (env) {
	  char *s, *old_var;
	  SV *sv;
	  for (; *env; env++) {
	    old_var = *env;

	    if (!(s = strchr(old_var,'=')) || s == old_var)
		continue;

#if defined(MSDOS) && !defined(DJGPP)
	    *s = '\0';
	    (void)strupr(old_var);
	    *s = '=';
#endif
	    sv = newSVpv(s+1, 0);
	    (void)hv_store(hv, old_var, s - old_var, sv, 0);
	    if (env_is_not_environ)
	        mg_set(sv);
	  }
      }
#endif /* USE_ENVIRON_ARRAY */
#endif /* !PERL_MICRO */
    }
    TAINT_NOT;
    if ((tmpgv = gv_fetchpvs("$", GV_ADD|GV_NOTQUAL, SVt_PV))) {
        SvREADONLY_off(GvSV(tmpgv));
	sv_setiv(GvSV(tmpgv), (IV)PerlProc_getpid());
        SvREADONLY_on(GvSV(tmpgv));
    }
#ifdef THREADS_HAVE_PIDS
    PL_ppid = (IV)getppid();
#endif

    /* touch @F array to prevent spurious warnings 20020415 MJD */
    if (PL_minus_a) {
      (void) get_av("main::F", GV_ADD | GV_ADDMULTI);
    }
}

STATIC void
S_init_perllib(pTHX)
{
    dVAR;
    char *s;
    if (!PL_tainting) {
#ifndef VMS
	s = PerlEnv_getenv("PERL5LIB");
/*
 * It isn't possible to delete an environment variable with
 * PERL_USE_SAFE_PUTENV set unless unsetenv() is also available, so in that
 * case we treat PERL5LIB as undefined if it has a zero-length value.
 */
#if defined(PERL_USE_SAFE_PUTENV) && ! defined(HAS_UNSETENV)
	if (s && *s != '\0')
#else
	if (s)
#endif
	    incpush(s, TRUE, TRUE, TRUE, FALSE);
	else
	    incpush(PerlEnv_getenv("PERLLIB"), FALSE, FALSE, TRUE, FALSE);
#else /* VMS */
	/* Treat PERL5?LIB as a possible search list logical name -- the
	 * "natural" VMS idiom for a Unix path string.  We allow each
	 * element to be a set of |-separated directories for compatibility.
	 */
	char buf[256];
	int idx = 0;
	if (my_trnlnm("PERL5LIB",buf,0))
	    do { incpush(buf,TRUE,TRUE,TRUE,FALSE); } while (my_trnlnm("PERL5LIB",buf,++idx));
	else
	    while (my_trnlnm("PERLLIB",buf,idx++)) incpush(buf,FALSE,FALSE,TRUE,FALSE);
#endif /* VMS */
    }

/* Use the ~-expanded versions of APPLLIB (undocumented),
    ARCHLIB PRIVLIB SITEARCH SITELIB VENDORARCH and VENDORLIB
*/
#ifdef APPLLIB_EXP
    incpush(APPLLIB_EXP, TRUE, TRUE, TRUE, TRUE);
#endif

#ifdef ARCHLIB_EXP
    incpush(ARCHLIB_EXP, FALSE, FALSE, TRUE, TRUE);
#endif
#ifdef MACOS_TRADITIONAL
    {
	Stat_t tmpstatbuf;
    	SV * privdir = newSV(0);
	char * macperl = PerlEnv_getenv("MACPERL");
	
	if (!macperl)
	    macperl = "";
	
	Perl_sv_setpvf(aTHX_ privdir, "%slib:", macperl);
	if (PerlLIO_stat(SvPVX(privdir), &tmpstatbuf) >= 0 && S_ISDIR(tmpstatbuf.st_mode))
	    incpush(SvPVX(privdir), TRUE, FALSE, TRUE, FALSE);
	Perl_sv_setpvf(aTHX_ privdir, "%ssite_perl:", macperl);
	if (PerlLIO_stat(SvPVX(privdir), &tmpstatbuf) >= 0 && S_ISDIR(tmpstatbuf.st_mode))
	    incpush(SvPVX(privdir), TRUE, FALSE, TRUE, FALSE);
	
   	SvREFCNT_dec(privdir);
    }
    if (!PL_tainting)
	incpush(":", FALSE, FALSE, TRUE, FALSE);
#else
#ifndef PRIVLIB_EXP
#  define PRIVLIB_EXP "/usr/local/lib/perl5:/usr/local/lib/perl"
#endif
#if defined(WIN32)
    incpush(PRIVLIB_EXP, TRUE, FALSE, TRUE, TRUE);
#else
    incpush(PRIVLIB_EXP, FALSE, FALSE, TRUE, TRUE);
#endif

#ifdef SITEARCH_EXP
    /* sitearch is always relative to sitelib on Windows for
     * DLL-based path intuition to work correctly */
#  if !defined(WIN32)
    incpush(SITEARCH_EXP, FALSE, FALSE, TRUE, TRUE);
#  endif
#endif

#ifdef SITELIB_EXP
#  if defined(WIN32)
    /* this picks up sitearch as well */
    incpush(SITELIB_EXP, TRUE, FALSE, TRUE, TRUE);
#  else
    incpush(SITELIB_EXP, FALSE, FALSE, TRUE, TRUE);
#  endif
#endif

#if defined(SITELIB_STEM) && defined(PERL_INC_VERSION_LIST)
    /* Search for version-specific dirs below here */
    incpush(SITELIB_STEM, FALSE, TRUE, TRUE, TRUE);
#endif

#ifdef PERL_VENDORARCH_EXP
    /* vendorarch is always relative to vendorlib on Windows for
     * DLL-based path intuition to work correctly */
#  if !defined(WIN32)
    incpush(PERL_VENDORARCH_EXP, FALSE, FALSE, TRUE, TRUE);
#  endif
#endif

#ifdef PERL_VENDORLIB_EXP
#  if defined(WIN32)
    incpush(PERL_VENDORLIB_EXP, TRUE, FALSE, TRUE, TRUE);	/* this picks up vendorarch as well */
#  else
    incpush(PERL_VENDORLIB_EXP, FALSE, FALSE, TRUE, TRUE);
#  endif
#endif

#ifdef PERL_VENDORLIB_STEM /* Search for version-specific dirs below here */
    incpush(PERL_VENDORLIB_STEM, FALSE, TRUE, TRUE, TRUE);
#endif

#ifdef PERL_OTHERLIBDIRS
    incpush(PERL_OTHERLIBDIRS, TRUE, TRUE, TRUE, TRUE);
#endif

    if (!PL_tainting)
	incpush(".", FALSE, FALSE, TRUE, FALSE);
#endif /* MACOS_TRADITIONAL */
}

#if defined(DOSISH) || defined(EPOC) || defined(__SYMBIAN32__)
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

/* Push a directory onto @INC if it exists.
   Generate a new SV if we do this, to save needing to copy the SV we push
   onto @INC  */
STATIC SV *
S_incpush_if_exists(pTHX_ SV *dir)
{
    dVAR;
    Stat_t tmpstatbuf;

    PERL_ARGS_ASSERT_INCPUSH_IF_EXISTS;

    if (PerlLIO_stat(SvPVX_const(dir), &tmpstatbuf) >= 0 &&
	S_ISDIR(tmpstatbuf.st_mode)) {
	av_push(GvAVn(PL_incgv), dir);
	dir = newSV(0);
    }
    return dir;
}

STATIC void
S_incpush(pTHX_ const char *dir, bool addsubdirs, bool addoldvers, bool usesep,
	  bool canrelocate)
{
    dVAR;
    SV *subdir = NULL;
    const char *p = dir;

    if (!p || !*p)
	return;

    if (addsubdirs || addoldvers) {
	subdir = newSV(0);
    }

    /* Break at all separators */
    while (p && *p) {
	SV *libdir = newSV(0);
        const char *s;

	/* skip any consecutive separators */
	if (usesep) {
	    while ( *p == PERLLIB_SEP ) {
		/* Uncomment the next line for PATH semantics */
		/* av_push(GvAVn(PL_incgv), newSVpvs(".")); */
		p++;
	    }
	}

	if ( usesep && (s = strchr(p, PERLLIB_SEP)) != NULL ) {
	    sv_setpvn(libdir, PERLLIB_MANGLE(p, (STRLEN)(s - p)),
		      (STRLEN)(s - p));
	    p = s + 1;
	}
	else {
	    sv_setpv(libdir, PERLLIB_MANGLE(p, 0));
	    p = NULL;	/* break out */
	}
#ifdef MACOS_TRADITIONAL
	if (!strchr(SvPVX(libdir), ':')) {
	    char buf[256];

	    sv_setpv(libdir, MacPerl_CanonDir(SvPVX(libdir), buf, 0));
	}
	if (SvPVX(libdir)[SvCUR(libdir)-1] != ':')
	    sv_catpvs(libdir, ":");
#endif

	/* Do the if() outside the #ifdef to avoid warnings about an unused
	   parameter.  */
	if (canrelocate) {
#ifdef PERL_RELOCATABLE_INC
	/*
	 * Relocatable include entries are marked with a leading .../
	 *
	 * The algorithm is
	 * 0: Remove that leading ".../"
	 * 1: Remove trailing executable name (anything after the last '/')
	 *    from the perl path to give a perl prefix
	 * Then
	 * While the @INC element starts "../" and the prefix ends with a real
	 * directory (ie not . or ..) chop that real directory off the prefix
	 * and the leading "../" from the @INC element. ie a logical "../"
	 * cleanup
	 * Finally concatenate the prefix and the remainder of the @INC element
	 * The intent is that /usr/local/bin/perl and .../../lib/perl5
	 * generates /usr/local/lib/perl5
	 */
	    const char *libpath = SvPVX(libdir);
	    STRLEN libpath_len = SvCUR(libdir);
	    if (libpath_len >= 4 && memEQ (libpath, ".../", 4)) {
		/* Game on!  */
		SV * const caret_X = get_sv("\030", 0);
		/* Going to use the SV just as a scratch buffer holding a C
		   string:  */
		SV *prefix_sv;
		char *prefix;
		char *lastslash;

		/* $^X is *the* source of taint if tainting is on, hence
		   SvPOK() won't be true.  */
		assert(caret_X);
		assert(SvPOKp(caret_X));
		prefix_sv = newSVpvn_flags(SvPVX(caret_X), SvCUR(caret_X),
					   SvUTF8(caret_X));
		/* Firstly take off the leading .../
		   If all else fail we'll do the paths relative to the current
		   directory.  */
		sv_chop(libdir, libpath + 4);
		/* Don't use SvPV as we're intentionally bypassing taining,
		   mortal copies that the mg_get of tainting creates, and
		   corruption that seems to come via the save stack.
		   I guess that the save stack isn't correctly set up yet.  */
		libpath = SvPVX(libdir);
		libpath_len = SvCUR(libdir);

		/* This would work more efficiently with memrchr, but as it's
		   only a GNU extension we'd need to probe for it and
		   implement our own. Not hard, but maybe not worth it?  */

		prefix = SvPVX(prefix_sv);
		lastslash = strrchr(prefix, '/');

		/* First time in with the *lastslash = '\0' we just wipe off
		   the trailing /perl from (say) /usr/foo/bin/perl
		*/
		if (lastslash) {
		    SV *tempsv;
		    while ((*lastslash = '\0'), /* Do that, come what may.  */
			   (libpath_len >= 3 && memEQ(libpath, "../", 3)
			    && (lastslash = strrchr(prefix, '/')))) {
			if (lastslash[1] == '\0'
			    || (lastslash[1] == '.'
				&& (lastslash[2] == '/' /* ends "/."  */
				    || (lastslash[2] == '/'
					&& lastslash[3] == '/' /* or "/.."  */
					)))) {
			    /* Prefix ends "/" or "/." or "/..", any of which
			       are fishy, so don't do any more logical cleanup.
			    */
			    break;
			}
			/* Remove leading "../" from path  */
			libpath += 3;
			libpath_len -= 3;
			/* Next iteration round the loop removes the last
			   directory name from prefix by writing a '\0' in
			   the while clause.  */
		    }
		    /* prefix has been terminated with a '\0' to the correct
		       length. libpath points somewhere into the libdir SV.
		       We need to join the 2 with '/' and drop the result into
		       libdir.  */
		    tempsv = Perl_newSVpvf(aTHX_ "%s/%s", prefix, libpath);
		    SvREFCNT_dec(libdir);
		    /* And this is the new libdir.  */
		    libdir = tempsv;
		    if (PL_tainting &&
			(PL_uid != PL_euid || PL_gid != PL_egid)) {
			/* Need to taint reloccated paths if running set ID  */
			SvTAINTED_on(libdir);
		    }
		}
		SvREFCNT_dec(prefix_sv);
	    }
#endif
	}
	/*
	 * BEFORE pushing libdir onto @INC we may first push version- and
	 * archname-specific sub-directories.
	 */
	if (addsubdirs || addoldvers) {
#ifdef PERL_INC_VERSION_LIST
	    /* Configure terminates PERL_INC_VERSION_LIST with a NULL */
	    const char * const incverlist[] = { PERL_INC_VERSION_LIST };
	    const char * const *incver;
#endif
#ifdef VMS
	    char *unix;
	    STRLEN len;

	    if ((unix = tounixspec_ts(SvPV(libdir,len),NULL)) != NULL) {
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
#define PERL_ARCH_FMT_PREFIX	""
#define PERL_ARCH_FMT_SUFFIX	":"
#define PERL_ARCH_FMT_PATH	PERL_FS_VERSION ""
#else
#define PERL_ARCH_FMT_PREFIX	"/"
#define PERL_ARCH_FMT_SUFFIX	""
#define PERL_ARCH_FMT_PATH	"/" PERL_FS_VERSION
#endif
		/* .../version/archname if -d .../version/archname */
		sv_setsv(subdir, libdir);
		sv_catpvs(subdir, PERL_ARCH_FMT_PATH \
			  PERL_ARCH_FMT_PREFIX ARCHNAME PERL_ARCH_FMT_SUFFIX);
		subdir = S_incpush_if_exists(aTHX_ subdir);

		/* .../version if -d .../version */
		sv_setsv(subdir, libdir);
		sv_catpvs(subdir, PERL_ARCH_FMT_PATH);
		subdir = S_incpush_if_exists(aTHX_ subdir);

		/* .../archname if -d .../archname */
		sv_setsv(subdir, libdir);
		sv_catpvs(subdir,
			  PERL_ARCH_FMT_PREFIX ARCHNAME PERL_ARCH_FMT_SUFFIX);
		subdir = S_incpush_if_exists(aTHX_ subdir);

	    }

#ifdef PERL_INC_VERSION_LIST
	    if (addoldvers) {
		for (incver = incverlist; *incver; incver++) {
		    /* .../xxx if -d .../xxx */
		    Perl_sv_setpvf(aTHX_ subdir, "%"SVf PERL_ARCH_FMT_PREFIX \
				   "%s" PERL_ARCH_FMT_SUFFIX,
				   SVfARG(libdir), *incver);
		    subdir = S_incpush_if_exists(aTHX_ subdir);
		}
	    }
#endif
	}

	/* finally push this lib directory on the end of @INC */
	av_push(GvAVn(PL_incgv), libdir);
    }
    if (subdir) {
	assert (SvREFCNT(subdir) == 1);
	SvREFCNT_dec(subdir);
    }
}


void
Perl_call_list(pTHX_ I32 oldscope, AV *paramList)
{
    dVAR;
    SV *atsv;
    volatile const line_t oldline = PL_curcop ? CopLINE(PL_curcop) : 0;
    CV *cv;
    STRLEN len;
    int ret;
    dJMPENV;

    PERL_ARGS_ASSERT_CALL_LIST;

    while (av_len(paramList) >= 0) {
	cv = MUTABLE_CV(av_shift(paramList));
	if (PL_savebegin) {
	    if (paramList == PL_beginav) {
		/* save PL_beginav for compiler */
		Perl_av_create_and_push(aTHX_ &PL_beginav_save, MUTABLE_SV(cv));
	    }
	    else if (paramList == PL_checkav) {
		/* save PL_checkav for compiler */
		Perl_av_create_and_push(aTHX_ &PL_checkav_save, MUTABLE_SV(cv));
	    }
	    else if (paramList == PL_unitcheckav) {
		/* save PL_unitcheckav for compiler */
		Perl_av_create_and_push(aTHX_ &PL_unitcheckav_save, MUTABLE_SV(cv));
	    }
	} else {
	    if (!PL_madskills)
		SAVEFREESV(cv);
	}
	JMPENV_PUSH(ret);
	switch (ret) {
	case 0:
#ifdef PERL_MAD
	    if (PL_madskills)
		PL_madskills |= 16384;
#endif
	    CALL_LIST_BODY(cv);
#ifdef PERL_MAD
	    if (PL_madskills)
		PL_madskills &= ~16384;
#endif
	    atsv = ERRSV;
	    (void)SvPV_const(atsv, len);
	    if (len) {
		PL_curcop = &PL_compiling;
		CopLINE_set(PL_curcop, oldline);
		if (paramList == PL_beginav)
		    sv_catpvs(atsv, "BEGIN failed--compilation aborted");
		else
		    Perl_sv_catpvf(aTHX_ atsv,
				   "%s failed--call queue aborted",
				   paramList == PL_checkav ? "CHECK"
				   : paramList == PL_initav ? "INIT"
				   : paramList == PL_unitcheckav ? "UNITCHECK"
				   : "END");
		while (PL_scopestack_ix > oldscope)
		    LEAVE;
		JMPENV_POP;
		Perl_croak(aTHX_ "%"SVf"", SVfARG(atsv));
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
			       : paramList == PL_unitcheckav ? "UNITCHECK"
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

void
Perl_my_exit(pTHX_ U32 status)
{
    dVAR;
    DEBUG_S(PerlIO_printf(Perl_debug_log, "my_exit: thread %p, status %lu\n",
			  (void*)thr, (unsigned long) status));
    switch (status) {
    case 0:
	STATUS_ALL_SUCCESS;
	break;
    case 1:
	STATUS_ALL_FAILURE;
	break;
    default:
	STATUS_EXIT_SET(status);
	break;
    }
    my_exit_jump();
}

void
Perl_my_failure_exit(pTHX)
{
    dVAR;
#ifdef VMS
     /* We have been called to fall on our sword.  The desired exit code
      * should be already set in STATUS_UNIX, but could be shifted over
      * by 8 bits.  STATUS_UNIX_EXIT_SET will handle the cases where a
      * that code is set.
      *
      * If an error code has not been set, then force the issue.
      */
    if (MY_POSIX_EXIT) {

        /* According to the die_exit.t tests, if errno is non-zero */
        /* It should be used for the error status. */

	if (errno == EVMSERR) {
	    STATUS_NATIVE = vaxc$errno;
	} else {

            /* According to die_exit.t tests, if the child_exit code is */
            /* also zero, then we need to exit with a code of 255 */
            if ((errno != 0) && (errno < 256))
		STATUS_UNIX_EXIT_SET(errno);
            else if (STATUS_UNIX < 255) {
		STATUS_UNIX_EXIT_SET(255);
            }

	}

	/* The exit code could have been set by $? or vmsish which
	 * means that it may not have fatal set.  So convert
	 * success/warning codes to fatal with out changing
	 * the POSIX status code.  The severity makes VMS native
	 * status handling work, while UNIX mode programs use the
	 * the POSIX exit codes.
	 */
	 if ((STATUS_NATIVE & (STS$K_SEVERE|STS$K_ERROR)) == 0) {
	    STATUS_NATIVE &= STS$M_COND_ID;
	    STATUS_NATIVE |= STS$K_ERROR | STS$M_INHIB_MSG;
         }
    }
    else {
	/* Traditionally Perl on VMS always expects a Fatal Error. */
	if (vaxc$errno & 1) {

	    /* So force success status to failure */
	    if (STATUS_NATIVE & 1)
		STATUS_ALL_FAILURE;
	}
	else {
	    if (!vaxc$errno) {
		STATUS_UNIX = EINTR; /* In case something cares */
		STATUS_ALL_FAILURE;
	    }
	    else {
		int severity;
		STATUS_NATIVE = vaxc$errno; /* Should already be this */

		/* Encode the severity code */
		severity = STATUS_NATIVE & STS$M_SEVERITY;
		STATUS_UNIX = (severity ? severity : 1) << 8;

		/* Perl expects this to be a fatal error */
		if (severity != STS$K_SEVERE)
		    STATUS_ALL_FAILURE;
	    }
	}
    }

#else
    int exitstatus;
    if (errno & 255)
	STATUS_UNIX_SET(errno);
    else {
	exitstatus = STATUS_UNIX >> 8;
	if (exitstatus & 255)
	    STATUS_UNIX_SET(exitstatus);
	else
	    STATUS_UNIX_SET(255);
    }
#endif
    my_exit_jump();
}

STATIC void
S_my_exit_jump(pTHX)
{
    dVAR;

    if (PL_e_script) {
	SvREFCNT_dec(PL_e_script);
	PL_e_script = NULL;
    }

    POPSTACK_TO(PL_mainstack);
    dounwind(-1);
    LEAVE_SCOPE(0);

    JMPENV_JUMP(2);
}

static I32
read_e_script(pTHX_ int idx, SV *buf_sv, int maxlen)
{
    dVAR;
    const char * const p  = SvPVX_const(PL_e_script);
    const char *nl = strchr(p, '\n');

    PERL_UNUSED_ARG(idx);
    PERL_UNUSED_ARG(maxlen);

    nl = (nl) ? nl+1 : SvEND(PL_e_script);
    if (nl-p == 0) {
	filter_del(read_e_script);
	return 0;
    }
    sv_catpvn(buf_sv, p, nl-p);
    sv_chop(PL_e_script, nl);
    return 1;
}

/*
 * Local variables:
 * c-indentation-style: bsd
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 *
 * ex: set ts=8 sts=4 sw=4 noet:
 */
