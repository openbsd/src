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
#  define _WIN32_WINNT 0x0400     /* needed for TryEnterCriticalSection() etc. */
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

/* Define USE_SOCKETS_AS_HANDLES to enable emulation of windows sockets as
 * real filehandles. XXX Should always be defined (the other version is untested) */
#define USE_SOCKETS_AS_HANDLES

/* read() and write() aren't transparent for socket handles */
#define PERL_SOCK_SYSREAD_IS_RECV
#define PERL_SOCK_SYSWRITE_IS_SEND

#define PERL_NO_FORCE_LINK		/* no need for PL_force_link_funcs */

/* Define USE_FIXED_OSFHANDLE to fix MSVCRT's _open_osfhandle() on W95.
   It now uses some black magic to work seamlessly with the DLL CRT and
   works with MSVC++ 4.0+ or GCC/Mingw32
	-- BKS 1-24-2000
   Only use this fix for VC++ 6.x or earlier (and for GCC, which we assume
   uses MSVCRT.DLL). Later versions use MSVCR70.dll, MSVCR71.dll, etc, which
   do not require the fix. */
#if (defined(_M_IX86) && _MSC_VER >= 1000 && _MSC_VER <= 1200) || defined(__MINGW32__)
#define USE_FIXED_OSFHANDLE
#endif

/* Define PERL_WIN32_SOCK_DLOAD to have Perl dynamically load the winsock
   DLL when needed. Don't use if your compiler supports delayloading (ie, VC++ 6.0)
	-- BKS 5-29-2000 */
#if !(defined(_M_IX86) && _MSC_VER >= 1200)
#define PERL_WIN32_SOCK_DLOAD
#endif
#define ENV_IS_CASELESS

#define PIPESOCK_MODE	"b"		/* pipes, sockets default to binmode */

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

/* for waitpid() */
#ifndef WNOHANG
#  define WNOHANG	1
#endif

#define PERL_GET_CONTEXT_DEFINED

/* Compiler-specific stuff. */

#if defined(_MSC_VER) || defined(__MINGW32__)
/* VC uses non-standard way to determine the size and alignment if bit-fields */
/* MinGW will compiler with -mms-bitfields, so should use the same types */
#  define PERL_BITFIELD8  unsigned char
#  define PERL_BITFIELD16 unsigned short
#  define PERL_BITFIELD32 unsigned int
#endif

#ifdef __BORLANDC__		/* Borland C++ */

#if (__BORLANDC__ <= 0x520)
#define _access access
#define _chdir chdir
#endif

#define _getpid getpid
#define wcsicmp _wcsicmp
#include <sys/types.h>

#ifndef DllMain
#define DllMain DllEntryPoint
#endif

#pragma warn -8004	/* "'foo' is assigned a value that is never used" */
#pragma warn -8008	/* "condition is always true/false" */
#pragma warn -8012	/* "comparing signed and unsigned values" */
#pragma warn -8027	/* "functions containing %s are not expanded inline" */
#pragma warn -8057	/* "parameter 'foo' is never used" */
#pragma warn -8060	/* "possibly incorrect assignment" */
#pragma warn -8066	/* "unreachable code" */
#pragma warn -8071	/* "conversion may lose significant digits" */
#pragma warn -8080	/* "'foo' is declared but never used" */

/* Borland C thinks that a pointer to a member variable is 12 bytes in size. */
#define PERL_MEMBER_PTR_SIZE	12

#define isnan		_isnan

#endif

#ifdef _MSC_VER			/* Microsoft Visual C++ */

#ifndef UNDER_CE
typedef long		uid_t;
typedef long		gid_t;
typedef unsigned short	mode_t;
#endif

#pragma  warning(disable: 4102)	/* "unreferenced label" */

/* Visual C thinks that a pointer to a member variable is 16 bytes in size. */
#define PERL_MEMBER_PTR_SIZE	16

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

#endif /* __MINGW32__ */

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

/* For UNIX compatibility. */

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

#ifndef USE_SOCKETS_AS_HANDLES
extern FILE *		my_fdopen(int, char *);
#endif
extern int		my_fclose(FILE *);
extern int		my_fstat(int fd, Stat_t *sbufptr);
extern char *		win32_get_privlib(const char *pl, STRLEN *const len);
extern char *		win32_get_sitelib(const char *pl, STRLEN *const len);
extern char *		win32_get_vendorlib(const char *pl, STRLEN *const len);
extern int		IsWin95(void);
extern int		IsWinNT(void);

#ifdef PERL_IMPLICIT_SYS
extern void		win32_delete_internal_host(void *h);
#endif

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
#if defined(__MINGW32__) && (__MINGW32_MAJOR_VERSION>=3)
#undef _CRTIMP
#endif
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

#define EXEC_ARGV_CAST(x) ((const char *const *) x)

#if !defined(ECONNABORTED) && defined(WSAECONNABORTED)
#define ECONNABORTED WSAECONNABORTED
#endif
#if !defined(ECONNRESET) && defined(WSAECONNRESET)
#define ECONNRESET WSAECONNRESET
#endif
#if !defined(EAFNOSUPPORT) && defined(WSAEAFNOSUPPORT)
#define EAFNOSUPPORT WSAEAFNOSUPPORT
#endif
/* Why not needed for ECONNREFUSED? --abe */

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

