/* Time-stamp: <01/08/01 20:59:54 keuchel@w2k> */

/* WIN32.H
 *
 * (c) 1995 Microsoft Corporation. All rights reserved. 
 * 		Developed by hip communications inc., http://info.hip.com/info/
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */

#ifndef  _INC_WIN32_PERL5
#define  _INC_WIN32_PERL5

#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0400     /* needed for TryEnterCriticalSection() etc. */
#endif

#if defined(PERL_IMPLICIT_SYS)
#  define DYNAMIC_ENV_FETCH
#  define ENV_HV_NAME "___ENV_HV_NAME___"
#  define HAS_GETENV_LEN
#  define prime_env_iter()
#  define WIN32IO_IS_STDIO		/* don't pull in custom stdio layer */
#  define WIN32SCK_IS_STDSCK		/* don't pull in custom wsock layer */
#  ifdef PERL_GLOBAL_STRUCT
#    error PERL_GLOBAL_STRUCT cannot be defined with PERL_IMPLICIT_SYS
#  endif
#  define win32_get_privlib PerlEnv_lib_path
#  define win32_get_sitelib PerlEnv_sitelib_path
#  define win32_get_vendorlib PerlEnv_vendorlib_path
#endif

#ifdef __GNUC__
#  ifndef __int64		/* some versions seem to #define it already */
#    define __int64 long long
#  endif
#  define Win32_Winsock
#endif

/* Define DllExport akin to perl's EXT, 
 * If we are in the DLL or mimicing the DLL for Win95 work round
 * then Export the symbol, 
 * otherwise import it.
 */

/* now even GCC supports __declspec() */

#if defined(PERLDLL) || defined(WIN95FIX)
#define DllExport
/*#define DllExport __declspec(dllexport)*/	/* noises with VC5+sp3 */
#else 
#define DllExport __declspec(dllimport)
#endif

#define  WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef   WIN32_LEAN_AND_MEAN		/* C file is NOT a Perl5 original. */
#define  CONTEXT	PERL_CONTEXT	/* Avoid conflict of CONTEXT defs. */
#endif /*WIN32_LEAN_AND_MEAN */

#ifndef TLS_OUT_OF_INDEXES
#define TLS_OUT_OF_INDEXES (DWORD)0xFFFFFFFF
#endif

#include <dirent.h>
#ifndef UNDER_CE
#include <io.h>
#include <process.h>
#include <direct.h>
#include <fcntl.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#ifndef EXT
#include "EXTERN.h"
#endif

struct tms {
	long	tms_utime;
	long	tms_stime;
	long	tms_cutime;
	long	tms_cstime;
};

#ifndef SYS_NMLN
#define SYS_NMLN	257
#endif

struct utsname {
    char sysname[SYS_NMLN];
    char nodename[SYS_NMLN];
    char release[SYS_NMLN];
    char version[SYS_NMLN];
    char machine[SYS_NMLN];
};

#ifndef START_EXTERN_C
#undef EXTERN_C
#ifdef __cplusplus
#  define START_EXTERN_C extern "C" {
#  define END_EXTERN_C }
#  define EXTERN_C extern "C"
#else
#  define START_EXTERN_C 
#  define END_EXTERN_C 
#  define EXTERN_C
#endif
#endif

#define  STANDARD_C	1
#define  DOSISH		1		/* no escaping our roots */
#define  OP_BINARY	O_BINARY	/* mistake in in pp_sys.c? */

/* Define USE_SOCKETS_AS_HANDLES to enable emulation of windows sockets as
 * real filehandles. XXX Should always be defined (the other version is untested) */

#define USE_SOCKETS_AS_HANDLES

/* read() and write() aren't transparent for socket handles */
#define PERL_SOCK_SYSREAD_IS_RECV
#define PERL_SOCK_SYSWRITE_IS_SEND

#define PERL_NO_FORCE_LINK		/* no need for PL_force_link_funcs */

/* if USE_WIN32_RTL_ENV is not defined, Perl uses direct Win32 calls
 * to read the environment, bypassing the runtime's (usually broken)
 * facilities for accessing the same.  See note in util.c/my_setenv(). */
/*#define USE_WIN32_RTL_ENV */

/* Define USE_FIXED_OSFHANDLE to fix MSVCRT's _open_osfhandle() on W95.
   It now uses some black magic to work seamlessly with the DLL CRT and
   works with MSVC++ 4.0+ or GCC/Mingw32
	-- BKS 1-24-2000 */
#if (defined(_M_IX86) && _MSC_VER >= 1000) || defined(__MINGW32__)
#define USE_FIXED_OSFHANDLE
#endif

#define ENV_IS_CASELESS

#ifndef VER_PLATFORM_WIN32_WINDOWS	/* VC-2.0 headers don't have this */
#define VER_PLATFORM_WIN32_WINDOWS	1
#endif

#ifndef FILE_SHARE_DELETE		/* VC-4.0 headers don't have this */
#define FILE_SHARE_DELETE		0x00000004
#endif

/* access() mode bits */
#ifndef R_OK
#  define	R_OK	4
#  define	W_OK	2
#  define	X_OK	1
#  define	F_OK	0
#endif

#define PERL_GET_CONTEXT_DEFINED

/* Compiler-specific stuff. */

#ifdef __BORLANDC__		/* Borland C++ */

#define _access access
#define _chdir chdir
#define _getpid getpid
#define wcsicmp _wcsicmp
#include <sys/types.h>

#ifndef DllMain
#define DllMain DllEntryPoint
#endif

#pragma warn -ccc	/* "condition is always true/false" */
#pragma warn -rch	/* "unreachable code" */
#pragma warn -sig	/* "conversion may lose significant digits" */
#pragma warn -pia	/* "possibly incorrect assignment" */
#pragma warn -par	/* "parameter 'foo' is never used" */
#pragma warn -aus	/* "'foo' is assigned a value that is never used" */
#pragma warn -use	/* "'foo' is declared but never used" */
#pragma warn -csu	/* "comparing signed and unsigned values" */
#pragma warn -pro	/* "call to function with no prototype" */
#pragma warn -stu	/* "undefined structure 'foo'" */

/* Borland C thinks that a pointer to a member variable is 12 bytes in size. */
#define PERL_MEMBER_PTR_SIZE	12

#endif

#ifdef _MSC_VER			/* Microsoft Visual C++ */

#ifndef _MODE_T_DEFINED_
typedef unsigned long mode_t;
#define _MODE_T_DEFINED_
#endif

#pragma  warning(disable: 4018 4035 4101 4102 4244 4245 4761)

/* Visual C thinks that a pointer to a member variable is 16 bytes in size. */
#define PERL_MEMBER_PTR_SIZE	16

#endif /* _MSC_VER */

#ifdef __MINGW32__		/* Minimal Gnu-Win32 */

typedef long		uid_t;
typedef long		gid_t;
#ifndef _environ
#define _environ	environ
#endif
#define flushall	_flushall
#define fcloseall	_fcloseall

#endif /* __MINGW32__ */

#ifndef _O_NOINHERIT
#  define _O_NOINHERIT	0x0080
#  ifndef _NO_OLDNAMES
#    define O_NOINHERIT	_O_NOINHERIT
#  endif
#endif

/* both GCC/Mingw32 and MSVC++ 4.0 are missing this, so we put it here */
#ifndef CP_UTF8
#  define CP_UTF8	65001
#endif

/* compatibility stuff for other compilers goes here */

#ifndef _INTPTR_T_DEFINED
typedef int		intptr_t;
#  define _INTPTR_T_DEFINED
#endif

#ifndef _UINTPTR_T_DEFINED
typedef unsigned int	uintptr_t;
#  define _UINTPTR_T_DEFINED
#endif

START_EXTERN_C

#undef	 Stat
#define  Stat		win32_stat

#undef   init_os_extras
#define  init_os_extras Perl_init_os_extras

DllExport void		Perl_win32_init(int *argcp, char ***argvp);
DllExport void		Perl_win32_term(void);
DllExport void		Perl_init_os_extras();
DllExport void		win32_str_os_error(void *sv, DWORD err);
DllExport int		RunPerl(int argc, char **argv, char **env);

typedef struct {
    HANDLE	childStdIn;
    HANDLE	childStdOut;
    HANDLE	childStdErr;
    /*
     * the following correspond to the fields of the same name
     * in the STARTUPINFO structure. Embedders can use these to
     * control the spawning process' look.
     * Example - to hide the window of the spawned process:
     *    dwFlags = STARTF_USESHOWWINDOW;
     *	  wShowWindow = SW_HIDE;
     */
    DWORD	dwFlags;
    DWORD	dwX; 
    DWORD	dwY; 
    DWORD	dwXSize; 
    DWORD	dwYSize; 
    DWORD	dwXCountChars; 
    DWORD	dwYCountChars; 
    DWORD	dwFillAttribute;
    WORD	wShowWindow; 
} child_IO_table;

DllExport void		win32_get_child_IO(child_IO_table* ptr);

#ifndef USE_SOCKETS_AS_HANDLES
extern FILE *		my_fdopen(int, char *);
#endif

extern int		my_fclose(FILE *);
extern int		do_aspawn(void *really, void **mark, void **sp);
extern int		do_spawn(char *cmd);
extern int		do_spawn_nowait(char *cmd);
extern char *		win32_get_privlib(const char *pl);
extern char *		win32_get_sitelib(const char *pl);
extern char *		win32_get_vendorlib(const char *pl);
extern int		IsWin95(void);
extern int		IsWinNT(void);
extern void		win32_argv2utf8(int argc, char** argv);

#ifdef PERL_IMPLICIT_SYS
extern void		win32_delete_internal_host(void *h);
#endif

extern char *		staticlinkmodules[];

END_EXTERN_C

/*
 * handle socket stuff, assuming socket is always available
 */

#include <sys/socket.h>
#include <netdb.h>

#ifdef MYMALLOC
#define EMBEDMYMALLOC	/**/
/* #define USE_PERL_SBRK	/**/
/* #define PERL_SBRK_VIA_MALLOC	/**/
#endif

#if defined(PERLDLL) && !defined(PERL_CORE)
#define PERL_CORE
#endif

#ifdef PERL_TEXTMODE_SCRIPTS
#  define PERL_SCRIPT_MODE		"r"
#else
#  define PERL_SCRIPT_MODE		"rb"
#endif

#ifndef Sighandler_t
typedef Signal_t (*Sighandler_t) (int);
#define Sighandler_t	Sighandler_t
#endif

/* 
 * Now Win32 specific per-thread data stuff 
 */

struct thread_intern {
    /* XXX can probably use one buffer instead of several */
    char		Wstrerror_buffer[512];
    struct servent	Wservent;
    char		Wgetlogin_buffer[128];
#    ifdef USE_SOCKETS_AS_HANDLES
    int			Winit_socktype;
#    endif
#    ifdef HAVE_DES_FCRYPT
    char		Wcrypt_buffer[30];
#    endif
#    ifdef USE_RTL_THREAD_API
    void *		retv;	/* slot for thread return value */
#    endif
    BOOL               Wuse_showwindow;
    WORD               Wshowwindow;
};

#ifdef USE_5005THREADS
#  ifndef USE_DECLSPEC_THREAD
#    define HAVE_THREAD_INTERN
#  endif /* !USE_DECLSPEC_THREAD */
#endif /* USE_5005THREADS */

#define HAVE_INTERP_INTERN
typedef struct {
    long	num;
    DWORD	pids[MAXIMUM_WAIT_OBJECTS];
    HANDLE	handles[MAXIMUM_WAIT_OBJECTS];
} child_tab;

struct interp_intern {
    char *	perlshell_tokens;
    char **	perlshell_vec;
    long	perlshell_items;
    struct av *	fdpid;
    child_tab *	children;
#ifdef USE_ITHREADS
    DWORD	pseudo_id;
    child_tab *	pseudo_children;
#endif
    void *	internal_host;
#ifndef USE_5005THREADS
    struct thread_intern	thr_intern;
#endif
    UINT	timerid;
    unsigned 	poll_count;
    Sighandler_t sigtable[SIG_SIZE];
};

DllExport int win32_async_check(pTHX);

#define WIN32_POLL_INTERVAL 32768
#define PERL_ASYNC_CHECK() if (w32_do_async || PL_sig_pending) win32_async_check(aTHX)

#define w32_perlshell_tokens	(PL_sys_intern.perlshell_tokens)
#define w32_perlshell_vec	(PL_sys_intern.perlshell_vec)
#define w32_perlshell_items	(PL_sys_intern.perlshell_items)
#define w32_fdpid		(PL_sys_intern.fdpid)
#define w32_children		(PL_sys_intern.children)
#define w32_num_children	(w32_children->num)
#define w32_child_pids		(w32_children->pids)
#define w32_child_handles	(w32_children->handles)
#define w32_pseudo_id		(PL_sys_intern.pseudo_id)
#define w32_pseudo_children	(PL_sys_intern.pseudo_children)
#define w32_num_pseudo_children		(w32_pseudo_children->num)
#define w32_pseudo_child_pids		(w32_pseudo_children->pids)
#define w32_pseudo_child_handles	(w32_pseudo_children->handles)
#define w32_internal_host		(PL_sys_intern.internal_host)
#define w32_timerid			(PL_sys_intern.timerid)
#define w32_sighandler			(PL_sys_intern.sigtable)
#define w32_poll_count			(PL_sys_intern.poll_count)
#define w32_do_async			(w32_poll_count++ > WIN32_POLL_INTERVAL)
#ifdef USE_5005THREADS
#  define w32_strerror_buffer	(thr->i.Wstrerror_buffer)
#  define w32_getlogin_buffer	(thr->i.Wgetlogin_buffer)
#  define w32_crypt_buffer	(thr->i.Wcrypt_buffer)
#  define w32_servent		(thr->i.Wservent)
#  define w32_init_socktype	(thr->i.Winit_socktype)
#  define w32_use_showwindow	(thr->i.Wuse_showwindow)
#  define w32_showwindow	(thr->i.Wshowwindow)
#else
#  define w32_strerror_buffer	(PL_sys_intern.thr_intern.Wstrerror_buffer)
#  define w32_getlogin_buffer	(PL_sys_intern.thr_intern.Wgetlogin_buffer)
#  define w32_crypt_buffer	(PL_sys_intern.thr_intern.Wcrypt_buffer)
#  define w32_servent		(PL_sys_intern.thr_intern.Wservent)
#  define w32_init_socktype	(PL_sys_intern.thr_intern.Winit_socktype)
#  define w32_use_showwindow	(PL_sys_intern.thr_intern.Wuse_showwindow)
#  define w32_showwindow	(PL_sys_intern.thr_intern.Wshowwindow)
#endif /* USE_5005THREADS */

/* UNICODE<>ANSI translation helpers */
/* Use CP_ACP when mode is ANSI */
/* Use CP_UTF8 when mode is UTF8 */

#define A2WHELPER_LEN(lpa, alen, lpw, nBytes)\
    (lpw[0] = 0, MultiByteToWideChar((IN_BYTES) ? CP_ACP : CP_UTF8, 0, \
				    lpa, alen, lpw, (nBytes/sizeof(WCHAR))))
#define A2WHELPER(lpa, lpw, nBytes)	A2WHELPER_LEN(lpa, -1, lpw, nBytes)

#define W2AHELPER_LEN(lpw, wlen, lpa, nChars)\
    (lpa[0] = '\0', WideCharToMultiByte((IN_BYTES) ? CP_ACP : CP_UTF8, 0, \
				       lpw, wlen, (LPSTR)lpa, nChars,NULL,NULL))
#define W2AHELPER(lpw, lpa, nChars)	W2AHELPER_LEN(lpw, -1, lpa, nChars)

#define USING_WIDE() (0)

#ifdef USE_ITHREADS
#  define PERL_WAIT_FOR_CHILDREN \
    STMT_START {							\
	if (w32_pseudo_children && w32_num_pseudo_children) {		\
	    long children = w32_num_pseudo_children;			\
	    WaitForMultipleObjects(children,				\
				   w32_pseudo_child_handles,		\
				   TRUE, INFINITE);			\
	    while (children)						\
		CloseHandle(w32_pseudo_child_handles[--children]);	\
	}								\
    } STMT_END
#endif

#if defined(USE_FIXED_OSFHANDLE) || defined(PERL_MSVCRT_READFIX)
#ifdef PERL_CORE

/* C doesn't like repeat struct definitions */
#ifndef _CRTIMP
#define _CRTIMP __declspec(dllimport)
#endif

/*
 * Control structure for lowio file handles
 */
typedef struct {
    intptr_t osfhnd;/* underlying OS file HANDLE */
    char osfile;    /* attributes of file (e.g., open in text mode?) */
    char pipech;    /* one char buffer for handles opened on pipes */
    int lockinitflag;
    CRITICAL_SECTION lock;
} ioinfo;


/*
 * Array of arrays of control structures for lowio files.
 */
EXTERN_C _CRTIMP ioinfo* __pioinfo[];

/*
 * Definition of IOINFO_L2E, the log base 2 of the number of elements in each
 * array of ioinfo structs.
 */
#define IOINFO_L2E	    5

/*
 * Definition of IOINFO_ARRAY_ELTS, the number of elements in ioinfo array
 */
#define IOINFO_ARRAY_ELTS   (1 << IOINFO_L2E)

/*
 * Access macros for getting at an ioinfo struct and its fields from a
 * file handle
 */
#define _pioinfo(i) (__pioinfo[(i) >> IOINFO_L2E] + ((i) & (IOINFO_ARRAY_ELTS - 1)))
#define _osfhnd(i)  (_pioinfo(i)->osfhnd)
#define _osfile(i)  (_pioinfo(i)->osfile)
#define _pipech(i)  (_pioinfo(i)->pipech)

/* since we are not doing a dup2(), this works fine */
#define _set_osfhnd(fh, osfh) (void)(_osfhnd(fh) = (intptr_t)osfh)
#endif
#endif

/* IO.xs and POSIX.xs define PERLIO_NOT_STDIO to 1 */
#if defined(PERL_EXT_IO) || defined(PERL_EXT_POSIX)
#undef  PERLIO_NOT_STDIO
#endif
#define PERLIO_NOT_STDIO 0

#include "perlio.h"

/*
 * This provides a layer of functions and macros to ensure extensions will
 * get to use the same RTL functions as the core.
 */
#include "win32iop.h"

#endif /* _INC_WIN32_PERL5 */

