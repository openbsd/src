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

#ifdef PERL_OBJECT
#  define DYNAMIC_ENV_FETCH
#  define ENV_HV_NAME "___ENV_HV_NAME___"
#  define prime_env_iter()
#  define WIN32IO_IS_STDIO		/* don't pull in custom stdio layer */
#  ifdef PERL_GLOBAL_STRUCT
#    error PERL_GLOBAL_STRUCT cannot be defined with PERL_OBJECT
#  endif
#  define win32_get_privlib PerlEnv_lib_path
#  define win32_get_sitelib PerlEnv_sitelib_path
#endif

#ifdef __GNUC__
typedef long long __int64;
#  define Win32_Winsock
/* GCC does not do __declspec() - render it a nop 
 * and turn on options to avoid importing data 
 */
#ifndef __declspec
#  define __declspec(x)
#endif
#  ifndef PERL_OBJECT
#    define PERL_GLOBAL_STRUCT
#    define MULTIPLICITY
#  endif
#endif

/* Define DllExport akin to perl's EXT, 
 * If we are in the DLL or mimicing the DLL for Win95 work round
 * then Export the symbol, 
 * otherwise import it.
 */

#if defined(PERL_OBJECT)
#define DllExport
#else
#if defined(PERLDLL) || defined(WIN95FIX)
#define DllExport
/*#define DllExport __declspec(dllexport)*/	/* noises with VC5+sp3 */
#else 
#define DllExport __declspec(dllimport)
#endif
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
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <direct.h>
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


/* if USE_WIN32_RTL_ENV is not defined, Perl uses direct Win32 calls
 * to read the environment, bypassing the runtime's (usually broken)
 * facilities for accessing the same.  See note in util.c/my_setenv(). */
/*#define USE_WIN32_RTL_ENV */

/* Define USE_FIXED_OSFHANDLE to fix VC's _open_osfhandle() on W95.
 * Can only enable it if not using the DLL CRT (it doesn't expose internals) */
#if defined(_MSC_VER) && !defined(_DLL) && defined(_M_IX86)
#define USE_FIXED_OSFHANDLE
#endif

#define ENV_IS_CASELESS

#ifndef VER_PLATFORM_WIN32_WINDOWS	/* VC-2.0 headers don't have this */
#define VER_PLATFORM_WIN32_WINDOWS	1
#endif

#ifndef FILE_SHARE_DELETE		/* VC-4.0 headers don't have this */
#define FILE_SHARE_DELETE		0x00000004
#endif

/* Compiler-specific stuff. */

#ifdef __BORLANDC__		/* Borland C++ */

#define _access access
#define _chdir chdir
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

#define USE_RTL_WAIT	/* Borland has a working wait() */

/* Borland is picky about a bare member function name used as its ptr */
#ifdef PERL_OBJECT
#define FUNC_NAME_TO_PTR(name)	&(name)
#endif

#endif

#ifdef _MSC_VER			/* Microsoft Visual C++ */

typedef long		uid_t;
typedef long		gid_t;
#pragma  warning(disable: 4018 4035 4101 4102 4244 4245 4761)

#ifndef PERL_OBJECT

/* Visual C thinks that a pointer to a member variable is 16 bytes in size. */
#define STRUCT_MGVTBL_DEFINITION					\
struct mgvtbl {								\
    union {								\
	int	    (CPERLscope(*svt_get))	_((SV *sv, MAGIC* mg));	\
	char	    handle_VC_problem1[16];				\
    };									\
    union {								\
	int	    (CPERLscope(*svt_set))	_((SV *sv, MAGIC* mg));	\
	char	    handle_VC_problem2[16];				\
    };									\
    union {								\
	U32	    (CPERLscope(*svt_len))	_((SV *sv, MAGIC* mg));	\
	char	    handle_VC_problem3[16];				\
    };									\
    union {								\
	int	    (CPERLscope(*svt_clear))	_((SV *sv, MAGIC* mg));	\
	char	    handle_VC_problem4[16];				\
    };									\
    union {								\
	int	    (CPERLscope(*svt_free))	_((SV *sv, MAGIC* mg));	\
	char	    handle_VC_problem5[16];				\
    };									\
}

#define BASEOP_DEFINITION		\
    OP*		op_next;		\
    OP*		op_sibling;		\
    OP*		(CPERLscope(*op_ppaddr))_((ARGSproto));		\
    char	handle_VC_problem[12];	\
    PADOFFSET	op_targ;		\
    OPCODE	op_type;		\
    U16		op_seq;			\
    U8		op_flags;		\
    U8		op_private;

#define UNION_ANY_DEFINITION union any {		\
    void*	any_ptr;				\
    I32		any_i32;				\
    IV		any_iv;					\
    long	any_long;				\
    void	(CPERLscope(*any_dptr)) _((void*));	\
    char	handle_VC_problem[16];			\
}

#endif /* PERL_OBJECT */

#endif /* _MSC_VER */

#ifdef __MINGW32__		/* Minimal Gnu-Win32 */

typedef long		uid_t;
typedef long		gid_t;
#ifndef _environ
#define _environ	environ
#endif
#define flushall	_flushall
#define fcloseall	_fcloseall

#ifdef PERL_OBJECT
#define FUNC_NAME_TO_PTR(name)	&(name)
#endif

#ifndef _O_NOINHERIT
#  define _O_NOINHERIT	0x0080
#  ifndef _NO_OLDNAMES
#    define O_NOINHERIT	_O_NOINHERIT
#  endif
#endif

#ifndef _O_NOINHERIT
#  define _O_NOINHERIT	0x0080
#  ifndef _NO_OLDNAMES
#    define O_NOINHERIT	_O_NOINHERIT
#  endif
#endif

#endif /* __MINGW32__ */

/* compatibility stuff for other compilers goes here */


START_EXTERN_C

/* For UNIX compatibility. */

extern  uid_t	getuid(void);
extern  gid_t	getgid(void);
extern  uid_t	geteuid(void);
extern  gid_t	getegid(void);
extern  int	setuid(uid_t uid);
extern  int	setgid(gid_t gid);
extern  int	kill(int pid, int sig);
extern  void	*sbrk(int need);
extern	char *	getlogin(void);
extern	int	chown(const char *p, uid_t o, gid_t g);

#undef	 Stat
#define  Stat		win32_stat

#undef   init_os_extras
#define  init_os_extras Perl_init_os_extras

DllExport void		Perl_win32_init(int *argcp, char ***argvp);
DllExport void		Perl_init_os_extras(void);
DllExport void		win32_str_os_error(void *sv, DWORD err);

#ifndef USE_SOCKETS_AS_HANDLES
extern FILE *		my_fdopen(int, char *);
#endif
extern int		my_fclose(FILE *);
extern int		do_aspawn(void *really, void **mark, void **sp);
extern int		do_spawn(char *cmd);
extern int		do_spawn_nowait(char *cmd);
extern char		do_exec(char *cmd);
extern char *		win32_get_privlib(char *pl);
extern char *		win32_get_sitelib(char *pl);
extern int		IsWin95(void);
extern int		IsWinNT(void);

extern char *		staticlinkmodules[];

END_EXTERN_C

typedef  char *		caddr_t;	/* In malloc.c (core address). */

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

#ifdef USE_BINMODE_SCRIPTS
#define PERL_SCRIPT_MODE "rb"
EXT void win32_strip_return(struct sv *sv);
#else
#define PERL_SCRIPT_MODE "r"
#define win32_strip_return(sv) NOOP
#endif

#define HAVE_INTERP_INTERN
struct interp_intern {
    char *	w32_perlshell_tokens;
    char **	w32_perlshell_vec;
    long	w32_perlshell_items;
    struct av *	w32_fdpid;
#ifndef USE_RTL_WAIT
    long	w32_num_children;
    HANDLE	w32_child_pids[MAXIMUM_WAIT_OBJECTS];
#endif
};

#define w32_perlshell_tokens	(PL_sys_intern.w32_perlshell_tokens)
#define w32_perlshell_vec	(PL_sys_intern.w32_perlshell_vec)
#define w32_perlshell_items	(PL_sys_intern.w32_perlshell_items)
#define w32_fdpid		(PL_sys_intern.w32_fdpid)

#ifndef USE_RTL_WAIT
#  define w32_num_children	(PL_sys_intern.w32_num_children)
#  define w32_child_pids	(PL_sys_intern.w32_child_pids)
#endif

/* 
 * Now Win32 specific per-thread data stuff 
 */

#ifdef USE_THREADS
#  ifndef USE_DECLSPEC_THREAD
#    define HAVE_THREAD_INTERN

struct thread_intern {
    /* XXX can probably use one buffer instead of several */
    char		Wstrerror_buffer[512];
    struct servent	Wservent;
    char		Wgetlogin_buffer[128];
    char		Ww32_perllib_root[MAX_PATH+1];
#    ifdef USE_SOCKETS_AS_HANDLES
    int			Winit_socktype;
#    endif
#    ifdef HAVE_DES_FCRYPT
    char		Wcrypt_buffer[30];
#    endif
#    ifdef USE_RTL_THREAD_API
    void *		retv;	/* slot for thread return value */
#    endif
};
#  endif /* !USE_DECLSPEC_THREAD */
#endif /* USE_THREADS */

#endif /* _INC_WIN32_PERL5 */
