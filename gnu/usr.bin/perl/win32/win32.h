/* WIN32.H
 *
 * (c) 1995 Microsoft Corporation. All rights reserved.
 * 		Developed by hip communications inc.
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */
#ifndef  _INC_WIN32_PERL5
#define  _INC_WIN32_PERL5

#ifndef _WIN32_WINNT
#  define _WIN32_WINNT 0x0500     /* needed for CreateHardlink() etc. */
#endif

#ifdef PERL_IS_MINIPERL
/* this macro will remove Winsock only on miniperl, PERL_IMPLICIT_SYS and
 * makedef.pl create dependencies that will keep Winsock linked in even with
 * this macro defined, even though sockets will be umimplemented from a script
 * level in full perl
 */
#  define WIN32_NO_SOCKETS
#endif

#ifdef WIN32_NO_SOCKETS
#  undef HAS_SOCKET
#  undef HAS_GETPROTOBYNAME
#  undef HAS_GETPROTOBYNUMBER
#  undef HAS_GETPROTOENT
#  undef HAS_GETNETBYNAME
#  undef HAS_GETNETBYADDR
#  undef HAS_GETNETENT
#  undef HAS_GETSERVBYNAME
#  undef HAS_GETSERVBYPORT
#  undef HAS_GETSERVENT
#  undef HAS_GETHOSTBYNAME
#  undef HAS_GETHOSTBYADDR
#  undef HAS_GETHOSTENT
#  undef HAS_SELECT
#  undef HAS_IOCTL
#  undef HAS_NTOHL
#  undef HAS_HTONL
#  undef HAS_HTONS
#  undef HAS_NTOHS
#  define WIN32SCK_IS_STDSCK
#endif

#if defined(PERL_IMPLICIT_SYS)
#  define DYNAMIC_ENV_FETCH
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
#ifdef __cplusplus
/* Mingw32 gcc -xc++ objects to __attribute((unused)) at least */
#undef  PERL_UNUSED_DECL
#define PERL_UNUSED_DECL
#endif
#endif


/* Define DllExport akin to perl's EXT,
 * If we are in the DLL then Export the symbol,
 * otherwise import it.
 */

/* now even GCC supports __declspec() */
/* miniperl has no reason to export anything */
#if defined(PERL_IS_MINIPERL) && !defined(UNDER_CE) && defined(_MSC_VER)
#  define DllExport
#else
#  if defined(PERLDLL)
#    define DllExport __declspec(dllexport)
#  else
#    define DllExport __declspec(dllimport)
#  endif
#endif

/* The Perl APIs can only be called directly inside the perl5xx.dll.
 * All other code has to import them.  By declaring them as "dllimport"
 * we tell the compiler to generate an indirect call instruction and
 * avoid redirection through a call thunk.
 *
 * The XS code in the re extension is special, in that it redefines
 * core APIs locally, so don't mark them as "dllimport" because GCC
 * cannot handle this situation.
 */
#if !defined(PERLDLL) && !defined(PERL_EXT_RE_BUILD)
#  ifdef __cplusplus
#    define PERL_CALLCONV extern "C" __declspec(dllimport)
#    ifdef _MSC_VER
#      define PERL_CALLCONV_NO_RET extern "C" __declspec(dllimport) __declspec(noreturn)
#    endif
#  else
#    define PERL_CALLCONV __declspec(dllimport)
#    ifdef _MSC_VER
#      define PERL_CALLCONV_NO_RET __declspec(dllimport) __declspec(noreturn)
#    endif
#  endif
#else /* MSVC noreturn support inside the interp */
#  ifdef _MSC_VER
#    define PERL_CALLCONV_NO_RET __declspec(noreturn)
#  endif
#endif

#ifdef _MSC_VER
#  define PERL_STATIC_NO_RET __declspec(noreturn) static
#  define PERL_STATIC_INLINE_NO_RET __declspec(noreturn) PERL_STATIC_INLINE
#endif

#define  WIN32_LEAN_AND_MEAN
#include <windows.h>

/*
 * Bug in winbase.h in mingw-w64 4.4.0-1 at least... they
 * do #define GetEnvironmentStringsA GetEnvironmentStrings and fail
 * to declare GetEnvironmentStringsA.
 */
#if defined(__MINGW64__) && defined(GetEnvironmentStringsA) && !defined(UNICODE)
#ifdef __cplusplus
extern "C" {
#endif
#undef GetEnvironmentStringsA
WINBASEAPI LPCH WINAPI GetEnvironmentStringsA(VOID);
#define GetEnvironmentStrings GetEnvironmentStringsA
#ifdef __cplusplus
}
#endif
#endif

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
#include <stddef.h>
#include <fcntl.h>
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

/* read() and write() aren't transparent for socket handles */
#ifndef WIN32_NO_SOCKETS
#  define PERL_SOCK_SYSREAD_IS_RECV
#  define PERL_SOCK_SYSWRITE_IS_SEND
#endif

#define PERL_NO_FORCE_LINK		/* no need for PL_force_link_funcs */

#define ENV_IS_CASELESS

#define PIPESOCK_MODE	"b"		/* pipes, sockets default to binmode */

/* access() mode bits */
#ifndef R_OK
#  define	R_OK	4
#  define	W_OK	2
#  define	X_OK	1
#  define	F_OK	0
#endif

/* for waitpid() */
#ifndef WNOHANG
#  define WNOHANG	1
#endif

#define PERL_GET_CONTEXT_DEFINED

/* Compiler-specific stuff. */

/* VC uses non-standard way to determine the size and alignment if bit-fields */
/* MinGW will compile with -mms-bitfields, so should use the same types */
#define PERL_BITFIELD8  unsigned char
#define PERL_BITFIELD16 unsigned short
#define PERL_BITFIELD32 unsigned int

#ifdef _MSC_VER			/* Microsoft Visual C++ */

#ifndef UNDER_CE
typedef long		uid_t;
typedef long		gid_t;
typedef unsigned short	mode_t;
#endif

#pragma  warning(disable: 4102)	/* "unreferenced label" */

#define isnan		_isnan
#define snprintf	_snprintf
#define vsnprintf	_vsnprintf

#if _MSC_VER < 1300
/* VC6 has broken NaN semantics: NaN == NaN returns true instead of false */
#define NAN_COMPARE_BROKEN 1
#endif

#endif /* _MSC_VER */

#ifdef __MINGW32__		/* Minimal Gnu-Win32 */

typedef long		uid_t;
typedef long		gid_t;
#ifndef _environ
#define _environ	environ
#endif
#define flushall	_flushall
#define fcloseall	_fcloseall
#ifndef isnan
#define isnan		_isnan	/* ...same libraries as MSVC */
#endif

#ifndef _O_NOINHERIT
#  define _O_NOINHERIT	0x0080
#  ifndef _NO_OLDNAMES
#    define O_NOINHERIT	_O_NOINHERIT
#  endif
#endif

/* <stdint.h>, pulled in by <io.h> as of mingw-runtime-3.3, typedef's
 * (u)intptr_t but doesn't set the _(U)INTPTR_T_DEFINED defines */
#ifdef _STDINT_H
#  ifndef _INTPTR_T_DEFINED
#    define _INTPTR_T_DEFINED
#  endif
#  ifndef _UINTPTR_T_DEFINED
#    define _UINTPTR_T_DEFINED
#  endif
#endif

#ifndef CP_UTF8
#  define CP_UTF8	65001
#endif

#endif /* __MINGW32__ */

#ifndef _INTPTR_T_DEFINED
typedef int		intptr_t;
#  define _INTPTR_T_DEFINED
#endif

#ifndef _UINTPTR_T_DEFINED
typedef unsigned int	uintptr_t;
#  define _UINTPTR_T_DEFINED
#endif

START_EXTERN_C

/* For UNIX compatibility. */

#ifdef PERL_CORE
extern  uid_t	getuid(void);
extern  gid_t	getgid(void);
extern  uid_t	geteuid(void);
extern  gid_t	getegid(void);
extern  int	setuid(uid_t uid);
extern  int	setgid(gid_t gid);
extern  int	kill(int pid, int sig);
extern  int	killpg(int pid, int sig);
#ifndef USE_PERL_SBRK
extern  void	*sbrk(ptrdiff_t need);
#  define HAS_SBRK_PROTO
#endif
extern	char *	getlogin(void);
extern	int	chown(const char *p, uid_t o, gid_t g);
extern  int	mkstemp(const char *path);
#endif

#undef	 Stat
#define  Stat		win32_stat

#undef   init_os_extras
#define  init_os_extras Perl_init_os_extras

DllExport void		Perl_win32_init(int *argcp, char ***argvp);
DllExport void		Perl_win32_term(void);
DllExport void		Perl_init_os_extras(void);
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
DllExport HWND		win32_create_message_window(void);

extern int		my_fclose(FILE *);
extern char *		win32_get_privlib(const char *pl, STRLEN *const len);
extern char *		win32_get_sitelib(const char *pl, STRLEN *const len);
extern char *		win32_get_vendorlib(const char *pl, STRLEN *const len);

#ifdef PERL_IMPLICIT_SYS
extern void		win32_delete_internal_host(void *h);
#endif

extern const char * const		staticlinkmodules[];

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

#ifdef PERL_TEXTMODE_SCRIPTS
#  define PERL_SCRIPT_MODE		"r"
#else
#  define PERL_SCRIPT_MODE		"rb"
#endif

/*
 * Now Win32 specific per-thread data stuff
 */

/* Leave the first couple ids after WM_USER unused because they
 * might be used by an embedding application, and on Windows
 * version before 2000 we might end up eating those messages
 * if they were not meant for us.
 */
#define WM_USER_MIN     (WM_USER+30)
#define WM_USER_MESSAGE (WM_USER_MIN)
#define WM_USER_KILL    (WM_USER_MIN+1)
#define WM_USER_MAX     (WM_USER_MIN+1)

struct thread_intern {
    /* XXX can probably use one buffer instead of several */
    char		Wstrerror_buffer[512];
    struct servent	Wservent;
    char		Wgetlogin_buffer[128];
    int			Winit_socktype;
    char		Wcrypt_buffer[30];
#    ifdef USE_RTL_THREAD_API
    void *		retv;	/* slot for thread return value */
#    endif
    BOOL               Wuse_showwindow;
    WORD               Wshowwindow;
};

#define HAVE_INTERP_INTERN
typedef struct {
    long	num;
    DWORD	pids[MAXIMUM_WAIT_OBJECTS];
    HANDLE	handles[MAXIMUM_WAIT_OBJECTS];
} child_tab;

#ifdef USE_ITHREADS
typedef struct {
    long	num;
    DWORD	pids[MAXIMUM_WAIT_OBJECTS];
    HANDLE	handles[MAXIMUM_WAIT_OBJECTS];
    HWND	message_hwnds[MAXIMUM_WAIT_OBJECTS];
    char        sigterm[MAXIMUM_WAIT_OBJECTS];
} pseudo_child_tab;
#endif

#ifndef Sighandler_t
typedef Signal_t (*Sighandler_t) (int);
#define Sighandler_t	Sighandler_t
#endif

struct interp_intern {
    char *	perlshell_tokens;
    char **	perlshell_vec;
    long	perlshell_items;
    struct av *	fdpid;
    child_tab *	children;
#ifdef USE_ITHREADS
    DWORD	pseudo_id;
    pseudo_child_tab * pseudo_children;
#endif
    void *	internal_host;
    struct thread_intern	thr_intern;
    HWND        message_hwnd;
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
#define w32_pseudo_child_message_hwnds	(w32_pseudo_children->message_hwnds)
#define w32_pseudo_child_sigterm	(w32_pseudo_children->sigterm)
#define w32_internal_host		(PL_sys_intern.internal_host)
#define w32_timerid			(PL_sys_intern.timerid)
#define w32_message_hwnd		(PL_sys_intern.message_hwnd)
#define w32_sighandler			(PL_sys_intern.sigtable)
#define w32_poll_count			(PL_sys_intern.poll_count)
#define w32_do_async			(w32_poll_count++ > WIN32_POLL_INTERVAL)
#define w32_strerror_buffer	(PL_sys_intern.thr_intern.Wstrerror_buffer)
#define w32_getlogin_buffer	(PL_sys_intern.thr_intern.Wgetlogin_buffer)
#define w32_crypt_buffer	(PL_sys_intern.thr_intern.Wcrypt_buffer)
#define w32_servent		(PL_sys_intern.thr_intern.Wservent)
#define w32_init_socktype	(PL_sys_intern.thr_intern.Winit_socktype)
#define w32_use_showwindow	(PL_sys_intern.thr_intern.Wuse_showwindow)
#define w32_showwindow	(PL_sys_intern.thr_intern.Wshowwindow)

#ifdef USE_ITHREADS
void win32_wait_for_children(pTHX);
#  define PERL_WAIT_FOR_CHILDREN win32_wait_for_children(aTHX)
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

#define EXEC_ARGV_CAST(x) ((const char *const *) x)

DllExport void *win32_signal_context(void);
#define PERL_GET_SIG_CONTEXT win32_signal_context()

#ifdef UNDER_CE
#define Win_GetModuleHandle   XCEGetModuleHandleA
#define Win_GetProcAddress    XCEGetProcAddressA
#define Win_GetModuleFileName XCEGetModuleFileNameA
#define Win_CreateSemaphore   CreateSemaphoreW
#else
#define Win_GetModuleHandle   GetModuleHandle
#define Win_GetProcAddress    GetProcAddress
#define Win_GetModuleFileName GetModuleFileName
#define Win_CreateSemaphore   CreateSemaphore
#endif

#endif /* _INC_WIN32_PERL5 */

