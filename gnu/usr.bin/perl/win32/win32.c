/* WIN32.C
 *
 * (c) 1995 Microsoft Corporation. All rights reserved.
 * 		Developed by hip communications inc., http://info.hip.com/info/
 * Portions (c) 1993 Intergraph Corporation. All rights reserved.
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 */
#define PERLIO_NOT_STDIO 0
#define WIN32_LEAN_AND_MEAN
#define WIN32IO_IS_STDIO
#include <tchar.h>
#ifdef __GNUC__
#define Win32_Winsock
#endif
#include <windows.h>
#ifndef HWND_MESSAGE
#  define HWND_MESSAGE     ((HWND)-3)
#endif
#ifndef WC_NO_BEST_FIT_CHARS
#  define WC_NO_BEST_FIT_CHARS 0x00000400 /* requires Windows 2000 or later */
#endif
#include <winnt.h>
#include <tlhelp32.h>
#include <io.h>
#include <signal.h>

#define SystemProcessesAndThreadsInformation 5

/* Inline some definitions from the DDK */
typedef struct {
    USHORT	    Length;
    USHORT	    MaximumLength;
    PWSTR	    Buffer;
}   UNICODE_STRING;

typedef struct {
    ULONG           NextEntryDelta;
    ULONG	    ThreadCount;
    ULONG	    Reserved1[6];
    LARGE_INTEGER   CreateTime;
    LARGE_INTEGER   UserTime;
    LARGE_INTEGER   KernelTime;
    UNICODE_STRING  ProcessName;
    LONG	    BasePriority;
    ULONG	    ProcessId;
    ULONG	    InheritedFromProcessId;
    /* Remainder of the structure depends on the Windows version,
     * but we don't need those additional fields anyways... */
}   SYSTEM_PROCESSES;

/* #include "config.h" */

#if !defined(PERLIO_IS_STDIO) && !defined(USE_SFIO)
#define PerlIO FILE
#endif

#include <sys/stat.h>
#include "EXTERN.h"
#include "perl.h"

#define NO_XSLOCKS
#define PERL_NO_GET_CONTEXT
#include "XSUB.h"

#include "Win32iop.h"
#include <fcntl.h>
#ifndef __GNUC__
/* assert.h conflicts with #define of assert in perl.h */
#include <assert.h>
#endif
#include <string.h>
#include <stdarg.h>
#include <float.h>
#include <time.h>
#if defined(_MSC_VER) || defined(__MINGW32__)
#include <sys/utime.h>
#else
#include <utime.h>
#endif
#ifdef __GNUC__
/* Mingw32 defaults to globing command line
 * So we turn it off like this:
 */
int _CRT_glob = 0;
#endif

#if defined(__MINGW32__) && (__MINGW32_MAJOR_VERSION==1)	
/* Mingw32-1.1 is missing some prototypes */
START_EXTERN_C
FILE * _wfopen(LPCWSTR wszFileName, LPCWSTR wszMode);
FILE * _wfdopen(int nFd, LPCWSTR wszMode);
FILE * _freopen(LPCWSTR wszFileName, LPCWSTR wszMode, FILE * pOldStream);
int _flushall();
int _fcloseall();
END_EXTERN_C
#endif

#if defined(__BORLANDC__)
#  define _stat stat
#  define _utimbuf utimbuf
#endif

#define EXECF_EXEC 1
#define EXECF_SPAWN 2
#define EXECF_SPAWN_NOWAIT 3

#if defined(PERL_IMPLICIT_SYS)
#  undef win32_get_privlib
#  define win32_get_privlib g_win32_get_privlib
#  undef win32_get_sitelib
#  define win32_get_sitelib g_win32_get_sitelib
#  undef win32_get_vendorlib
#  define win32_get_vendorlib g_win32_get_vendorlib
#  undef getlogin
#  define getlogin g_getlogin
#endif

static void		get_shell(void);
static long		tokenize(const char *str, char **dest, char ***destv);
static int		do_spawn2(pTHX_ const char *cmd, int exectype);
static BOOL		has_shell_metachars(const char *ptr);
static long		filetime_to_clock(PFILETIME ft);
static BOOL		filetime_from_time(PFILETIME ft, time_t t);
static char *		get_emd_part(SV **leading, char *trailing, ...);
static void		remove_dead_process(long deceased);
static long		find_pid(int pid);
static char *		qualified_path(const char *cmd);
static char *		win32_get_xlib(const char *pl, const char *xlib,
				       const char *libname);
static LRESULT  win32_process_message(HWND hwnd, UINT msg,
                       WPARAM wParam, LPARAM lParam);

#ifdef USE_ITHREADS
static void		remove_dead_pseudo_process(long child);
static long		find_pseudo_pid(int pid);
#endif

START_EXTERN_C
HANDLE	w32_perldll_handle = INVALID_HANDLE_VALUE;
char	w32_module_name[MAX_PATH+1];
END_EXTERN_C

static OSVERSIONINFO g_osver = {0, 0, 0, 0, 0, ""};

static HANDLE (WINAPI *pfnCreateToolhelp32Snapshot)(DWORD, DWORD) = NULL;
static BOOL   (WINAPI *pfnProcess32First)(HANDLE, PROCESSENTRY32*) = NULL;
static BOOL   (WINAPI *pfnProcess32Next)(HANDLE, PROCESSENTRY32*) = NULL;
static LONG   (WINAPI *pfnZwQuerySystemInformation)(UINT, PVOID, ULONG, PULONG);

#ifdef __BORLANDC__
/* Silence STDERR grumblings from Borland's math library. */
DllExport int
_matherr(struct _exception *a)
{
    PERL_UNUSED_VAR(a);
    return 1;
}
#endif

/* VS2005 (MSC version 14) provides a mechanism to set an invalid
 * parameter handler.  This functionality is not available in the
 * 64-bit compiler from the Platform SDK, which unfortunately also
 * believes itself to be MSC version 14.
 *
 * There is no #define related to _set_invalid_parameter_handler(),
 * but we can check for one of the constants defined for
 * _set_abort_behavior(), which was introduced into stdlib.h at
 * the same time.
 */

#if _MSC_VER >= 1400 && defined(_WRITE_ABORT_MSG)
#  define SET_INVALID_PARAMETER_HANDLER
#endif

#ifdef SET_INVALID_PARAMETER_HANDLER
void my_invalid_parameter_handler(const wchar_t* expression,
    const wchar_t* function, 
    const wchar_t* file, 
    unsigned int line, 
    uintptr_t pReserved)
{
#  ifdef _DEBUG
    wprintf(L"Invalid parameter detected in function %s."
            L" File: %s Line: %d\n", function, file, line);
    wprintf(L"Expression: %s\n", expression);
#  endif
}
#endif

int
IsWin95(void)
{
    return (g_osver.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS);
}

int
IsWinNT(void)
{
    return (g_osver.dwPlatformId == VER_PLATFORM_WIN32_NT);
}

int
IsWin2000(void)
{
    return (g_osver.dwMajorVersion > 4);
}

EXTERN_C void
set_w32_module_name(void)
{
    /* this function may be called at DLL_PROCESS_ATTACH time */
    char* ptr;
    HMODULE module = (HMODULE)((w32_perldll_handle == INVALID_HANDLE_VALUE)
                               ? GetModuleHandle(NULL)
                               : w32_perldll_handle);

    OSVERSIONINFO osver; /* g_osver may not yet be initialized */
    osver.dwOSVersionInfoSize = sizeof(osver);
    GetVersionEx(&osver);

    if (osver.dwMajorVersion > 4) {
        WCHAR modulename[MAX_PATH];
        WCHAR fullname[MAX_PATH];
        char *ansi;

        DWORD (__stdcall *pfnGetLongPathNameW)(LPCWSTR, LPWSTR, DWORD) =
            (DWORD (__stdcall *)(LPCWSTR, LPWSTR, DWORD))
            GetProcAddress(GetModuleHandle("kernel32.dll"), "GetLongPathNameW");

        GetModuleFileNameW(module, modulename, sizeof(modulename)/sizeof(WCHAR));

        /* Make sure we get an absolute pathname in case the module was loaded
         * explicitly by LoadLibrary() with a relative path. */
        GetFullPathNameW(modulename, sizeof(fullname)/sizeof(WCHAR), fullname, NULL);

        /* Make sure we start with the long path name of the module because we
         * later scan for pathname components to match "5.xx" to locate
         * compatible sitelib directories, and the short pathname might mangle
         * this path segment (e.g. by removing the dot on NTFS to something
         * like "5xx~1.yy") */
        if (pfnGetLongPathNameW)
            pfnGetLongPathNameW(fullname, fullname, sizeof(fullname)/sizeof(WCHAR));

        /* remove \\?\ prefix */
        if (memcmp(fullname, L"\\\\?\\", 4*sizeof(WCHAR)) == 0)
            memmove(fullname, fullname+4, (wcslen(fullname+4)+1)*sizeof(WCHAR));

        ansi = win32_ansipath(fullname);
        my_strlcpy(w32_module_name, ansi, sizeof(w32_module_name));
        win32_free(ansi);
    }
    else {
        GetModuleFileName(module, w32_module_name, sizeof(w32_module_name));

        /* remove \\?\ prefix */
        if (memcmp(w32_module_name, "\\\\?\\", 4) == 0)
            memmove(w32_module_name, w32_module_name+4, strlen(w32_module_name+4)+1);

        /* try to get full path to binary (which may be mangled when perl is
         * run from a 16-bit app) */
        /*PerlIO_printf(Perl_debug_log, "Before %s\n", w32_module_name);*/
        win32_longpath(w32_module_name);
        /*PerlIO_printf(Perl_debug_log, "After  %s\n", w32_module_name);*/
    }

    /* normalize to forward slashes */
    ptr = w32_module_name;
    while (*ptr) {
	if (*ptr == '\\')
	    *ptr = '/';
	++ptr;
    }
}

/* *svp (if non-NULL) is expected to be POK (valid allocated SvPVX(*svp)) */
static char*
get_regstr_from(HKEY hkey, const char *valuename, SV **svp)
{
    /* Retrieve a REG_SZ or REG_EXPAND_SZ from the registry */
    HKEY handle;
    DWORD type;
    const char *subkey = "Software\\Perl";
    char *str = NULL;
    long retval;

    retval = RegOpenKeyEx(hkey, subkey, 0, KEY_READ, &handle);
    if (retval == ERROR_SUCCESS) {
	DWORD datalen;
	retval = RegQueryValueEx(handle, valuename, 0, &type, NULL, &datalen);
	if (retval == ERROR_SUCCESS
	    && (type == REG_SZ || type == REG_EXPAND_SZ))
	{
	    dTHX;
	    if (!*svp)
		*svp = sv_2mortal(newSVpvn("",0));
	    SvGROW(*svp, datalen);
	    retval = RegQueryValueEx(handle, valuename, 0, NULL,
				     (PBYTE)SvPVX(*svp), &datalen);
	    if (retval == ERROR_SUCCESS) {
		str = SvPVX(*svp);
		SvCUR_set(*svp,datalen-1);
	    }
	}
	RegCloseKey(handle);
    }
    return str;
}

/* *svp (if non-NULL) is expected to be POK (valid allocated SvPVX(*svp)) */
static char*
get_regstr(const char *valuename, SV **svp)
{
    char *str = get_regstr_from(HKEY_CURRENT_USER, valuename, svp);
    if (!str)
	str = get_regstr_from(HKEY_LOCAL_MACHINE, valuename, svp);
    return str;
}

/* *prev_pathp (if non-NULL) is expected to be POK (valid allocated SvPVX(sv)) */
static char *
get_emd_part(SV **prev_pathp, char *trailing_path, ...)
{
    char base[10];
    va_list ap;
    char mod_name[MAX_PATH+1];
    char *ptr;
    char *optr;
    char *strip;
    STRLEN baselen;

    va_start(ap, trailing_path);
    strip = va_arg(ap, char *);

    sprintf(base, "%d.%d", (int)PERL_REVISION, (int)PERL_VERSION);
    baselen = strlen(base);

    if (!*w32_module_name) {
	set_w32_module_name();
    }
    strcpy(mod_name, w32_module_name);
    ptr = strrchr(mod_name, '/');
    while (ptr && strip) {
        /* look for directories to skip back */
	optr = ptr;
	*ptr = '\0';
	ptr = strrchr(mod_name, '/');
	/* avoid stripping component if there is no slash,
	 * or it doesn't match ... */
	if (!ptr || stricmp(ptr+1, strip) != 0) {
	    /* ... but not if component matches m|5\.$patchlevel.*| */
	    if (!ptr || !(*strip == '5' && *(ptr+1) == '5'
			  && strncmp(strip, base, baselen) == 0
			  && strncmp(ptr+1, base, baselen) == 0))
	    {
		*optr = '/';
		ptr = optr;
	    }
	}
	strip = va_arg(ap, char *);
    }
    if (!ptr) {
	ptr = mod_name;
	*ptr++ = '.';
	*ptr = '/';
    }
    va_end(ap);
    strcpy(++ptr, trailing_path);

    /* only add directory if it exists */
    if (GetFileAttributes(mod_name) != (DWORD) -1) {
	/* directory exists */
	dTHX;
	if (!*prev_pathp)
	    *prev_pathp = sv_2mortal(newSVpvn("",0));
	else if (SvPVX(*prev_pathp))
	    sv_catpvn(*prev_pathp, ";", 1);
	sv_catpv(*prev_pathp, mod_name);
	return SvPVX(*prev_pathp);
    }

    return NULL;
}

char *
win32_get_privlib(const char *pl)
{
    dTHX;
    char *stdlib = "lib";
    char buffer[MAX_PATH+1];
    SV *sv = NULL;

    /* $stdlib = $HKCU{"lib-$]"} || $HKLM{"lib-$]"} || $HKCU{"lib"} || $HKLM{"lib"} || "";  */
    sprintf(buffer, "%s-%s", stdlib, pl);
    if (!get_regstr(buffer, &sv))
	(void)get_regstr(stdlib, &sv);

    /* $stdlib .= ";$EMD/../../lib" */
    return get_emd_part(&sv, stdlib, ARCHNAME, "bin", NULL);
}

static char *
win32_get_xlib(const char *pl, const char *xlib, const char *libname)
{
    dTHX;
    char regstr[40];
    char pathstr[MAX_PATH+1];
    SV *sv1 = NULL;
    SV *sv2 = NULL;

    /* $HKCU{"$xlib-$]"} || $HKLM{"$xlib-$]"} . ---; */
    sprintf(regstr, "%s-%s", xlib, pl);
    (void)get_regstr(regstr, &sv1);

    /* $xlib .=
     * ";$EMD/" . ((-d $EMD/../../../$]) ? "../../.." : "../.."). "/$libname/$]/lib";  */
    sprintf(pathstr, "%s/%s/lib", libname, pl);
    (void)get_emd_part(&sv1, pathstr, ARCHNAME, "bin", pl, NULL);

    /* $HKCU{$xlib} || $HKLM{$xlib} . ---; */
    (void)get_regstr(xlib, &sv2);

    /* $xlib .=
     * ";$EMD/" . ((-d $EMD/../../../$]) ? "../../.." : "../.."). "/$libname/lib";  */
    sprintf(pathstr, "%s/lib", libname);
    (void)get_emd_part(&sv2, pathstr, ARCHNAME, "bin", pl, NULL);

    if (!sv1 && !sv2)
	return NULL;
    if (!sv1)
	return SvPVX(sv2);
    if (!sv2)
	return SvPVX(sv1);

    sv_catpvn(sv1, ";", 1);
    sv_catsv(sv1, sv2);

    return SvPVX(sv1);
}

char *
win32_get_sitelib(const char *pl)
{
    return win32_get_xlib(pl, "sitelib", "site");
}

#ifndef PERL_VENDORLIB_NAME
#  define PERL_VENDORLIB_NAME	"vendor"
#endif

char *
win32_get_vendorlib(const char *pl)
{
    return win32_get_xlib(pl, "vendorlib", PERL_VENDORLIB_NAME);
}

static BOOL
has_shell_metachars(const char *ptr)
{
    int inquote = 0;
    char quote = '\0';

    /*
     * Scan string looking for redirection (< or >) or pipe
     * characters (|) that are not in a quoted string.
     * Shell variable interpolation (%VAR%) can also happen inside strings.
     */
    while (*ptr) {
	switch(*ptr) {
	case '%':
	    return TRUE;
	case '\'':
	case '\"':
	    if (inquote) {
		if (quote == *ptr) {
		    inquote = 0;
		    quote = '\0';
		}
	    }
	    else {
		quote = *ptr;
		inquote++;
	    }
	    break;
	case '>':
	case '<':
	case '|':
	    if (!inquote)
		return TRUE;
	default:
	    break;
	}
	++ptr;
    }
    return FALSE;
}

#if !defined(PERL_IMPLICIT_SYS)
/* since the current process environment is being updated in util.c
 * the library functions will get the correct environment
 */
PerlIO *
Perl_my_popen(pTHX_ const char *cmd, const char *mode)
{
#ifdef FIXCMD
#define fixcmd(x)   {					\
			char *pspace = strchr((x),' ');	\
			if (pspace) {			\
			    char *p = (x);		\
			    while (p < pspace) {	\
				if (*p == '/')		\
				    *p = '\\';		\
				p++;			\
			    }				\
			}				\
		    }
#else
#define fixcmd(x)
#endif
    fixcmd(cmd);
    PERL_FLUSHALL_FOR_CHILD;
    return win32_popen(cmd, mode);
}

long
Perl_my_pclose(pTHX_ PerlIO *fp)
{
    return win32_pclose(fp);
}
#endif

DllExport unsigned long
win32_os_id(void)
{
    return (unsigned long)g_osver.dwPlatformId;
}

DllExport int
win32_getpid(void)
{
    int pid;
#ifdef USE_ITHREADS
    dTHX;
    if (w32_pseudo_id)
	return -((int)w32_pseudo_id);
#endif
    pid = _getpid();
    /* Windows 9x appears to always reports a pid for threads and processes
     * that has the high bit set. So we treat the lower 31 bits as the
     * "real" PID for Perl's purposes. */
    if (IsWin95() && pid < 0)
	pid = -pid;
    return pid;
}

/* Tokenize a string.  Words are null-separated, and the list
 * ends with a doubled null.  Any character (except null and
 * including backslash) may be escaped by preceding it with a
 * backslash (the backslash will be stripped).
 * Returns number of words in result buffer.
 */
static long
tokenize(const char *str, char **dest, char ***destv)
{
    char *retstart = NULL;
    char **retvstart = 0;
    int items = -1;
    if (str) {
	dTHX;
	int slen = strlen(str);
	register char *ret;
	register char **retv;
	Newx(ret, slen+2, char);
	Newx(retv, (slen+3)/2, char*);

	retstart = ret;
	retvstart = retv;
	*retv = ret;
	items = 0;
	while (*str) {
	    *ret = *str++;
	    if (*ret == '\\' && *str)
		*ret = *str++;
	    else if (*ret == ' ') {
		while (*str == ' ')
		    str++;
		if (ret == retstart)
		    ret--;
		else {
		    *ret = '\0';
		    ++items;
		    if (*str)
			*++retv = ret+1;
		}
	    }
	    else if (!*str)
		++items;
	    ret++;
	}
	retvstart[items] = NULL;
	*ret++ = '\0';
	*ret = '\0';
    }
    *dest = retstart;
    *destv = retvstart;
    return items;
}

static void
get_shell(void)
{
    dTHX;
    if (!w32_perlshell_tokens) {
	/* we don't use COMSPEC here for two reasons:
	 *  1. the same reason perl on UNIX doesn't use SHELL--rampant and
	 *     uncontrolled unportability of the ensuing scripts.
	 *  2. PERL5SHELL could be set to a shell that may not be fit for
	 *     interactive use (which is what most programs look in COMSPEC
	 *     for).
	 */
	const char* defaultshell = (IsWinNT()
				    ? "cmd.exe /x/d/c" : "command.com /c");
	const char *usershell = PerlEnv_getenv("PERL5SHELL");
	w32_perlshell_items = tokenize(usershell ? usershell : defaultshell,
				       &w32_perlshell_tokens,
				       &w32_perlshell_vec);
    }
}

int
Perl_do_aspawn(pTHX_ SV *really, SV **mark, SV **sp)
{
    char **argv;
    char *str;
    int status;
    int flag = P_WAIT;
    int index = 0;

    PERL_ARGS_ASSERT_DO_ASPAWN;

    if (sp <= mark)
	return -1;

    get_shell();
    Newx(argv, (sp - mark) + w32_perlshell_items + 2, char*);

    if (SvNIOKp(*(mark+1)) && !SvPOKp(*(mark+1))) {
	++mark;
	flag = SvIVx(*mark);
    }

    while (++mark <= sp) {
	if (*mark && (str = SvPV_nolen(*mark)))
	    argv[index++] = str;
	else
	    argv[index++] = "";
    }
    argv[index++] = 0;

    status = win32_spawnvp(flag,
			   (const char*)(really ? SvPV_nolen(really) : argv[0]),
			   (const char* const*)argv);

    if (status < 0 && (errno == ENOEXEC || errno == ENOENT)) {
	/* possible shell-builtin, invoke with shell */
	int sh_items;
	sh_items = w32_perlshell_items;
	while (--index >= 0)
	    argv[index+sh_items] = argv[index];
	while (--sh_items >= 0)
	    argv[sh_items] = w32_perlshell_vec[sh_items];

	status = win32_spawnvp(flag,
			       (const char*)(really ? SvPV_nolen(really) : argv[0]),
			       (const char* const*)argv);
    }

    if (flag == P_NOWAIT) {
	PL_statusvalue = -1;	/* >16bits hint for pp_system() */
    }
    else {
	if (status < 0) {
	    if (ckWARN(WARN_EXEC))
		Perl_warner(aTHX_ packWARN(WARN_EXEC), "Can't spawn \"%s\": %s", argv[0], strerror(errno));
	    status = 255 * 256;
	}
	else
	    status *= 256;
	PL_statusvalue = status;
    }
    Safefree(argv);
    return (status);
}

/* returns pointer to the next unquoted space or the end of the string */
static char*
find_next_space(const char *s)
{
    bool in_quotes = FALSE;
    while (*s) {
	/* ignore doubled backslashes, or backslash+quote */
	if (*s == '\\' && (s[1] == '\\' || s[1] == '"')) {
	    s += 2;
	}
	/* keep track of when we're within quotes */
	else if (*s == '"') {
	    s++;
	    in_quotes = !in_quotes;
	}
	/* break it up only at spaces that aren't in quotes */
	else if (!in_quotes && isSPACE(*s))
	    return (char*)s;
	else
	    s++;
    }
    return (char*)s;
}

static int
do_spawn2(pTHX_ const char *cmd, int exectype)
{
    char **a;
    char *s;
    char **argv;
    int status = -1;
    BOOL needToTry = TRUE;
    char *cmd2;

    /* Save an extra exec if possible. See if there are shell
     * metacharacters in it */
    if (!has_shell_metachars(cmd)) {
	Newx(argv, strlen(cmd) / 2 + 2, char*);
	Newx(cmd2, strlen(cmd) + 1, char);
	strcpy(cmd2, cmd);
	a = argv;
	for (s = cmd2; *s;) {
	    while (*s && isSPACE(*s))
		s++;
	    if (*s)
		*(a++) = s;
	    s = find_next_space(s);
	    if (*s)
		*s++ = '\0';
	}
	*a = NULL;
	if (argv[0]) {
	    switch (exectype) {
	    case EXECF_SPAWN:
		status = win32_spawnvp(P_WAIT, argv[0],
				       (const char* const*)argv);
		break;
	    case EXECF_SPAWN_NOWAIT:
		status = win32_spawnvp(P_NOWAIT, argv[0],
				       (const char* const*)argv);
		break;
	    case EXECF_EXEC:
		status = win32_execvp(argv[0], (const char* const*)argv);
		break;
	    }
	    if (status != -1 || errno == 0)
		needToTry = FALSE;
	}
	Safefree(argv);
	Safefree(cmd2);
    }
    if (needToTry) {
	char **argv;
	int i = -1;
	get_shell();
	Newx(argv, w32_perlshell_items + 2, char*);
	while (++i < w32_perlshell_items)
	    argv[i] = w32_perlshell_vec[i];
	argv[i++] = (char *)cmd;
	argv[i] = NULL;
	switch (exectype) {
	case EXECF_SPAWN:
	    status = win32_spawnvp(P_WAIT, argv[0],
				   (const char* const*)argv);
	    break;
	case EXECF_SPAWN_NOWAIT:
	    status = win32_spawnvp(P_NOWAIT, argv[0],
				   (const char* const*)argv);
	    break;
	case EXECF_EXEC:
	    status = win32_execvp(argv[0], (const char* const*)argv);
	    break;
	}
	cmd = argv[0];
	Safefree(argv);
    }
    if (exectype == EXECF_SPAWN_NOWAIT) {
	PL_statusvalue = -1;	/* >16bits hint for pp_system() */
    }
    else {
	if (status < 0) {
	    if (ckWARN(WARN_EXEC))
		Perl_warner(aTHX_ packWARN(WARN_EXEC), "Can't %s \"%s\": %s",
		     (exectype == EXECF_EXEC ? "exec" : "spawn"),
		     cmd, strerror(errno));
	    status = 255 * 256;
	}
	else
	    status *= 256;
	PL_statusvalue = status;
    }
    return (status);
}

int
Perl_do_spawn(pTHX_ char *cmd)
{
    PERL_ARGS_ASSERT_DO_SPAWN;

    return do_spawn2(aTHX_ cmd, EXECF_SPAWN);
}

int
Perl_do_spawn_nowait(pTHX_ char *cmd)
{
    PERL_ARGS_ASSERT_DO_SPAWN_NOWAIT;

    return do_spawn2(aTHX_ cmd, EXECF_SPAWN_NOWAIT);
}

bool
Perl_do_exec(pTHX_ const char *cmd)
{
    PERL_ARGS_ASSERT_DO_EXEC;

    do_spawn2(aTHX_ cmd, EXECF_EXEC);
    return FALSE;
}

/* The idea here is to read all the directory names into a string table
 * (separated by nulls) and when one of the other dir functions is called
 * return the pointer to the current file name.
 */
DllExport DIR *
win32_opendir(const char *filename)
{
    dTHX;
    DIR			*dirp;
    long		len;
    long		idx;
    char		scanname[MAX_PATH+3];
    Stat_t		sbuf;
    WIN32_FIND_DATAA	aFindData;
    WIN32_FIND_DATAW	wFindData;
    bool                using_wide;
    char		buffer[MAX_PATH*2];
    char		*ptr;

    len = strlen(filename);
    if (len > MAX_PATH)
	return NULL;

    /* check to see if filename is a directory */
    if (win32_stat(filename, &sbuf) < 0 || !S_ISDIR(sbuf.st_mode))
	return NULL;

    /* Get us a DIR structure */
    Newxz(dirp, 1, DIR);

    /* Create the search pattern */
    strcpy(scanname, filename);

    /* bare drive name means look in cwd for drive */
    if (len == 2 && isALPHA(scanname[0]) && scanname[1] == ':') {
	scanname[len++] = '.';
	scanname[len++] = '/';
    }
    else if (scanname[len-1] != '/' && scanname[len-1] != '\\') {
	scanname[len++] = '/';
    }
    scanname[len++] = '*';
    scanname[len] = '\0';

    /* do the FindFirstFile call */
    if (IsWin2000()) {
        WCHAR wscanname[sizeof(scanname)];
        MultiByteToWideChar(CP_ACP, 0, scanname, -1, wscanname, sizeof(wscanname)/sizeof(WCHAR));
	dirp->handle = FindFirstFileW(PerlDir_mapW(wscanname), &wFindData);
        using_wide = TRUE;
    }
    else {
 	dirp->handle = FindFirstFileA(PerlDir_mapA(scanname), &aFindData);
    }
    if (dirp->handle == INVALID_HANDLE_VALUE) {
	DWORD err = GetLastError();
	/* FindFirstFile() fails on empty drives! */
	switch (err) {
	case ERROR_FILE_NOT_FOUND:
	    return dirp;
	case ERROR_NO_MORE_FILES:
	case ERROR_PATH_NOT_FOUND:
	    errno = ENOENT;
	    break;
	case ERROR_NOT_ENOUGH_MEMORY:
	    errno = ENOMEM;
	    break;
	default:
	    errno = EINVAL;
	    break;
	}
	Safefree(dirp);
	return NULL;
    }

    if (using_wide) {
        BOOL use_default = FALSE;
        WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS,
                            wFindData.cFileName, -1,
                            buffer, sizeof(buffer), NULL, &use_default);
        if (use_default && *wFindData.cAlternateFileName) {
            WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS,
                                wFindData.cAlternateFileName, -1,
                                buffer, sizeof(buffer), NULL, NULL);
        }
        ptr = buffer;
    }
    else {
        ptr = aFindData.cFileName;
    }
    /* now allocate the first part of the string table for
     * the filenames that we find.
     */
    idx = strlen(ptr)+1;
    if (idx < 256)
	dirp->size = 256;
    else
	dirp->size = idx;
    Newx(dirp->start, dirp->size, char);
    strcpy(dirp->start, ptr);
    dirp->nfiles++;
    dirp->end = dirp->curr = dirp->start;
    dirp->end += idx;
    return dirp;
}


/* Readdir just returns the current string pointer and bumps the
 * string pointer to the nDllExport entry.
 */
DllExport struct direct *
win32_readdir(DIR *dirp)
{
    long         len;

    if (dirp->curr) {
	/* first set up the structure to return */
	len = strlen(dirp->curr);
	strcpy(dirp->dirstr.d_name, dirp->curr);
	dirp->dirstr.d_namlen = len;

	/* Fake an inode */
	dirp->dirstr.d_ino = dirp->curr - dirp->start;

	/* Now set up for the next call to readdir */
	dirp->curr += len + 1;
	if (dirp->curr >= dirp->end) {
	    dTHX;
	    BOOL res;
            WIN32_FIND_DATAA aFindData;
	    char buffer[MAX_PATH*2];
            char *ptr;

	    /* finding the next file that matches the wildcard
	     * (which should be all of them in this directory!).
	     */
	    if (IsWin2000()) {
                WIN32_FIND_DATAW wFindData;
		res = FindNextFileW(dirp->handle, &wFindData);
		if (res) {
                    BOOL use_default = FALSE;
                    WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS,
                                        wFindData.cFileName, -1,
                                        buffer, sizeof(buffer), NULL, &use_default);
                    if (use_default && *wFindData.cAlternateFileName) {
                        WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS,
                                            wFindData.cAlternateFileName, -1,
                                            buffer, sizeof(buffer), NULL, NULL);
                    }
                    ptr = buffer;
                }
            }
            else {
                res = FindNextFileA(dirp->handle, &aFindData);
                ptr = aFindData.cFileName;
            }
	    if (res) {
		long endpos = dirp->end - dirp->start;
		long newsize = endpos + strlen(ptr) + 1;
		/* bump the string table size by enough for the
		 * new name and its null terminator */
		while (newsize > dirp->size) {
		    long curpos = dirp->curr - dirp->start;
		    dirp->size *= 2;
		    Renew(dirp->start, dirp->size, char);
		    dirp->curr = dirp->start + curpos;
		}
		strcpy(dirp->start + endpos, ptr);
		dirp->end = dirp->start + newsize;
		dirp->nfiles++;
	    }
	    else
		dirp->curr = NULL;
	}
	return &(dirp->dirstr);
    }
    else
	return NULL;
}

/* Telldir returns the current string pointer position */
DllExport long
win32_telldir(DIR *dirp)
{
    return (dirp->curr - dirp->start);
}


/* Seekdir moves the string pointer to a previously saved position
 * (returned by telldir).
 */
DllExport void
win32_seekdir(DIR *dirp, long loc)
{
    dirp->curr = dirp->start + loc;
}

/* Rewinddir resets the string pointer to the start */
DllExport void
win32_rewinddir(DIR *dirp)
{
    dirp->curr = dirp->start;
}

/* free the memory allocated by opendir */
DllExport int
win32_closedir(DIR *dirp)
{
    dTHX;
    if (dirp->handle != INVALID_HANDLE_VALUE)
	FindClose(dirp->handle);
    Safefree(dirp->start);
    Safefree(dirp);
    return 1;
}


/*
 * various stubs
 */


/* Ownership
 *
 * Just pretend that everyone is a superuser. NT will let us know if
 * we don\'t really have permission to do something.
 */

#define ROOT_UID    ((uid_t)0)
#define ROOT_GID    ((gid_t)0)

uid_t
getuid(void)
{
    return ROOT_UID;
}

uid_t
geteuid(void)
{
    return ROOT_UID;
}

gid_t
getgid(void)
{
    return ROOT_GID;
}

gid_t
getegid(void)
{
    return ROOT_GID;
}

int
setuid(uid_t auid)
{
    return (auid == ROOT_UID ? 0 : -1);
}

int
setgid(gid_t agid)
{
    return (agid == ROOT_GID ? 0 : -1);
}

char *
getlogin(void)
{
    dTHX;
    char *buf = w32_getlogin_buffer;
    DWORD size = sizeof(w32_getlogin_buffer);
    if (GetUserName(buf,&size))
	return buf;
    return (char*)NULL;
}

int
chown(const char *path, uid_t owner, gid_t group)
{
    /* XXX noop */
    return 0;
}

/*
 * XXX this needs strengthening  (for PerlIO)
 *   -- BKS, 11-11-200
*/
int mkstemp(const char *path)
{
    dTHX;
    char buf[MAX_PATH+1];
    int i = 0, fd = -1;

retry:
    if (i++ > 10) { /* give up */
	errno = ENOENT;
	return -1;
    }
    if (!GetTempFileNameA((LPCSTR)path, "plr", 1, buf)) {
	errno = ENOENT;
	return -1;
    }
    fd = PerlLIO_open3(buf, O_CREAT|O_RDWR|O_EXCL, 0600);
    if (fd == -1)
	goto retry;
    return fd;
}

static long
find_pid(int pid)
{
    dTHX;
    long child = w32_num_children;
    while (--child >= 0) {
	if ((int)w32_child_pids[child] == pid)
	    return child;
    }
    return -1;
}

static void
remove_dead_process(long child)
{
    if (child >= 0) {
	dTHX;
	CloseHandle(w32_child_handles[child]);
	Move(&w32_child_handles[child+1], &w32_child_handles[child],
	     (w32_num_children-child-1), HANDLE);
	Move(&w32_child_pids[child+1], &w32_child_pids[child],
	     (w32_num_children-child-1), DWORD);
	w32_num_children--;
    }
}

#ifdef USE_ITHREADS
static long
find_pseudo_pid(int pid)
{
    dTHX;
    long child = w32_num_pseudo_children;
    while (--child >= 0) {
	if ((int)w32_pseudo_child_pids[child] == pid)
	    return child;
    }
    return -1;
}

static void
remove_dead_pseudo_process(long child)
{
    if (child >= 0) {
	dTHX;
	CloseHandle(w32_pseudo_child_handles[child]);
	Move(&w32_pseudo_child_handles[child+1], &w32_pseudo_child_handles[child],
	     (w32_num_pseudo_children-child-1), HANDLE);
	Move(&w32_pseudo_child_pids[child+1], &w32_pseudo_child_pids[child],
	     (w32_num_pseudo_children-child-1), DWORD);
	Move(&w32_pseudo_child_message_hwnds[child+1], &w32_pseudo_child_message_hwnds[child],
	     (w32_num_pseudo_children-child-1), HWND);
	w32_num_pseudo_children--;
    }
}
#endif

static int
terminate_process(DWORD pid, HANDLE process_handle, int sig)
{
    switch(sig) {
    case 0:
        /* "Does process exist?" use of kill */
        return 1;
    case 2:
        if (GenerateConsoleCtrlEvent(CTRL_C_EVENT, pid))
            return 1;
        break;
    case SIGBREAK:
    case SIGTERM:
        if (GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid))
            return 1;
        break;
    default: /* For now be backwards compatible with perl 5.6 */
    case 9:
        /* Note that we will only be able to kill processes owned by the
         * current process owner, even when we are running as an administrator.
         * To kill processes of other owners we would need to set the
         * 'SeDebugPrivilege' privilege before obtaining the process handle.
         */
        if (TerminateProcess(process_handle, sig))
            return 1;
        break;
    }
    return 0;
}

/* Traverse process tree using ToolHelp functions */
static int
kill_process_tree_toolhelp(DWORD pid, int sig)
{
    HANDLE process_handle;
    HANDLE snapshot_handle;
    int killed = 0;

    process_handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (process_handle == NULL)
        return 0;

    killed += terminate_process(pid, process_handle, sig);

    snapshot_handle = pfnCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot_handle != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 entry;

        entry.dwSize = sizeof(entry);
        if (pfnProcess32First(snapshot_handle, &entry)) {
            do {
                if (entry.th32ParentProcessID == pid)
                    killed += kill_process_tree_toolhelp(entry.th32ProcessID, sig);
                entry.dwSize = sizeof(entry);
            }
            while (pfnProcess32Next(snapshot_handle, &entry));
        }
        CloseHandle(snapshot_handle);
    }
    CloseHandle(process_handle);
    return killed;
}

/* Traverse process tree using undocumented system information structures.
 * This is only necessary on Windows NT, which lacks the ToolHelp functions.
 */
static int
kill_process_tree_sysinfo(SYSTEM_PROCESSES *process_info, DWORD pid, int sig)
{
    HANDLE process_handle;
    SYSTEM_PROCESSES *p = process_info;
    int killed = 0;

    process_handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (process_handle == NULL)
        return 0;

    killed += terminate_process(pid, process_handle, sig);

    while (1) {
        if (p->InheritedFromProcessId == (DWORD)pid)
            killed += kill_process_tree_sysinfo(process_info, p->ProcessId, sig);

        if (p->NextEntryDelta == 0)
            break;

        p = (SYSTEM_PROCESSES*)((char*)p + p->NextEntryDelta);
    }

    CloseHandle(process_handle);
    return killed;
}

int
killpg(int pid, int sig)
{
    /* Use "documented" method whenever available */
    if (pfnCreateToolhelp32Snapshot && pfnProcess32First && pfnProcess32Next) {
        return kill_process_tree_toolhelp((DWORD)pid, sig);
    }

    /* Fall back to undocumented Windows internals on Windows NT */
    if (pfnZwQuerySystemInformation) {
        dTHX;
        char *buffer;
        DWORD size = 0;

        pfnZwQuerySystemInformation(SystemProcessesAndThreadsInformation, NULL, 0, &size);
        Newx(buffer, size, char);

        if (pfnZwQuerySystemInformation(SystemProcessesAndThreadsInformation, buffer, size, NULL) >= 0) {
            int killed = kill_process_tree_sysinfo((SYSTEM_PROCESSES*)buffer, (DWORD)pid, sig);
            Safefree(buffer);
            return killed;
        }
    }
    return 0;
}

static int
my_kill(int pid, int sig)
{
    int retval = 0;
    HANDLE process_handle;

    if (sig < 0)
        return killpg(pid, -sig);

    process_handle = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    /* OpenProcess() returns NULL on error, *not* INVALID_HANDLE_VALUE */
    if (process_handle != NULL) {
        retval = terminate_process(pid, process_handle, sig);
        CloseHandle(process_handle);
    }
    return retval;
}

DllExport int
win32_kill(int pid, int sig)
{
    dTHX;
    long child;
#ifdef USE_ITHREADS
    if (pid < 0) {
	/* it is a pseudo-forked child */
	child = find_pseudo_pid(-pid);
	if (child >= 0) {
            HWND hwnd = w32_pseudo_child_message_hwnds[child];
	    HANDLE hProcess = w32_pseudo_child_handles[child];
	    switch (sig) {
	    case 0:
		/* "Does process exist?" use of kill */
		return 0;

	    case 9:
                /* kill -9 style un-graceful exit */
	    	if (TerminateThread(hProcess, sig)) {
		    remove_dead_pseudo_process(child);
		    return 0;
	    	}
		break;

	    default: {
                int count = 0;
                /* pseudo-process has not yet properly initialized if hwnd isn't set */
                while (hwnd == INVALID_HANDLE_VALUE && count < 5) {
                    /* Yield and wait for the other thread to send us its message_hwnd */
                    Sleep(0);
                    win32_async_check(aTHX);
		    hwnd = w32_pseudo_child_message_hwnds[child];
                    ++count;
                }
                if (hwnd != INVALID_HANDLE_VALUE) {
                    /* We fake signals to pseudo-processes using Win32
                     * message queue.  In Win9X the pids are negative already. */
                    if ((hwnd != NULL && PostMessage(hwnd, WM_USER_KILL, sig, 0)) ||
                        PostThreadMessage(IsWin95() ? pid : -pid, WM_USER_KILL, sig, 0))
                    {
                        /* It might be us ... */
                        PERL_ASYNC_CHECK();
                        return 0;
                    }
                }
		break;
            }
            } /* switch */
	}
	else if (IsWin95()) {
	    pid = -pid;
	    goto alien_process;
	}
    }
    else
#endif
    {
	child = find_pid(pid);
	if (child >= 0) {
            if (my_kill(pid, sig)) {
                DWORD exitcode = 0;
                if (GetExitCodeProcess(w32_child_handles[child], &exitcode) &&
                    exitcode != STILL_ACTIVE)
                {
                    remove_dead_process(child);
                }
                return 0;
            }
	}
	else {
alien_process:
            if (my_kill((IsWin95() ? -pid : pid), sig))
                return 0;
	}
    }
    errno = EINVAL;
    return -1;
}

DllExport int
win32_stat(const char *path, Stat_t *sbuf)
{
    dTHX;
    char	buffer[MAX_PATH+1];
    int		l = strlen(path);
    int		res;
    int         nlink = 1;
    BOOL        expect_dir = FALSE;

    GV          *gv_sloppy = gv_fetchpvs("\027IN32_SLOPPY_STAT",
                                         GV_NOTQUAL, SVt_PV);
    BOOL        sloppy = gv_sloppy && SvTRUE(GvSV(gv_sloppy));

    if (l > 1) {
	switch(path[l - 1]) {
	/* FindFirstFile() and stat() are buggy with a trailing
	 * slashes, except for the root directory of a drive */
	case '\\':
        case '/':
	    if (l > sizeof(buffer)) {
		errno = ENAMETOOLONG;
		return -1;
	    }
            --l;
            strncpy(buffer, path, l);
            /* remove additional trailing slashes */
            while (l > 1 && (buffer[l-1] == '/' || buffer[l-1] == '\\'))
                --l;
            /* add back slash if we otherwise end up with just a drive letter */
            if (l == 2 && isALPHA(buffer[0]) && buffer[1] == ':')
                buffer[l++] = '\\';
            buffer[l] = '\0';
            path = buffer;
            expect_dir = TRUE;
	    break;

	/* FindFirstFile() is buggy with "x:", so add a dot :-( */
	case ':':
	    if (l == 2 && isALPHA(path[0])) {
		buffer[0] = path[0];
		buffer[1] = ':';
		buffer[2] = '.';
		buffer[3] = '\0';
		l = 3;
		path = buffer;
	    }
	    break;
	}
    }

    path = PerlDir_mapA(path);
    l = strlen(path);

    if (!sloppy) {
        /* We must open & close the file once; otherwise file attribute changes  */
        /* might not yet have propagated to "other" hard links of the same file. */
        /* This also gives us an opportunity to determine the number of links.   */
        HANDLE handle = CreateFileA(path, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (handle != INVALID_HANDLE_VALUE) {
            BY_HANDLE_FILE_INFORMATION bhi;
            if (GetFileInformationByHandle(handle, &bhi))
                nlink = bhi.nNumberOfLinks;
            CloseHandle(handle);
        }
    }

    /* path will be mapped correctly above */
#if defined(WIN64) || defined(USE_LARGE_FILES)
    res = _stati64(path, sbuf);
#else
    res = stat(path, sbuf);
#endif
    sbuf->st_nlink = nlink;

    if (res < 0) {
	/* CRT is buggy on sharenames, so make sure it really isn't.
	 * XXX using GetFileAttributesEx() will enable us to set
	 * sbuf->st_*time (but note that's not available on the
	 * Windows of 1995) */
	DWORD r = GetFileAttributesA(path);
	if (r != 0xffffffff && (r & FILE_ATTRIBUTE_DIRECTORY)) {
	    /* sbuf may still contain old garbage since stat() failed */
	    Zero(sbuf, 1, Stat_t);
	    sbuf->st_mode = S_IFDIR | S_IREAD;
	    errno = 0;
	    if (!(r & FILE_ATTRIBUTE_READONLY))
		sbuf->st_mode |= S_IWRITE | S_IEXEC;
	    return 0;
	}
    }
    else {
	if (l == 3 && isALPHA(path[0]) && path[1] == ':'
	    && (path[2] == '\\' || path[2] == '/'))
	{
	    /* The drive can be inaccessible, some _stat()s are buggy */
	    if (!GetVolumeInformationA(path,NULL,0,NULL,NULL,NULL,NULL,0)) {
		errno = ENOENT;
		return -1;
	    }
	}
        if (expect_dir && !S_ISDIR(sbuf->st_mode)) {
            errno = ENOTDIR;
            return -1;
        }
	if (S_ISDIR(sbuf->st_mode)) {
	    /* Ensure the "write" bit is switched off in the mode for
	     * directories with the read-only attribute set. Borland (at least)
	     * switches it on for directories, which is technically correct
	     * (directories are indeed always writable unless denied by DACLs),
	     * but we want stat() and -w to reflect the state of the read-only
	     * attribute for symmetry with chmod(). */
	    DWORD r = GetFileAttributesA(path);
	    if (r != 0xffffffff && (r & FILE_ATTRIBUTE_READONLY)) {
		sbuf->st_mode &= ~S_IWRITE;
	    }
	}
#ifdef __BORLANDC__
	if (S_ISDIR(sbuf->st_mode)) {
	    sbuf->st_mode |= S_IEXEC;
	}
	else if (S_ISREG(sbuf->st_mode)) {
	    int perms;
	    if (l >= 4 && path[l-4] == '.') {
		const char *e = path + l - 3;
		if (strnicmp(e,"exe",3)
		    && strnicmp(e,"bat",3)
		    && strnicmp(e,"com",3)
		    && (IsWin95() || strnicmp(e,"cmd",3)))
		    sbuf->st_mode &= ~S_IEXEC;
		else
		    sbuf->st_mode |= S_IEXEC;
	    }
	    else
		sbuf->st_mode &= ~S_IEXEC;
	    /* Propagate permissions to _group_ and _others_ */
	    perms = sbuf->st_mode & (S_IREAD|S_IWRITE|S_IEXEC);
	    sbuf->st_mode |= (perms>>3) | (perms>>6);
	}
#endif
    }
    return res;
}

#define isSLASH(c) ((c) == '/' || (c) == '\\')
#define SKIP_SLASHES(s) \
    STMT_START {				\
	while (*(s) && isSLASH(*(s)))		\
	    ++(s);				\
    } STMT_END
#define COPY_NONSLASHES(d,s) \
    STMT_START {				\
	while (*(s) && !isSLASH(*(s)))		\
	    *(d)++ = *(s)++;			\
    } STMT_END

/* Find the longname of a given path.  path is destructively modified.
 * It should have space for at least MAX_PATH characters. */
DllExport char *
win32_longpath(char *path)
{
    WIN32_FIND_DATA fdata;
    HANDLE fhand;
    char tmpbuf[MAX_PATH+1];
    char *tmpstart = tmpbuf;
    char *start = path;
    char sep;
    if (!path)
	return NULL;

    /* drive prefix */
    if (isALPHA(path[0]) && path[1] == ':') {
	start = path + 2;
	*tmpstart++ = path[0];
	*tmpstart++ = ':';
    }
    /* UNC prefix */
    else if (isSLASH(path[0]) && isSLASH(path[1])) {
	start = path + 2;
	*tmpstart++ = path[0];
	*tmpstart++ = path[1];
	SKIP_SLASHES(start);
	COPY_NONSLASHES(tmpstart,start);	/* copy machine name */
	if (*start) {
	    *tmpstart++ = *start++;
	    SKIP_SLASHES(start);
	    COPY_NONSLASHES(tmpstart,start);	/* copy share name */
	}
    }
    *tmpstart = '\0';
    while (*start) {
	/* copy initial slash, if any */
	if (isSLASH(*start)) {
	    *tmpstart++ = *start++;
	    *tmpstart = '\0';
	    SKIP_SLASHES(start);
	}

	/* FindFirstFile() expands "." and "..", so we need to pass
	 * those through unmolested */
	if (*start == '.'
	    && (!start[1] || isSLASH(start[1])
		|| (start[1] == '.' && (!start[2] || isSLASH(start[2])))))
	{
	    COPY_NONSLASHES(tmpstart,start);	/* copy "." or ".." */
	    *tmpstart = '\0';
	    continue;
	}

	/* if this is the end, bust outta here */
	if (!*start)
	    break;

	/* now we're at a non-slash; walk up to next slash */
	while (*start && !isSLASH(*start))
	    ++start;

	/* stop and find full name of component */
	sep = *start;
	*start = '\0';
	fhand = FindFirstFile(path,&fdata);
	*start = sep;
	if (fhand != INVALID_HANDLE_VALUE) {
	    STRLEN len = strlen(fdata.cFileName);
	    if ((STRLEN)(tmpbuf + sizeof(tmpbuf) - tmpstart) > len) {
		strcpy(tmpstart, fdata.cFileName);
		tmpstart += len;
		FindClose(fhand);
	    }
	    else {
		FindClose(fhand);
		errno = ERANGE;
		return NULL;
	    }
	}
	else {
	    /* failed a step, just return without side effects */
	    /*PerlIO_printf(Perl_debug_log, "Failed to find %s\n", path);*/
	    errno = EINVAL;
	    return NULL;
	}
    }
    strcpy(path,tmpbuf);
    return path;
}

static void
out_of_memory(void)
{
    if (PL_curinterp) {
        dTHX;
        /* Can't use PerlIO to write as it allocates memory */
        PerlLIO_write(PerlIO_fileno(Perl_error_log),
                      PL_no_mem, strlen(PL_no_mem));
        my_exit(1);
    }
    exit(1);
}

/* The win32_ansipath() function takes a Unicode filename and converts it
 * into the current Windows codepage. If some characters cannot be mapped,
 * then it will convert the short name instead.
 *
 * The buffer to the ansi pathname must be freed with win32_free() when it
 * it no longer needed.
 *
 * The argument to win32_ansipath() must exist before this function is
 * called; otherwise there is no way to determine the short path name.
 *
 * Ideas for future refinement:
 * - Only convert those segments of the path that are not in the current
 *   codepage, but leave the other segments in their long form.
 * - If the resulting name is longer than MAX_PATH, start converting
 *   additional path segments into short names until the full name
 *   is shorter than MAX_PATH.  Shorten the filename part last!
 */
DllExport char *
win32_ansipath(const WCHAR *widename)
{
    char *name;
    BOOL use_default = FALSE;
    size_t widelen = wcslen(widename)+1;
    int len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, widename, widelen,
                                  NULL, 0, NULL, NULL);
    name = win32_malloc(len);
    if (!name)
        out_of_memory();

    WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, widename, widelen,
                        name, len, NULL, &use_default);
    if (use_default) {
        DWORD shortlen = GetShortPathNameW(widename, NULL, 0);
        if (shortlen) {
            WCHAR *shortname = win32_malloc(shortlen*sizeof(WCHAR));
            if (!shortname)
                out_of_memory();
            shortlen = GetShortPathNameW(widename, shortname, shortlen)+1;

            len = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, shortname, shortlen,
                                      NULL, 0, NULL, NULL);
            name = win32_realloc(name, len);
            if (!name)
                out_of_memory();
            WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, shortname, shortlen,
                                name, len, NULL, NULL);
            win32_free(shortname);
        }
    }
    return name;
}

DllExport char *
win32_getenv(const char *name)
{
    dTHX;
    DWORD needlen;
    SV *curitem = NULL;

    needlen = GetEnvironmentVariableA(name,NULL,0);
    if (needlen != 0) {
	curitem = sv_2mortal(newSVpvn("", 0));
        do {
            SvGROW(curitem, needlen+1);
            needlen = GetEnvironmentVariableA(name,SvPVX(curitem),
                                              needlen);
        } while (needlen >= SvLEN(curitem));
        SvCUR_set(curitem, needlen);
    }
    else {
	/* allow any environment variables that begin with 'PERL'
	   to be stored in the registry */
	if (strncmp(name, "PERL", 4) == 0)
	    (void)get_regstr(name, &curitem);
    }
    if (curitem && SvCUR(curitem))
	return SvPVX(curitem);

    return NULL;
}

DllExport int
win32_putenv(const char *name)
{
    dTHX;
    char* curitem;
    char* val;
    int relval = -1;

    if (name) {
        Newx(curitem,strlen(name)+1,char);
        strcpy(curitem, name);
        val = strchr(curitem, '=');
        if (val) {
            /* The sane way to deal with the environment.
             * Has these advantages over putenv() & co.:
             *  * enables us to store a truly empty value in the
             *    environment (like in UNIX).
             *  * we don't have to deal with RTL globals, bugs and leaks
             *    (specifically, see http://support.microsoft.com/kb/235601).
             *  * Much faster.
             * Why you may want to use the RTL environment handling
             * (previously enabled by USE_WIN32_RTL_ENV):
             *  * environ[] and RTL functions will not reflect changes,
             *    which might be an issue if extensions want to access
             *    the env. via RTL.  This cuts both ways, since RTL will
             *    not see changes made by extensions that call the Win32
             *    functions directly, either.
             * GSAR 97-06-07
             */
            *val++ = '\0';
            if (SetEnvironmentVariableA(curitem, *val ? val : NULL))
                relval = 0;
        }
        Safefree(curitem);
    }
    return relval;
}

static long
filetime_to_clock(PFILETIME ft)
{
    __int64 qw = ft->dwHighDateTime;
    qw <<= 32;
    qw |= ft->dwLowDateTime;
    qw /= 10000;  /* File time ticks at 0.1uS, clock at 1mS */
    return (long) qw;
}

DllExport int
win32_times(struct tms *timebuf)
{
    FILETIME user;
    FILETIME kernel;
    FILETIME dummy;
    clock_t process_time_so_far = clock();
    if (GetProcessTimes(GetCurrentProcess(), &dummy, &dummy,
                        &kernel,&user)) {
	timebuf->tms_utime = filetime_to_clock(&user);
	timebuf->tms_stime = filetime_to_clock(&kernel);
	timebuf->tms_cutime = 0;
	timebuf->tms_cstime = 0;
    } else {
        /* That failed - e.g. Win95 fallback to clock() */
	timebuf->tms_utime = process_time_so_far;
	timebuf->tms_stime = 0;
	timebuf->tms_cutime = 0;
	timebuf->tms_cstime = 0;
    }
    return process_time_so_far;
}

/* fix utime() so it works on directories in NT */
static BOOL
filetime_from_time(PFILETIME pFileTime, time_t Time)
{
    struct tm *pTM = localtime(&Time);
    SYSTEMTIME SystemTime;
    FILETIME LocalTime;

    if (pTM == NULL)
	return FALSE;

    SystemTime.wYear   = pTM->tm_year + 1900;
    SystemTime.wMonth  = pTM->tm_mon + 1;
    SystemTime.wDay    = pTM->tm_mday;
    SystemTime.wHour   = pTM->tm_hour;
    SystemTime.wMinute = pTM->tm_min;
    SystemTime.wSecond = pTM->tm_sec;
    SystemTime.wMilliseconds = 0;

    return SystemTimeToFileTime(&SystemTime, &LocalTime) &&
           LocalFileTimeToFileTime(&LocalTime, pFileTime);
}

DllExport int
win32_unlink(const char *filename)
{
    dTHX;
    int ret;
    DWORD attrs;

    filename = PerlDir_mapA(filename);
    attrs = GetFileAttributesA(filename);
    if (attrs == 0xFFFFFFFF) {
        errno = ENOENT;
        return -1;
    }
    if (attrs & FILE_ATTRIBUTE_READONLY) {
        (void)SetFileAttributesA(filename, attrs & ~FILE_ATTRIBUTE_READONLY);
        ret = unlink(filename);
        if (ret == -1)
            (void)SetFileAttributesA(filename, attrs);
    }
    else
        ret = unlink(filename);
    return ret;
}

DllExport int
win32_utime(const char *filename, struct utimbuf *times)
{
    dTHX;
    HANDLE handle;
    FILETIME ftCreate;
    FILETIME ftAccess;
    FILETIME ftWrite;
    struct utimbuf TimeBuffer;
    int rc;

    filename = PerlDir_mapA(filename);
    rc = utime(filename, times);

    /* EACCES: path specifies directory or readonly file */
    if (rc == 0 || errno != EACCES /* || !IsWinNT() */)
	return rc;

    if (times == NULL) {
	times = &TimeBuffer;
	time(&times->actime);
	times->modtime = times->actime;
    }

    /* This will (and should) still fail on readonly files */
    handle = CreateFileA(filename, GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_DELETE, NULL,
                         OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (handle == INVALID_HANDLE_VALUE)
	return rc;

    if (GetFileTime(handle, &ftCreate, &ftAccess, &ftWrite) &&
	filetime_from_time(&ftAccess, times->actime) &&
	filetime_from_time(&ftWrite, times->modtime) &&
	SetFileTime(handle, &ftCreate, &ftAccess, &ftWrite))
    {
	rc = 0;
    }

    CloseHandle(handle);
    return rc;
}

typedef union {
    unsigned __int64	ft_i64;
    FILETIME		ft_val;
} FT_t;

#ifdef __GNUC__
#define Const64(x) x##LL
#else
#define Const64(x) x##i64
#endif
/* Number of 100 nanosecond units from 1/1/1601 to 1/1/1970 */
#define EPOCH_BIAS  Const64(116444736000000000)

/* NOTE: This does not compute the timezone info (doing so can be expensive,
 * and appears to be unsupported even by glibc) */
DllExport int
win32_gettimeofday(struct timeval *tp, void *not_used)
{
    FT_t ft;

    /* this returns time in 100-nanosecond units  (i.e. tens of usecs) */
    GetSystemTimeAsFileTime(&ft.ft_val);

    /* seconds since epoch */
    tp->tv_sec = (long)((ft.ft_i64 - EPOCH_BIAS) / Const64(10000000));

    /* microseconds remaining */
    tp->tv_usec = (long)((ft.ft_i64 / Const64(10)) % Const64(1000000));

    return 0;
}

DllExport int
win32_uname(struct utsname *name)
{
    struct hostent *hep;
    STRLEN nodemax = sizeof(name->nodename)-1;

    /* sysname */
    switch (g_osver.dwPlatformId) {
    case VER_PLATFORM_WIN32_WINDOWS:
        strcpy(name->sysname, "Windows");
        break;
    case VER_PLATFORM_WIN32_NT:
        strcpy(name->sysname, "Windows NT");
        break;
    case VER_PLATFORM_WIN32s:
        strcpy(name->sysname, "Win32s");
        break;
    default:
        strcpy(name->sysname, "Win32 Unknown");
        break;
    }

    /* release */
    sprintf(name->release, "%d.%d",
            g_osver.dwMajorVersion, g_osver.dwMinorVersion);

    /* version */
    sprintf(name->version, "Build %d",
            g_osver.dwPlatformId == VER_PLATFORM_WIN32_NT
            ? g_osver.dwBuildNumber : (g_osver.dwBuildNumber & 0xffff));
    if (g_osver.szCSDVersion[0]) {
        char *buf = name->version + strlen(name->version);
        sprintf(buf, " (%s)", g_osver.szCSDVersion);
    }

    /* nodename */
    hep = win32_gethostbyname("localhost");
    if (hep) {
	STRLEN len = strlen(hep->h_name);
	if (len <= nodemax) {
	    strcpy(name->nodename, hep->h_name);
	}
	else {
	    strncpy(name->nodename, hep->h_name, nodemax);
	    name->nodename[nodemax] = '\0';
	}
    }
    else {
	DWORD sz = nodemax;
	if (!GetComputerName(name->nodename, &sz))
	    *name->nodename = '\0';
    }

    /* machine (architecture) */
    {
	SYSTEM_INFO info;
	DWORD procarch;
	char *arch;
	GetSystemInfo(&info);

#if (defined(__BORLANDC__)&&(__BORLANDC__<=0x520)) \
 || (defined(__MINGW32__) && !defined(_ANONYMOUS_UNION))
	procarch = info.u.s.wProcessorArchitecture;
#else
	procarch = info.wProcessorArchitecture;
#endif
	switch (procarch) {
	case PROCESSOR_ARCHITECTURE_INTEL:
	    arch = "x86"; break;
	case PROCESSOR_ARCHITECTURE_MIPS:
	    arch = "mips"; break;
	case PROCESSOR_ARCHITECTURE_ALPHA:
	    arch = "alpha"; break;
	case PROCESSOR_ARCHITECTURE_PPC:
	    arch = "ppc"; break;
#ifdef PROCESSOR_ARCHITECTURE_SHX
	case PROCESSOR_ARCHITECTURE_SHX:
	    arch = "shx"; break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_ARM
	case PROCESSOR_ARCHITECTURE_ARM:
	    arch = "arm"; break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_IA64
	case PROCESSOR_ARCHITECTURE_IA64:
	    arch = "ia64"; break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_ALPHA64
	case PROCESSOR_ARCHITECTURE_ALPHA64:
	    arch = "alpha64"; break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_MSIL
	case PROCESSOR_ARCHITECTURE_MSIL:
	    arch = "msil"; break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_AMD64
	case PROCESSOR_ARCHITECTURE_AMD64:
	    arch = "amd64"; break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_IA32_ON_WIN64
	case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:
	    arch = "ia32-64"; break;
#endif
#ifdef PROCESSOR_ARCHITECTURE_UNKNOWN
	case PROCESSOR_ARCHITECTURE_UNKNOWN:
	    arch = "unknown"; break;
#endif
	default:
	    sprintf(name->machine, "unknown(0x%x)", procarch);
	    arch = name->machine;
	    break;
	}
	if (name->machine != arch)
	    strcpy(name->machine, arch);
    }
    return 0;
}

/* Timing related stuff */

int
do_raise(pTHX_ int sig) 
{
    if (sig < SIG_SIZE) {
	Sighandler_t handler = w32_sighandler[sig];
	if (handler == SIG_IGN) {
	    return 0;
	}
	else if (handler != SIG_DFL) {
	    (*handler)(sig);
	    return 0;
	}
	else {
	    /* Choose correct default behaviour */
	    switch (sig) {
#ifdef SIGCLD
		case SIGCLD:
#endif
#ifdef SIGCHLD
		case SIGCHLD:
#endif
		case 0:
		    return 0;
		case SIGTERM:
		default:
		    break;
	    }
	}
    }
    /* Tell caller to exit thread/process as approriate */
    return 1;
}

void
sig_terminate(pTHX_ int sig)
{
    Perl_warn(aTHX_ "Terminating on signal SIG%s(%d)\n",PL_sig_name[sig], sig);
    /* exit() seems to be safe, my_exit() or die() is a problem in ^C 
       thread 
     */
    exit(sig);
}

DllExport int
win32_async_check(pTHX)
{
    MSG msg;
    HWND hwnd = w32_message_hwnd;

    /* Reset w32_poll_count before doing anything else, incase we dispatch
     * messages that end up calling back into perl */
    w32_poll_count = 0;

    if (hwnd != INVALID_HANDLE_VALUE) {
        /* Passing PeekMessage -1 as HWND (2nd arg) only gets PostThreadMessage() messages
        * and ignores window messages - should co-exist better with windows apps e.g. Tk
        */
        if (hwnd == NULL)
            hwnd = (HWND)-1;

        while (PeekMessage(&msg, hwnd, WM_TIMER,    WM_TIMER,    PM_REMOVE|PM_NOYIELD) ||
               PeekMessage(&msg, hwnd, WM_USER_MIN, WM_USER_MAX, PM_REMOVE|PM_NOYIELD))
        {
            /* re-post a WM_QUIT message (we'll mark it as read later) */
            if(msg.message == WM_QUIT) {
                PostQuitMessage((int)msg.wParam);
                break;
            }

            if(!CallMsgFilter(&msg, MSGF_USER))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }

    /* Call PeekMessage() to mark all pending messages in the queue as "old".
     * This is necessary when we are being called by win32_msgwait() to
     * make sure MsgWaitForMultipleObjects() stops reporting the same waiting
     * message over and over.  An example how this can happen is when
     * Perl is calling win32_waitpid() inside a GUI application and the GUI
     * is generating messages before the process terminated.
     */
    PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE|PM_NOYIELD);

    /* Above or other stuff may have set a signal flag */
    if (PL_sig_pending)
        despatch_signals();
    
    return 1;
}

/* This function will not return until the timeout has elapsed, or until
 * one of the handles is ready. */
DllExport DWORD
win32_msgwait(pTHX_ DWORD count, LPHANDLE handles, DWORD timeout, LPDWORD resultp)
{
    /* We may need several goes at this - so compute when we stop */
    DWORD ticks = 0;
    if (timeout != INFINITE) {
	ticks = GetTickCount();
	timeout += ticks;
    }
    while (1) {
	DWORD result = MsgWaitForMultipleObjects(count,handles,FALSE,timeout-ticks, QS_POSTMESSAGE|QS_TIMER|QS_SENDMESSAGE);
	if (resultp)
	   *resultp = result;
	if (result == WAIT_TIMEOUT) {
	    /* Ran out of time - explicit return of zero to avoid -ve if we
	       have scheduling issues
             */
	    return 0;
	}
	if (timeout != INFINITE) {
	    ticks = GetTickCount();
        }
	if (result == WAIT_OBJECT_0 + count) {
	    /* Message has arrived - check it */
	    (void)win32_async_check(aTHX);
	}
	else {
	   /* Not timeout or message - one of handles is ready */
	   break;
	}
    }
    /* compute time left to wait */
    ticks = timeout - ticks;
    /* If we are past the end say zero */
    return (ticks > 0) ? ticks : 0;
}

int
win32_internal_wait(int *status, DWORD timeout)
{
    /* XXX this wait emulation only knows about processes
     * spawned via win32_spawnvp(P_NOWAIT, ...).
     */
    dTHX;
    int i, retval;
    DWORD exitcode, waitcode;

#ifdef USE_ITHREADS
    if (w32_num_pseudo_children) {
	win32_msgwait(aTHX_ w32_num_pseudo_children, w32_pseudo_child_handles,
		      timeout, &waitcode);
        /* Time out here if there are no other children to wait for. */
	if (waitcode == WAIT_TIMEOUT) {
	    if (!w32_num_children) {
		return 0;
	    }
	}
	else if (waitcode != WAIT_FAILED) {
	    if (waitcode >= WAIT_ABANDONED_0
		&& waitcode < WAIT_ABANDONED_0 + w32_num_pseudo_children)
		i = waitcode - WAIT_ABANDONED_0;
	    else
		i = waitcode - WAIT_OBJECT_0;
	    if (GetExitCodeThread(w32_pseudo_child_handles[i], &exitcode)) {
		*status = (int)((exitcode & 0xff) << 8);
		retval = (int)w32_pseudo_child_pids[i];
		remove_dead_pseudo_process(i);
		return -retval;
	    }
	}
    }
#endif

    if (!w32_num_children) {
	errno = ECHILD;
	return -1;
    }

    /* if a child exists, wait for it to die */
    win32_msgwait(aTHX_ w32_num_children, w32_child_handles, timeout, &waitcode);
    if (waitcode == WAIT_TIMEOUT) {
	return 0;
    }
    if (waitcode != WAIT_FAILED) {
	if (waitcode >= WAIT_ABANDONED_0
	    && waitcode < WAIT_ABANDONED_0 + w32_num_children)
	    i = waitcode - WAIT_ABANDONED_0;
	else
	    i = waitcode - WAIT_OBJECT_0;
	if (GetExitCodeProcess(w32_child_handles[i], &exitcode) ) {
	    *status = (int)((exitcode & 0xff) << 8);
	    retval = (int)w32_child_pids[i];
	    remove_dead_process(i);
	    return retval;
	}
    }

    errno = GetLastError();
    return -1;
}

DllExport int
win32_waitpid(int pid, int *status, int flags)
{
    dTHX;
    DWORD timeout = (flags & WNOHANG) ? 0 : INFINITE;
    int retval = -1;
    long child;
    if (pid == -1)				/* XXX threadid == 1 ? */
	return win32_internal_wait(status, timeout);
#ifdef USE_ITHREADS
    else if (pid < 0) {
	child = find_pseudo_pid(-pid);
	if (child >= 0) {
	    HANDLE hThread = w32_pseudo_child_handles[child];
	    DWORD waitcode;
	    win32_msgwait(aTHX_ 1, &hThread, timeout, &waitcode);
	    if (waitcode == WAIT_TIMEOUT) {
		return 0;
	    }
	    else if (waitcode == WAIT_OBJECT_0) {
		if (GetExitCodeThread(hThread, &waitcode)) {
		    *status = (int)((waitcode & 0xff) << 8);
		    retval = (int)w32_pseudo_child_pids[child];
		    remove_dead_pseudo_process(child);
		    return -retval;
		}
	    }
	    else
		errno = ECHILD;
	}
	else if (IsWin95()) {
	    pid = -pid;
	    goto alien_process;
	}
    }
#endif
    else {
	HANDLE hProcess;
	DWORD waitcode;
	child = find_pid(pid);
	if (child >= 0) {
	    hProcess = w32_child_handles[child];
	    win32_msgwait(aTHX_ 1, &hProcess, timeout, &waitcode);
	    if (waitcode == WAIT_TIMEOUT) {
		return 0;
	    }
	    else if (waitcode == WAIT_OBJECT_0) {
		if (GetExitCodeProcess(hProcess, &waitcode)) {
		    *status = (int)((waitcode & 0xff) << 8);
		    retval = (int)w32_child_pids[child];
		    remove_dead_process(child);
		    return retval;
		}
	    }
	    else
		errno = ECHILD;
	}
	else {
alien_process:
	    hProcess = OpenProcess(PROCESS_ALL_ACCESS, TRUE,
				   (IsWin95() ? -pid : pid));
	    if (hProcess) {
		win32_msgwait(aTHX_ 1, &hProcess, timeout, &waitcode);
		if (waitcode == WAIT_TIMEOUT) {
                    CloseHandle(hProcess);
		    return 0;
		}
		else if (waitcode == WAIT_OBJECT_0) {
		    if (GetExitCodeProcess(hProcess, &waitcode)) {
			*status = (int)((waitcode & 0xff) << 8);
			CloseHandle(hProcess);
			return pid;
		    }
		}
		CloseHandle(hProcess);
	    }
	    else
		errno = ECHILD;
	}
    }
    return retval >= 0 ? pid : retval;
}

DllExport int
win32_wait(int *status)
{
    return win32_internal_wait(status, INFINITE);
}

DllExport unsigned int
win32_sleep(unsigned int t)
{
    dTHX;
    /* Win32 times are in ms so *1000 in and /1000 out */
    return win32_msgwait(aTHX_ 0, NULL, t*1000, NULL)/1000;
}

DllExport unsigned int
win32_alarm(unsigned int sec)
{
    /*
     * the 'obvious' implentation is SetTimer() with a callback
     * which does whatever receiving SIGALRM would do
     * we cannot use SIGALRM even via raise() as it is not
     * one of the supported codes in <signal.h>
     */
    dTHX;

    if (w32_message_hwnd == INVALID_HANDLE_VALUE)
        w32_message_hwnd = win32_create_message_window();

    if (sec) {
        if (w32_message_hwnd == NULL)
            w32_timerid = SetTimer(NULL, w32_timerid, sec*1000, NULL);
        else {
  	    w32_timerid = 1;
            SetTimer(w32_message_hwnd, w32_timerid, sec*1000, NULL);
        }
    }
    else {
    	if (w32_timerid) {
            KillTimer(w32_message_hwnd, w32_timerid);
  	    w32_timerid = 0;
    	}
    }
    return 0;
}

#ifdef HAVE_DES_FCRYPT
extern char *	des_fcrypt(const char *txt, const char *salt, char *cbuf);
#endif

DllExport char *
win32_crypt(const char *txt, const char *salt)
{
    dTHX;
#ifdef HAVE_DES_FCRYPT
    return des_fcrypt(txt, salt, w32_crypt_buffer);
#else
    Perl_croak(aTHX_ "The crypt() function is unimplemented due to excessive paranoia.");
    return NULL;
#endif
}

#ifdef USE_FIXED_OSFHANDLE

#define FOPEN			0x01	/* file handle open */
#define FNOINHERIT		0x10	/* file handle opened O_NOINHERIT */
#define FAPPEND			0x20	/* file handle opened O_APPEND */
#define FDEV			0x40	/* file handle refers to device */
#define FTEXT			0x80	/* file handle is in text mode */

/***
*int my_open_osfhandle(intptr_t osfhandle, int flags) - open C Runtime file handle
*
*Purpose:
*       This function allocates a free C Runtime file handle and associates
*       it with the Win32 HANDLE specified by the first parameter. This is a
*	temperary fix for WIN95's brain damage GetFileType() error on socket
*	we just bypass that call for socket
*
*	This works with MSVC++ 4.0+ or GCC/Mingw32
*
*Entry:
*       intptr_t osfhandle - Win32 HANDLE to associate with C Runtime file handle.
*       int flags      - flags to associate with C Runtime file handle.
*
*Exit:
*       returns index of entry in fh, if successful
*       return -1, if no free entry is found
*
*Exceptions:
*
*******************************************************************************/

/*
 * we fake up some parts of the CRT that aren't exported by MSVCRT.dll
 * this lets sockets work on Win9X with GCC and should fix the problems
 * with perl95.exe
 *	-- BKS, 1-23-2000
*/

/* create an ioinfo entry, kill its handle, and steal the entry */

static int
_alloc_osfhnd(void)
{
    HANDLE hF = CreateFile("NUL", 0, 0, NULL, OPEN_ALWAYS, 0, NULL);
    int fh = _open_osfhandle((intptr_t)hF, 0);
    CloseHandle(hF);
    if (fh == -1)
        return fh;
    EnterCriticalSection(&(_pioinfo(fh)->lock));
    return fh;
}

static int
my_open_osfhandle(intptr_t osfhandle, int flags)
{
    int fh;
    char fileflags;		/* _osfile flags */

    /* copy relevant flags from second parameter */
    fileflags = FDEV;

    if (flags & O_APPEND)
	fileflags |= FAPPEND;

    if (flags & O_TEXT)
	fileflags |= FTEXT;

    if (flags & O_NOINHERIT)
	fileflags |= FNOINHERIT;

    /* attempt to allocate a C Runtime file handle */
    if ((fh = _alloc_osfhnd()) == -1) {
	errno = EMFILE;		/* too many open files */
	_doserrno = 0L;		/* not an OS error */
	return -1;		/* return error to caller */
    }

    /* the file is open. now, set the info in _osfhnd array */
    _set_osfhnd(fh, osfhandle);

    fileflags |= FOPEN;		/* mark as open */

    _osfile(fh) = fileflags;	/* set osfile entry */
    LeaveCriticalSection(&_pioinfo(fh)->lock);

    return fh;			/* return handle */
}

#endif	/* USE_FIXED_OSFHANDLE */

/* simulate flock by locking a range on the file */

#define LK_ERR(f,i)	((f) ? (i = 0) : (errno = GetLastError()))
#define LK_LEN		0xffff0000

DllExport int
win32_flock(int fd, int oper)
{
    OVERLAPPED o;
    int i = -1;
    HANDLE fh;

    if (!IsWinNT()) {
	dTHX;
	Perl_croak_nocontext("flock() unimplemented on this platform");
	return -1;
    }
    fh = (HANDLE)_get_osfhandle(fd);
    memset(&o, 0, sizeof(o));

    switch(oper) {
    case LOCK_SH:		/* shared lock */
	LK_ERR(LockFileEx(fh, 0, 0, LK_LEN, 0, &o),i);
	break;
    case LOCK_EX:		/* exclusive lock */
	LK_ERR(LockFileEx(fh, LOCKFILE_EXCLUSIVE_LOCK, 0, LK_LEN, 0, &o),i);
	break;
    case LOCK_SH|LOCK_NB:	/* non-blocking shared lock */
	LK_ERR(LockFileEx(fh, LOCKFILE_FAIL_IMMEDIATELY, 0, LK_LEN, 0, &o),i);
	break;
    case LOCK_EX|LOCK_NB:	/* non-blocking exclusive lock */
	LK_ERR(LockFileEx(fh,
		       LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,
		       0, LK_LEN, 0, &o),i);
	break;
    case LOCK_UN:		/* unlock lock */
	LK_ERR(UnlockFileEx(fh, 0, LK_LEN, 0, &o),i);
	break;
    default:			/* unknown */
	errno = EINVAL;
	break;
    }
    return i;
}

#undef LK_ERR
#undef LK_LEN

/*
 *  redirected io subsystem for all XS modules
 *
 */

DllExport int *
win32_errno(void)
{
    return (&errno);
}

DllExport char ***
win32_environ(void)
{
    return (&(_environ));
}

/* the rest are the remapped stdio routines */
DllExport FILE *
win32_stderr(void)
{
    return (stderr);
}

DllExport FILE *
win32_stdin(void)
{
    return (stdin);
}

DllExport FILE *
win32_stdout(void)
{
    return (stdout);
}

DllExport int
win32_ferror(FILE *fp)
{
    return (ferror(fp));
}


DllExport int
win32_feof(FILE *fp)
{
    return (feof(fp));
}

/*
 * Since the errors returned by the socket error function
 * WSAGetLastError() are not known by the library routine strerror
 * we have to roll our own.
 */

DllExport char *
win32_strerror(int e)
{
#if !defined __BORLANDC__ && !defined __MINGW32__      /* compiler intolerance */
    extern int sys_nerr;
#endif
    DWORD source = 0;

    if (e < 0 || e > sys_nerr) {
        dTHX;
	if (e < 0)
	    e = GetLastError();

	if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, &source, e, 0,
			  w32_strerror_buffer,
			  sizeof(w32_strerror_buffer), NULL) == 0)
	    strcpy(w32_strerror_buffer, "Unknown Error");

	return w32_strerror_buffer;
    }
    return strerror(e);
}

DllExport void
win32_str_os_error(void *sv, DWORD dwErr)
{
    DWORD dwLen;
    char *sMsg;
    dwLen = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER
			  |FORMAT_MESSAGE_IGNORE_INSERTS
			  |FORMAT_MESSAGE_FROM_SYSTEM, NULL,
			   dwErr, 0, (char *)&sMsg, 1, NULL);
    /* strip trailing whitespace and period */
    if (0 < dwLen) {
	do {
	    --dwLen;	/* dwLen doesn't include trailing null */
	} while (0 < dwLen && isSPACE(sMsg[dwLen]));
	if ('.' != sMsg[dwLen])
	    dwLen++;
	sMsg[dwLen] = '\0';
    }
    if (0 == dwLen) {
	sMsg = (char*)LocalAlloc(0, 64/**sizeof(TCHAR)*/);
	if (sMsg)
	    dwLen = sprintf(sMsg,
			    "Unknown error #0x%lX (lookup 0x%lX)",
			    dwErr, GetLastError());
    }
    if (sMsg) {
	dTHX;
	sv_setpvn((SV*)sv, sMsg, dwLen);
	LocalFree(sMsg);
    }
}

DllExport int
win32_fprintf(FILE *fp, const char *format, ...)
{
    va_list marker;
    va_start(marker, format);     /* Initialize variable arguments. */

    return (vfprintf(fp, format, marker));
}

DllExport int
win32_printf(const char *format, ...)
{
    va_list marker;
    va_start(marker, format);     /* Initialize variable arguments. */

    return (vprintf(format, marker));
}

DllExport int
win32_vfprintf(FILE *fp, const char *format, va_list args)
{
    return (vfprintf(fp, format, args));
}

DllExport int
win32_vprintf(const char *format, va_list args)
{
    return (vprintf(format, args));
}

DllExport size_t
win32_fread(void *buf, size_t size, size_t count, FILE *fp)
{
    return fread(buf, size, count, fp);
}

DllExport size_t
win32_fwrite(const void *buf, size_t size, size_t count, FILE *fp)
{
    return fwrite(buf, size, count, fp);
}

#define MODE_SIZE 10

DllExport FILE *
win32_fopen(const char *filename, const char *mode)
{
    dTHX;
    FILE *f;

    if (!*filename)
	return NULL;

    if (stricmp(filename, "/dev/null")==0)
	filename = "NUL";

    f = fopen(PerlDir_mapA(filename), mode);
    /* avoid buffering headaches for child processes */
    if (f && *mode == 'a')
	win32_fseek(f, 0, SEEK_END);
    return f;
}

#ifndef USE_SOCKETS_AS_HANDLES
#undef fdopen
#define fdopen my_fdopen
#endif

DllExport FILE *
win32_fdopen(int handle, const char *mode)
{
    dTHX;
    FILE *f;
    f = fdopen(handle, (char *) mode);
    /* avoid buffering headaches for child processes */
    if (f && *mode == 'a')
	win32_fseek(f, 0, SEEK_END);
    return f;
}

DllExport FILE *
win32_freopen(const char *path, const char *mode, FILE *stream)
{
    dTHX;
    if (stricmp(path, "/dev/null")==0)
	path = "NUL";

    return freopen(PerlDir_mapA(path), mode, stream);
}

DllExport int
win32_fclose(FILE *pf)
{
    return my_fclose(pf);	/* defined in win32sck.c */
}

DllExport int
win32_fputs(const char *s,FILE *pf)
{
    return fputs(s, pf);
}

DllExport int
win32_fputc(int c,FILE *pf)
{
    return fputc(c,pf);
}

DllExport int
win32_ungetc(int c,FILE *pf)
{
    return ungetc(c,pf);
}

DllExport int
win32_getc(FILE *pf)
{
    return getc(pf);
}

DllExport int
win32_fileno(FILE *pf)
{
    return fileno(pf);
}

DllExport void
win32_clearerr(FILE *pf)
{
    clearerr(pf);
    return;
}

DllExport int
win32_fflush(FILE *pf)
{
    return fflush(pf);
}

DllExport Off_t
win32_ftell(FILE *pf)
{
#if defined(WIN64) || defined(USE_LARGE_FILES)
#if defined(__BORLANDC__) /* buk */
    return win32_tell( fileno( pf ) );
#else
    fpos_t pos;
    if (fgetpos(pf, &pos))
	return -1;
    return (Off_t)pos;
#endif
#else
    return ftell(pf);
#endif
}

DllExport int
win32_fseek(FILE *pf, Off_t offset,int origin)
{
#if defined(WIN64) || defined(USE_LARGE_FILES)
#if defined(__BORLANDC__) /* buk */
    return win32_lseek(
        fileno(pf),
        offset,
        origin
        );
#else
    fpos_t pos;
    switch (origin) {
    case SEEK_CUR:
	if (fgetpos(pf, &pos))
	    return -1;
	offset += pos;
	break;
    case SEEK_END:
	fseek(pf, 0, SEEK_END);
	pos = _telli64(fileno(pf));
	offset += pos;
	break;
    case SEEK_SET:
	break;
    default:
	errno = EINVAL;
	return -1;
    }
    return fsetpos(pf, &offset);
#endif
#else
    return fseek(pf, (long)offset, origin);
#endif
}

DllExport int
win32_fgetpos(FILE *pf,fpos_t *p)
{
#if defined(__BORLANDC__) && defined(USE_LARGE_FILES) /* buk */
    if( win32_tell(fileno(pf)) == -1L ) {
        errno = EBADF;
        return -1;
    }
    return 0;
#else
    return fgetpos(pf, p);
#endif
}

DllExport int
win32_fsetpos(FILE *pf,const fpos_t *p)
{
#if defined(__BORLANDC__) && defined(USE_LARGE_FILES) /* buk */
    return win32_lseek(fileno(pf), *p, SEEK_CUR);
#else
    return fsetpos(pf, p);
#endif
}

DllExport void
win32_rewind(FILE *pf)
{
    rewind(pf);
    return;
}

DllExport int
win32_tmpfd(void)
{
    dTHX;
    char prefix[MAX_PATH+1];
    char filename[MAX_PATH+1];
    DWORD len = GetTempPath(MAX_PATH, prefix);
    if (len && len < MAX_PATH) {
	if (GetTempFileName(prefix, "plx", 0, filename)) {
	    HANDLE fh = CreateFile(filename,
				   DELETE | GENERIC_READ | GENERIC_WRITE,
				   0,
				   NULL,
				   CREATE_ALWAYS,
				   FILE_ATTRIBUTE_NORMAL
				   | FILE_FLAG_DELETE_ON_CLOSE,
				   NULL);
	    if (fh != INVALID_HANDLE_VALUE) {
		int fd = win32_open_osfhandle((intptr_t)fh, 0);
		if (fd >= 0) {
#if defined(__BORLANDC__)
        	    setmode(fd,O_BINARY);
#endif
		    DEBUG_p(PerlIO_printf(Perl_debug_log,
					  "Created tmpfile=%s\n",filename));
		    return fd;
		}
	    }
	}
    }
    return -1;
}

DllExport FILE*
win32_tmpfile(void)
{
    int fd = win32_tmpfd();
    if (fd >= 0)
	return win32_fdopen(fd, "w+b");
    return NULL;
}

DllExport void
win32_abort(void)
{
    abort();
    return;
}

DllExport int
win32_fstat(int fd, Stat_t *sbufptr)
{
#ifdef __BORLANDC__
    /* A file designated by filehandle is not shown as accessible
     * for write operations, probably because it is opened for reading.
     * --Vadim Konovalov
     */
    BY_HANDLE_FILE_INFORMATION bhfi;
#if defined(WIN64) || defined(USE_LARGE_FILES)    
    /* Borland 5.5.1 has a 64-bit stat, but only a 32-bit fstat */
    struct stat tmp;
    int rc = fstat(fd,&tmp);
   
    sbufptr->st_dev   = tmp.st_dev;
    sbufptr->st_ino   = tmp.st_ino;
    sbufptr->st_mode  = tmp.st_mode;
    sbufptr->st_nlink = tmp.st_nlink;
    sbufptr->st_uid   = tmp.st_uid;
    sbufptr->st_gid   = tmp.st_gid;
    sbufptr->st_rdev  = tmp.st_rdev;
    sbufptr->st_size  = tmp.st_size;
    sbufptr->st_atime = tmp.st_atime;
    sbufptr->st_mtime = tmp.st_mtime;
    sbufptr->st_ctime = tmp.st_ctime;
#else
    int rc = fstat(fd,sbufptr);
#endif       

    if (GetFileInformationByHandle((HANDLE)_get_osfhandle(fd), &bhfi)) {
#if defined(WIN64) || defined(USE_LARGE_FILES)    
        sbufptr->st_size = ((__int64)bhfi.nFileSizeHigh << 32) | bhfi.nFileSizeLow ;
#endif
        sbufptr->st_mode &= 0xFE00;
        if (bhfi.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
            sbufptr->st_mode |= (S_IREAD + (S_IREAD >> 3) + (S_IREAD >> 6));
        else
            sbufptr->st_mode |= ((S_IREAD|S_IWRITE) + ((S_IREAD|S_IWRITE) >> 3)
              + ((S_IREAD|S_IWRITE) >> 6));
    }
    return rc;
#else
    return my_fstat(fd,sbufptr);
#endif
}

DllExport int
win32_pipe(int *pfd, unsigned int size, int mode)
{
    return _pipe(pfd, size, mode);
}

DllExport PerlIO*
win32_popenlist(const char *mode, IV narg, SV **args)
{
 dTHX;
 Perl_croak(aTHX_ "List form of pipe open not implemented");
 return NULL;
}

/*
 * a popen() clone that respects PERL5SHELL
 *
 * changed to return PerlIO* rather than FILE * by BKS, 11-11-2000
 */

DllExport PerlIO*
win32_popen(const char *command, const char *mode)
{
#ifdef USE_RTL_POPEN
    return _popen(command, mode);
#else
    dTHX;
    int p[2];
    int parent, child;
    int stdfd, oldfd;
    int ourmode;
    int childpid;
    DWORD nhandle;
    HANDLE old_h;
    int lock_held = 0;

    /* establish which ends read and write */
    if (strchr(mode,'w')) {
        stdfd = 0;		/* stdin */
        parent = 1;
        child = 0;
	nhandle = STD_INPUT_HANDLE;
    }
    else if (strchr(mode,'r')) {
        stdfd = 1;		/* stdout */
        parent = 0;
        child = 1;
	nhandle = STD_OUTPUT_HANDLE;
    }
    else
        return NULL;

    /* set the correct mode */
    if (strchr(mode,'b'))
        ourmode = O_BINARY;
    else if (strchr(mode,'t'))
        ourmode = O_TEXT;
    else
        ourmode = _fmode & (O_TEXT | O_BINARY);

    /* the child doesn't inherit handles */
    ourmode |= O_NOINHERIT;

    if (win32_pipe(p, 512, ourmode) == -1)
        return NULL;

    /* save the old std handle (this needs to happen before the
     * dup2(), since that might call SetStdHandle() too) */
    OP_REFCNT_LOCK;
    lock_held = 1;
    old_h = GetStdHandle(nhandle);

    /* save current stdfd */
    if ((oldfd = win32_dup(stdfd)) == -1)
        goto cleanup;

    /* make stdfd go to child end of pipe (implicitly closes stdfd) */
    /* stdfd will be inherited by the child */
    if (win32_dup2(p[child], stdfd) == -1)
        goto cleanup;

    /* close the child end in parent */
    win32_close(p[child]);

    /* set the new std handle (in case dup2() above didn't) */
    SetStdHandle(nhandle, (HANDLE)_get_osfhandle(stdfd));

    /* start the child */
    {
	dTHX;
	if ((childpid = do_spawn_nowait((char*)command)) == -1)
	    goto cleanup;

	/* revert stdfd to whatever it was before */
	if (win32_dup2(oldfd, stdfd) == -1)
	    goto cleanup;

	/* close saved handle */
	win32_close(oldfd);

	/* restore the old std handle (this needs to happen after the
	 * dup2(), since that might call SetStdHandle() too */
	if (lock_held) {
	    SetStdHandle(nhandle, old_h);
	    OP_REFCNT_UNLOCK;
	    lock_held = 0;
	}

	LOCK_FDPID_MUTEX;
	sv_setiv(*av_fetch(w32_fdpid, p[parent], TRUE), childpid);
	UNLOCK_FDPID_MUTEX;

	/* set process id so that it can be returned by perl's open() */
	PL_forkprocess = childpid;
    }

    /* we have an fd, return a file stream */
    return (PerlIO_fdopen(p[parent], (char *)mode));

cleanup:
    /* we don't need to check for errors here */
    win32_close(p[0]);
    win32_close(p[1]);
    if (oldfd != -1) {
        win32_dup2(oldfd, stdfd);
        win32_close(oldfd);
    }
    if (lock_held) {
	SetStdHandle(nhandle, old_h);
	OP_REFCNT_UNLOCK;
	lock_held = 0;
    }
    return (NULL);

#endif /* USE_RTL_POPEN */
}

/*
 * pclose() clone
 */

DllExport int
win32_pclose(PerlIO *pf)
{
#ifdef USE_RTL_POPEN
    return _pclose(pf);
#else
    dTHX;
    int childpid, status;
    SV *sv;

    LOCK_FDPID_MUTEX;
    sv = *av_fetch(w32_fdpid, PerlIO_fileno(pf), TRUE);

    if (SvIOK(sv))
	childpid = SvIVX(sv);
    else
	childpid = 0;

    if (!childpid) {
        UNLOCK_FDPID_MUTEX;
	errno = EBADF;
        return -1;
    }

#ifdef USE_PERLIO
    PerlIO_close(pf);
#else
    fclose(pf);
#endif
    SvIVX(sv) = 0;
    UNLOCK_FDPID_MUTEX;

    if (win32_waitpid(childpid, &status, 0) == -1)
        return -1;

    return status;

#endif /* USE_RTL_POPEN */
}

static BOOL WINAPI
Nt4CreateHardLinkW(
    LPCWSTR lpFileName,
    LPCWSTR lpExistingFileName,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    HANDLE handle;
    WCHAR wFullName[MAX_PATH+1];
    LPVOID lpContext = NULL;
    WIN32_STREAM_ID StreamId;
    DWORD dwSize = (char*)&StreamId.cStreamName - (char*)&StreamId;
    DWORD dwWritten;
    DWORD dwLen;
    BOOL bSuccess;

    BOOL (__stdcall *pfnBackupWrite)(HANDLE, LPBYTE, DWORD, LPDWORD,
				     BOOL, BOOL, LPVOID*) =
	(BOOL (__stdcall *)(HANDLE, LPBYTE, DWORD, LPDWORD,
			    BOOL, BOOL, LPVOID*))
	GetProcAddress(GetModuleHandle("kernel32.dll"), "BackupWrite");
    if (pfnBackupWrite == NULL)
	return 0;

    dwLen = GetFullPathNameW(lpFileName, MAX_PATH, wFullName, NULL);
    if (dwLen == 0)
	return 0;
    dwLen = (dwLen+1)*sizeof(WCHAR);

    handle = CreateFileW(lpExistingFileName, FILE_WRITE_ATTRIBUTES,
			 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			 NULL, OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE)
	return 0;

    StreamId.dwStreamId = BACKUP_LINK;
    StreamId.dwStreamAttributes = 0;
    StreamId.dwStreamNameSize = 0;
#if defined(__BORLANDC__) \
 ||(defined(__MINGW32__) && !defined(_ANONYMOUS_UNION))
    StreamId.Size.u.HighPart = 0;
    StreamId.Size.u.LowPart = dwLen;
#else
    StreamId.Size.HighPart = 0;
    StreamId.Size.LowPart = dwLen;
#endif

    bSuccess = pfnBackupWrite(handle, (LPBYTE)&StreamId, dwSize, &dwWritten,
			      FALSE, FALSE, &lpContext);
    if (bSuccess) {
	bSuccess = pfnBackupWrite(handle, (LPBYTE)wFullName, dwLen, &dwWritten,
				  FALSE, FALSE, &lpContext);
	pfnBackupWrite(handle, NULL, 0, &dwWritten, TRUE, FALSE, &lpContext);
    }

    CloseHandle(handle);
    return bSuccess;
}

DllExport int
win32_link(const char *oldname, const char *newname)
{
    dTHX;
    BOOL (__stdcall *pfnCreateHardLinkW)(LPCWSTR,LPCWSTR,LPSECURITY_ATTRIBUTES);
    WCHAR wOldName[MAX_PATH+1];
    WCHAR wNewName[MAX_PATH+1];

    if (IsWin95())
	Perl_croak(aTHX_ PL_no_func, "link");

    pfnCreateHardLinkW =
	(BOOL (__stdcall *)(LPCWSTR, LPCWSTR, LPSECURITY_ATTRIBUTES))
	GetProcAddress(GetModuleHandle("kernel32.dll"), "CreateHardLinkW");
    if (pfnCreateHardLinkW == NULL)
	pfnCreateHardLinkW = Nt4CreateHardLinkW;

    if (MultiByteToWideChar(CP_ACP, 0, oldname, -1, wOldName, MAX_PATH+1) &&
        MultiByteToWideChar(CP_ACP, 0, newname, -1, wNewName, MAX_PATH+1) &&
	(wcscpy(wOldName, PerlDir_mapW(wOldName)),
        pfnCreateHardLinkW(PerlDir_mapW(wNewName), wOldName, NULL)))
    {
	return 0;
    }
    errno = (GetLastError() == ERROR_FILE_NOT_FOUND) ? ENOENT : EINVAL;
    return -1;
}

DllExport int
win32_rename(const char *oname, const char *newname)
{
    char szOldName[MAX_PATH+1];
    char szNewName[MAX_PATH+1];
    BOOL bResult;
    dTHX;

    /* XXX despite what the documentation says about MoveFileEx(),
     * it doesn't work under Windows95!
     */
    if (IsWinNT()) {
	DWORD dwFlags = MOVEFILE_COPY_ALLOWED;
        if (stricmp(newname, oname))
            dwFlags |= MOVEFILE_REPLACE_EXISTING;
        strcpy(szOldName, PerlDir_mapA(oname));
        bResult = MoveFileExA(szOldName,PerlDir_mapA(newname), dwFlags);
	if (!bResult) {
	    DWORD err = GetLastError();
	    switch (err) {
	    case ERROR_BAD_NET_NAME:
	    case ERROR_BAD_NETPATH:
	    case ERROR_BAD_PATHNAME:
	    case ERROR_FILE_NOT_FOUND:
	    case ERROR_FILENAME_EXCED_RANGE:
	    case ERROR_INVALID_DRIVE:
	    case ERROR_NO_MORE_FILES:
	    case ERROR_PATH_NOT_FOUND:
		errno = ENOENT;
		break;
	    default:
		errno = EACCES;
		break;
	    }
	    return -1;
	}
	return 0;
    }
    else {
	int retval = 0;
	char szTmpName[MAX_PATH+1];
	char dname[MAX_PATH+1];
	char *endname = NULL;
	STRLEN tmplen = 0;
	DWORD from_attr, to_attr;

	strcpy(szOldName, PerlDir_mapA(oname));
	strcpy(szNewName, PerlDir_mapA(newname));

	/* if oname doesn't exist, do nothing */
	from_attr = GetFileAttributes(szOldName);
	if (from_attr == 0xFFFFFFFF) {
	    errno = ENOENT;
	    return -1;
	}

	/* if newname exists, rename it to a temporary name so that we
	 * don't delete it in case oname happens to be the same file
	 * (but perhaps accessed via a different path)
	 */
	to_attr = GetFileAttributes(szNewName);
	if (to_attr != 0xFFFFFFFF) {
	    /* if newname is a directory, we fail
	     * XXX could overcome this with yet more convoluted logic */
	    if (to_attr & FILE_ATTRIBUTE_DIRECTORY) {
		errno = EACCES;
		return -1;
	    }
	    tmplen = strlen(szNewName);
	    strcpy(szTmpName,szNewName);
	    endname = szTmpName+tmplen;
	    for (; endname > szTmpName ; --endname) {
		if (*endname == '/' || *endname == '\\') {
		    *endname = '\0';
		    break;
		}
	    }
	    if (endname > szTmpName)
		endname = strcpy(dname,szTmpName);
	    else
		endname = ".";

	    /* get a temporary filename in same directory
	     * XXX is this really the best we can do? */
	    if (!GetTempFileName((LPCTSTR)endname, "plr", 0, szTmpName)) {
		errno = ENOENT;
		return -1;
	    }
	    DeleteFile(szTmpName);

	    retval = rename(szNewName, szTmpName);
	    if (retval != 0) {
		errno = EACCES;
		return retval;
	    }
	}

	/* rename oname to newname */
	retval = rename(szOldName, szNewName);

	/* if we created a temporary file before ... */
	if (endname != NULL) {
	    /* ...and rename succeeded, delete temporary file/directory */
	    if (retval == 0)
		DeleteFile(szTmpName);
	    /* else restore it to what it was */
	    else
		(void)rename(szTmpName, szNewName);
	}
	return retval;
    }
}

DllExport int
win32_setmode(int fd, int mode)
{
    return setmode(fd, mode);
}

DllExport int
win32_chsize(int fd, Off_t size)
{
#if defined(WIN64) || defined(USE_LARGE_FILES)
    int retval = 0;
    Off_t cur, end, extend;

    cur = win32_tell(fd);
    if (cur < 0)
	return -1;
    end = win32_lseek(fd, 0, SEEK_END);
    if (end < 0)
	return -1;
    extend = size - end;
    if (extend == 0) {
	/* do nothing */
    }
    else if (extend > 0) {
	/* must grow the file, padding with nulls */
	char b[4096];
	int oldmode = win32_setmode(fd, O_BINARY);
	size_t count;
	memset(b, '\0', sizeof(b));
	do {
	    count = extend >= sizeof(b) ? sizeof(b) : (size_t)extend;
	    count = win32_write(fd, b, count);
	    if ((int)count < 0) {
		retval = -1;
		break;
	    }
	} while ((extend -= count) > 0);
	win32_setmode(fd, oldmode);
    }
    else {
	/* shrink the file */
	win32_lseek(fd, size, SEEK_SET);
	if (!SetEndOfFile((HANDLE)_get_osfhandle(fd))) {
	    errno = EACCES;
	    retval = -1;
	}
    }
finish:
    win32_lseek(fd, cur, SEEK_SET);
    return retval;
#else
    return chsize(fd, (long)size);
#endif
}

DllExport Off_t
win32_lseek(int fd, Off_t offset, int origin)
{
#if defined(WIN64) || defined(USE_LARGE_FILES)
#if defined(__BORLANDC__) /* buk */
    LARGE_INTEGER pos;
    pos.QuadPart = offset;
    pos.LowPart = SetFilePointer(
        (HANDLE)_get_osfhandle(fd),
        pos.LowPart,
        &pos.HighPart,
        origin
    );
    if (pos.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        pos.QuadPart = -1;
    }

    return pos.QuadPart;
#else
    return _lseeki64(fd, offset, origin);
#endif
#else
    return lseek(fd, (long)offset, origin);
#endif
}

DllExport Off_t
win32_tell(int fd)
{
#if defined(WIN64) || defined(USE_LARGE_FILES)
#if defined(__BORLANDC__) /* buk */
    LARGE_INTEGER pos;
    pos.QuadPart = 0;
    pos.LowPart = SetFilePointer(
        (HANDLE)_get_osfhandle(fd),
        pos.LowPart,
        &pos.HighPart,
        FILE_CURRENT
    );
    if (pos.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
        pos.QuadPart = -1;
    }

    return pos.QuadPart;
    /* return tell(fd); */
#else
    return _telli64(fd);
#endif
#else
    return tell(fd);
#endif
}

DllExport int
win32_open(const char *path, int flag, ...)
{
    dTHX;
    va_list ap;
    int pmode;

    va_start(ap, flag);
    pmode = va_arg(ap, int);
    va_end(ap);

    if (stricmp(path, "/dev/null")==0)
	path = "NUL";

    return open(PerlDir_mapA(path), flag, pmode);
}

/* close() that understands socket */
extern int my_close(int);	/* in win32sck.c */

DllExport int
win32_close(int fd)
{
    return my_close(fd);
}

DllExport int
win32_eof(int fd)
{
    return eof(fd);
}

DllExport int
win32_dup(int fd)
{
    return dup(fd);
}

DllExport int
win32_dup2(int fd1,int fd2)
{
    return dup2(fd1,fd2);
}

#ifdef PERL_MSVCRT_READFIX

#define LF		10	/* line feed */
#define CR		13	/* carriage return */
#define CTRLZ		26      /* ctrl-z means eof for text */
#define FOPEN		0x01	/* file handle open */
#define FEOFLAG		0x02	/* end of file has been encountered */
#define FCRLF		0x04	/* CR-LF across read buffer (in text mode) */
#define FPIPE		0x08	/* file handle refers to a pipe */
#define FAPPEND		0x20	/* file handle opened O_APPEND */
#define FDEV		0x40	/* file handle refers to device */
#define FTEXT		0x80	/* file handle is in text mode */
#define MAX_DESCRIPTOR_COUNT	(64*32) /* this is the maximun that MSVCRT can handle */

int __cdecl
_fixed_read(int fh, void *buf, unsigned cnt)
{
    int bytes_read;                 /* number of bytes read */
    char *buffer;                   /* buffer to read to */
    int os_read;                    /* bytes read on OS call */
    char *p, *q;                    /* pointers into buffer */
    char peekchr;                   /* peek-ahead character */
    ULONG filepos;                  /* file position after seek */
    ULONG dosretval;                /* o.s. return value */

    /* validate handle */
    if (((unsigned)fh >= (unsigned)MAX_DESCRIPTOR_COUNT) ||
         !(_osfile(fh) & FOPEN))
    {
	/* out of range -- return error */
	errno = EBADF;
	_doserrno = 0;  /* not o.s. error */
	return -1;
    }

    /*
     * If lockinitflag is FALSE, assume fd is device
     * lockinitflag is set to TRUE by open.
     */
    if (_pioinfo(fh)->lockinitflag)
	EnterCriticalSection(&(_pioinfo(fh)->lock));  /* lock file */

    bytes_read = 0;                 /* nothing read yet */
    buffer = (char*)buf;

    if (cnt == 0 || (_osfile(fh) & FEOFLAG)) {
        /* nothing to read or at EOF, so return 0 read */
        goto functionexit;
    }

    if ((_osfile(fh) & (FPIPE|FDEV)) && _pipech(fh) != LF) {
        /* a pipe/device and pipe lookahead non-empty: read the lookahead
         * char */
        *buffer++ = _pipech(fh);
        ++bytes_read;
        --cnt;
        _pipech(fh) = LF;           /* mark as empty */
    }

    /* read the data */

    if (!ReadFile((HANDLE)_osfhnd(fh), buffer, cnt, (LPDWORD)&os_read, NULL))
    {
        /* ReadFile has reported an error. recognize two special cases.
         *
         *      1. map ERROR_ACCESS_DENIED to EBADF
         *
         *      2. just return 0 if ERROR_BROKEN_PIPE has occurred. it
         *         means the handle is a read-handle on a pipe for which
         *         all write-handles have been closed and all data has been
         *         read. */

        if ((dosretval = GetLastError()) == ERROR_ACCESS_DENIED) {
            /* wrong read/write mode should return EBADF, not EACCES */
            errno = EBADF;
            _doserrno = dosretval;
            bytes_read = -1;
	    goto functionexit;
        }
        else if (dosretval == ERROR_BROKEN_PIPE) {
            bytes_read = 0;
	    goto functionexit;
        }
        else {
            bytes_read = -1;
	    goto functionexit;
        }
    }

    bytes_read += os_read;          /* update bytes read */

    if (_osfile(fh) & FTEXT) {
        /* now must translate CR-LFs to LFs in the buffer */

        /* set CRLF flag to indicate LF at beginning of buffer */
        /* if ((os_read != 0) && (*(char *)buf == LF))   */
        /*    _osfile(fh) |= FCRLF;                      */
        /* else                                          */
        /*    _osfile(fh) &= ~FCRLF;                     */

        _osfile(fh) &= ~FCRLF;

        /* convert chars in the buffer: p is src, q is dest */
        p = q = (char*)buf;
        while (p < (char *)buf + bytes_read) {
            if (*p == CTRLZ) {
                /* if fh is not a device, set ctrl-z flag */
                if (!(_osfile(fh) & FDEV))
                    _osfile(fh) |= FEOFLAG;
                break;              /* stop translating */
            }
            else if (*p != CR)
                *q++ = *p++;
            else {
                /* *p is CR, so must check next char for LF */
                if (p < (char *)buf + bytes_read - 1) {
                    if (*(p+1) == LF) {
                        p += 2;
                        *q++ = LF;  /* convert CR-LF to LF */
                    }
                    else
                        *q++ = *p++;    /* store char normally */
                }
                else {
                    /* This is the hard part.  We found a CR at end of
                       buffer.  We must peek ahead to see if next char
                       is an LF. */
                    ++p;

                    dosretval = 0;
                    if (!ReadFile((HANDLE)_osfhnd(fh), &peekchr, 1,
                                    (LPDWORD)&os_read, NULL))
                        dosretval = GetLastError();

                    if (dosretval != 0 || os_read == 0) {
                        /* couldn't read ahead, store CR */
                        *q++ = CR;
                    }
                    else {
                        /* peekchr now has the extra character -- we now
                           have several possibilities:
                           1. disk file and char is not LF; just seek back
                              and copy CR
                           2. disk file and char is LF; store LF, don't seek back
                           3. pipe/device and char is LF; store LF.
                           4. pipe/device and char isn't LF, store CR and
                              put char in pipe lookahead buffer. */
                        if (_osfile(fh) & (FDEV|FPIPE)) {
                            /* non-seekable device */
                            if (peekchr == LF)
                                *q++ = LF;
                            else {
                                *q++ = CR;
                                _pipech(fh) = peekchr;
                            }
                        }
                        else {
                            /* disk file */
                            if (peekchr == LF) {
                                /* nothing read yet; must make some
                                   progress */
                                *q++ = LF;
                                /* turn on this flag for tell routine */
                                _osfile(fh) |= FCRLF;
                            }
                            else {
				HANDLE osHandle;        /* o.s. handle value */
                                /* seek back */
				if ((osHandle = (HANDLE)_get_osfhandle(fh)) != (HANDLE)-1)
				{
				    if ((filepos = SetFilePointer(osHandle, -1, NULL, FILE_CURRENT)) == -1)
					dosretval = GetLastError();
				}
                                if (peekchr != LF)
                                    *q++ = CR;
                            }
                        }
                    }
                }
            }
        }

        /* we now change bytes_read to reflect the true number of chars
           in the buffer */
        bytes_read = q - (char *)buf;
    }

functionexit:
    if (_pioinfo(fh)->lockinitflag)
	LeaveCriticalSection(&(_pioinfo(fh)->lock));    /* unlock file */

    return bytes_read;
}

#endif	/* PERL_MSVCRT_READFIX */

DllExport int
win32_read(int fd, void *buf, unsigned int cnt)
{
#ifdef PERL_MSVCRT_READFIX
    return _fixed_read(fd, buf, cnt);
#else
    return read(fd, buf, cnt);
#endif
}

DllExport int
win32_write(int fd, const void *buf, unsigned int cnt)
{
    return write(fd, buf, cnt);
}

DllExport int
win32_mkdir(const char *dir, int mode)
{
    dTHX;
    return mkdir(PerlDir_mapA(dir)); /* just ignore mode */
}

DllExport int
win32_rmdir(const char *dir)
{
    dTHX;
    return rmdir(PerlDir_mapA(dir));
}

DllExport int
win32_chdir(const char *dir)
{
    dTHX;
    if (!dir) {
	errno = ENOENT;
	return -1;
    }
    return chdir(dir);
}

DllExport  int
win32_access(const char *path, int mode)
{
    dTHX;
    return access(PerlDir_mapA(path), mode);
}

DllExport  int
win32_chmod(const char *path, int mode)
{
    dTHX;
    return chmod(PerlDir_mapA(path), mode);
}


static char *
create_command_line(char *cname, STRLEN clen, const char * const *args)
{
    dTHX;
    int index, argc;
    char *cmd, *ptr;
    const char *arg;
    STRLEN len = 0;
    bool bat_file = FALSE;
    bool cmd_shell = FALSE;
    bool dumb_shell = FALSE;
    bool extra_quotes = FALSE;
    bool quote_next = FALSE;

    if (!cname)
	cname = (char*)args[0];

    /* The NT cmd.exe shell has the following peculiarity that needs to be
     * worked around.  It strips a leading and trailing dquote when any
     * of the following is true:
     *    1. the /S switch was used
     *    2. there are more than two dquotes
     *    3. there is a special character from this set: &<>()@^|
     *    4. no whitespace characters within the two dquotes
     *    5. string between two dquotes isn't an executable file
     * To work around this, we always add a leading and trailing dquote
     * to the string, if the first argument is either "cmd.exe" or "cmd",
     * and there were at least two or more arguments passed to cmd.exe
     * (not including switches).
     * XXX the above rules (from "cmd /?") don't seem to be applied
     * always, making for the convolutions below :-(
     */
    if (cname) {
	if (!clen)
	    clen = strlen(cname);

	if (clen > 4
	    && (stricmp(&cname[clen-4], ".bat") == 0
		|| (IsWinNT() && stricmp(&cname[clen-4], ".cmd") == 0)))
	{
	    bat_file = TRUE;
	    if (!IsWin95())
		len += 3;
	}
	else {
	    char *exe = strrchr(cname, '/');
	    char *exe2 = strrchr(cname, '\\');
	    if (exe2 > exe)
		exe = exe2;
	    if (exe)
		++exe;
	    else
		exe = cname;
	    if (stricmp(exe, "cmd.exe") == 0 || stricmp(exe, "cmd") == 0) {
		cmd_shell = TRUE;
		len += 3;
	    }
	    else if (stricmp(exe, "command.com") == 0
		     || stricmp(exe, "command") == 0)
	    {
		dumb_shell = TRUE;
	    }
	}
    }

    DEBUG_p(PerlIO_printf(Perl_debug_log, "Args "));
    for (index = 0; (arg = (char*)args[index]) != NULL; ++index) {
	STRLEN curlen = strlen(arg);
	if (!(arg[0] == '"' && arg[curlen-1] == '"'))
	    len += 2;	/* assume quoting needed (worst case) */
	len += curlen + 1;
	DEBUG_p(PerlIO_printf(Perl_debug_log, "[%s]",arg));
    }
    DEBUG_p(PerlIO_printf(Perl_debug_log, "\n"));

    argc = index;
    Newx(cmd, len, char);
    ptr = cmd;

    if (bat_file && !IsWin95()) {
	*ptr++ = '"';
	extra_quotes = TRUE;
    }

    for (index = 0; (arg = (char*)args[index]) != NULL; ++index) {
	bool do_quote = 0;
	STRLEN curlen = strlen(arg);

	/* we want to protect empty arguments and ones with spaces with
	 * dquotes, but only if they aren't already there */
	if (!dumb_shell) {
	    if (!curlen) {
		do_quote = 1;
	    }
	    else if (quote_next) {
		/* see if it really is multiple arguments pretending to
		 * be one and force a set of quotes around it */
		if (*find_next_space(arg))
		    do_quote = 1;
	    }
	    else if (!(arg[0] == '"' && curlen > 1 && arg[curlen-1] == '"')) {
		STRLEN i = 0;
		while (i < curlen) {
		    if (isSPACE(arg[i])) {
			do_quote = 1;
		    }
		    else if (arg[i] == '"') {
			do_quote = 0;
			break;
		    }
		    i++;
		}
	    }
	}

	if (do_quote)
	    *ptr++ = '"';

	strcpy(ptr, arg);
	ptr += curlen;

	if (do_quote)
	    *ptr++ = '"';

	if (args[index+1])
	    *ptr++ = ' ';

    	if (!extra_quotes
	    && cmd_shell
	    && curlen >= 2
	    && *arg  == '/'     /* see if arg is "/c", "/x/c", "/x/d/c" etc. */
	    && stricmp(arg+curlen-2, "/c") == 0)
	{
	    /* is there a next argument? */
	    if (args[index+1]) {
		/* are there two or more next arguments? */
		if (args[index+2]) {
		    *ptr++ = '"';
		    extra_quotes = TRUE;
		}
		else {
		    /* single argument, force quoting if it has spaces */
		    quote_next = TRUE;
		}
	    }
	}
    }

    if (extra_quotes)
	*ptr++ = '"';

    *ptr = '\0';

    return cmd;
}

static char *
qualified_path(const char *cmd)
{
    dTHX;
    char *pathstr;
    char *fullcmd, *curfullcmd;
    STRLEN cmdlen = 0;
    int has_slash = 0;

    if (!cmd)
	return NULL;
    fullcmd = (char*)cmd;
    while (*fullcmd) {
	if (*fullcmd == '/' || *fullcmd == '\\')
	    has_slash++;
	fullcmd++;
	cmdlen++;
    }

    /* look in PATH */
    pathstr = PerlEnv_getenv("PATH");

    /* worst case: PATH is a single directory; we need additional space
     * to append "/", ".exe" and trailing "\0" */
    Newx(fullcmd, (pathstr ? strlen(pathstr) : 0) + cmdlen + 6, char);
    curfullcmd = fullcmd;

    while (1) {
	DWORD res;

	/* start by appending the name to the current prefix */
	strcpy(curfullcmd, cmd);
	curfullcmd += cmdlen;

	/* if it doesn't end with '.', or has no extension, try adding
	 * a trailing .exe first */
	if (cmd[cmdlen-1] != '.'
	    && (cmdlen < 4 || cmd[cmdlen-4] != '.'))
	{
	    strcpy(curfullcmd, ".exe");
	    res = GetFileAttributes(fullcmd);
	    if (res != 0xFFFFFFFF && !(res & FILE_ATTRIBUTE_DIRECTORY))
		return fullcmd;
	    *curfullcmd = '\0';
	}

	/* that failed, try the bare name */
	res = GetFileAttributes(fullcmd);
	if (res != 0xFFFFFFFF && !(res & FILE_ATTRIBUTE_DIRECTORY))
	    return fullcmd;

	/* quit if no other path exists, or if cmd already has path */
	if (!pathstr || !*pathstr || has_slash)
	    break;

	/* skip leading semis */
	while (*pathstr == ';')
	    pathstr++;

	/* build a new prefix from scratch */
	curfullcmd = fullcmd;
	while (*pathstr && *pathstr != ';') {
	    if (*pathstr == '"') {	/* foo;"baz;etc";bar */
		pathstr++;		/* skip initial '"' */
		while (*pathstr && *pathstr != '"') {
                    *curfullcmd++ = *pathstr++;
		}
		if (*pathstr)
		    pathstr++;		/* skip trailing '"' */
	    }
	    else {
                *curfullcmd++ = *pathstr++;
	    }
	}
	if (*pathstr)
	    pathstr++;			/* skip trailing semi */
	if (curfullcmd > fullcmd	/* append a dir separator */
	    && curfullcmd[-1] != '/' && curfullcmd[-1] != '\\')
	{
	    *curfullcmd++ = '\\';
	}
    }

    Safefree(fullcmd);
    return NULL;
}

/* The following are just place holders.
 * Some hosts may provide and environment that the OS is
 * not tracking, therefore, these host must provide that
 * environment and the current directory to CreateProcess
 */

DllExport void*
win32_get_childenv(void)
{
    return NULL;
}

DllExport void
win32_free_childenv(void* d)
{
}

DllExport void
win32_clearenv(void)
{
    char *envv = GetEnvironmentStrings();
    char *cur = envv;
    STRLEN len;
    while (*cur) {
	char *end = strchr(cur,'=');
	if (end && end != cur) {
	    *end = '\0';
	    SetEnvironmentVariable(cur, NULL);
	    *end = '=';
	    cur = end + strlen(end+1)+2;
	}
	else if ((len = strlen(cur)))
	    cur += len+1;
    }
    FreeEnvironmentStrings(envv);
}

DllExport char*
win32_get_childdir(void)
{
    dTHX;
    char* ptr;
    char szfilename[MAX_PATH+1];

    GetCurrentDirectoryA(MAX_PATH+1, szfilename);
    Newx(ptr, strlen(szfilename)+1, char);
    strcpy(ptr, szfilename);
    return ptr;
}

DllExport void
win32_free_childdir(char* d)
{
    dTHX;
    Safefree(d);
}


/* XXX this needs to be made more compatible with the spawnvp()
 * provided by the various RTLs.  In particular, searching for
 * *.{com,bat,cmd} files (as done by the RTLs) is unimplemented.
 * This doesn't significantly affect perl itself, because we
 * always invoke things using PERL5SHELL if a direct attempt to
 * spawn the executable fails.
 *
 * XXX splitting and rejoining the commandline between do_aspawn()
 * and win32_spawnvp() could also be avoided.
 */

DllExport int
win32_spawnvp(int mode, const char *cmdname, const char *const *argv)
{
#ifdef USE_RTL_SPAWNVP
    return spawnvp(mode, cmdname, (char * const *)argv);
#else
    dTHX;
    int ret;
    void* env;
    char* dir;
    child_IO_table tbl;
    STARTUPINFO StartupInfo;
    PROCESS_INFORMATION ProcessInformation;
    DWORD create = 0;
    char *cmd;
    char *fullcmd = NULL;
    char *cname = (char *)cmdname;
    STRLEN clen = 0;

    if (cname) {
	clen = strlen(cname);
	/* if command name contains dquotes, must remove them */
	if (strchr(cname, '"')) {
	    cmd = cname;
	    Newx(cname,clen+1,char);
	    clen = 0;
	    while (*cmd) {
		if (*cmd != '"') {
		    cname[clen] = *cmd;
		    ++clen;
		}
		++cmd;
	    }
	    cname[clen] = '\0';
	}
    }

    cmd = create_command_line(cname, clen, argv);

    env = PerlEnv_get_childenv();
    dir = PerlEnv_get_childdir();

    switch(mode) {
    case P_NOWAIT:	/* asynch + remember result */
	if (w32_num_children >= MAXIMUM_WAIT_OBJECTS) {
	    errno = EAGAIN;
	    ret = -1;
	    goto RETVAL;
	}
	/* Create a new process group so we can use GenerateConsoleCtrlEvent()
	 * in win32_kill()
	 */
        create |= CREATE_NEW_PROCESS_GROUP;
	/* FALL THROUGH */

    case P_WAIT:	/* synchronous execution */
	break;
    default:		/* invalid mode */
	errno = EINVAL;
	ret = -1;
	goto RETVAL;
    }
    memset(&StartupInfo,0,sizeof(StartupInfo));
    StartupInfo.cb = sizeof(StartupInfo);
    memset(&tbl,0,sizeof(tbl));
    PerlEnv_get_child_IO(&tbl);
    StartupInfo.dwFlags		= tbl.dwFlags;
    StartupInfo.dwX		= tbl.dwX;
    StartupInfo.dwY		= tbl.dwY;
    StartupInfo.dwXSize		= tbl.dwXSize;
    StartupInfo.dwYSize		= tbl.dwYSize;
    StartupInfo.dwXCountChars	= tbl.dwXCountChars;
    StartupInfo.dwYCountChars	= tbl.dwYCountChars;
    StartupInfo.dwFillAttribute	= tbl.dwFillAttribute;
    StartupInfo.wShowWindow	= tbl.wShowWindow;
    StartupInfo.hStdInput	= tbl.childStdIn;
    StartupInfo.hStdOutput	= tbl.childStdOut;
    StartupInfo.hStdError	= tbl.childStdErr;
    if (StartupInfo.hStdInput == INVALID_HANDLE_VALUE &&
	StartupInfo.hStdOutput == INVALID_HANDLE_VALUE &&
	StartupInfo.hStdError == INVALID_HANDLE_VALUE)
    {
	create |= CREATE_NEW_CONSOLE;
    }
    else {
	StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    }
    if (w32_use_showwindow) {
        StartupInfo.dwFlags |= STARTF_USESHOWWINDOW;
        StartupInfo.wShowWindow = w32_showwindow;
    }

    DEBUG_p(PerlIO_printf(Perl_debug_log, "Spawning [%s] with [%s]\n",
			  cname,cmd));
RETRY:
    if (!CreateProcess(cname,		/* search PATH to find executable */
		       cmd,		/* executable, and its arguments */
		       NULL,		/* process attributes */
		       NULL,		/* thread attributes */
		       TRUE,		/* inherit handles */
		       create,		/* creation flags */
		       (LPVOID)env,	/* inherit environment */
		       dir,		/* inherit cwd */
		       &StartupInfo,
		       &ProcessInformation))
    {
	/* initial NULL argument to CreateProcess() does a PATH
	 * search, but it always first looks in the directory
	 * where the current process was started, which behavior
	 * is undesirable for backward compatibility.  So we
	 * jump through our own hoops by picking out the path
	 * we really want it to use. */
	if (!fullcmd) {
	    fullcmd = qualified_path(cname);
	    if (fullcmd) {
		if (cname != cmdname)
		    Safefree(cname);
		cname = fullcmd;
		DEBUG_p(PerlIO_printf(Perl_debug_log,
				      "Retrying [%s] with same args\n",
				      cname));
		goto RETRY;
	    }
	}
	errno = ENOENT;
	ret = -1;
	goto RETVAL;
    }

    if (mode == P_NOWAIT) {
	/* asynchronous spawn -- store handle, return PID */
	ret = (int)ProcessInformation.dwProcessId;
	if (IsWin95() && ret < 0)
	    ret = -ret;

	w32_child_handles[w32_num_children] = ProcessInformation.hProcess;
	w32_child_pids[w32_num_children] = (DWORD)ret;
	++w32_num_children;
    }
    else  {
	DWORD status;
	win32_msgwait(aTHX_ 1, &ProcessInformation.hProcess, INFINITE, NULL);
	/* FIXME: if msgwait returned due to message perhaps forward the
	   "signal" to the process
         */
	GetExitCodeProcess(ProcessInformation.hProcess, &status);
	ret = (int)status;
	CloseHandle(ProcessInformation.hProcess);
    }

    CloseHandle(ProcessInformation.hThread);

RETVAL:
    PerlEnv_free_childenv(env);
    PerlEnv_free_childdir(dir);
    Safefree(cmd);
    if (cname != cmdname)
	Safefree(cname);
    return ret;
#endif
}

DllExport int
win32_execv(const char *cmdname, const char *const *argv)
{
#ifdef USE_ITHREADS
    dTHX;
    /* if this is a pseudo-forked child, we just want to spawn
     * the new program, and return */
    if (w32_pseudo_id)
#  ifdef __BORLANDC__
	return spawnv(P_WAIT, cmdname, (char *const *)argv);
#  else
	return spawnv(P_WAIT, cmdname, argv);
#  endif
#endif
#ifdef __BORLANDC__
    return execv(cmdname, (char *const *)argv);
#else
    return execv(cmdname, argv);
#endif
}

DllExport int
win32_execvp(const char *cmdname, const char *const *argv)
{
#ifdef USE_ITHREADS
    dTHX;
    /* if this is a pseudo-forked child, we just want to spawn
     * the new program, and return */
    if (w32_pseudo_id) {
	int status = win32_spawnvp(P_WAIT, cmdname, (const char *const *)argv);
	if (status != -1) {
	    my_exit(status);
	    return 0;
	}
	else
	    return status;
    }
#endif
#ifdef __BORLANDC__
    return execvp(cmdname, (char *const *)argv);
#else
    return execvp(cmdname, argv);
#endif
}

DllExport void
win32_perror(const char *str)
{
    perror(str);
}

DllExport void
win32_setbuf(FILE *pf, char *buf)
{
    setbuf(pf, buf);
}

DllExport int
win32_setvbuf(FILE *pf, char *buf, int type, size_t size)
{
    return setvbuf(pf, buf, type, size);
}

DllExport int
win32_flushall(void)
{
    return flushall();
}

DllExport int
win32_fcloseall(void)
{
    return fcloseall();
}

DllExport char*
win32_fgets(char *s, int n, FILE *pf)
{
    return fgets(s, n, pf);
}

DllExport char*
win32_gets(char *s)
{
    return gets(s);
}

DllExport int
win32_fgetc(FILE *pf)
{
    return fgetc(pf);
}

DllExport int
win32_putc(int c, FILE *pf)
{
    return putc(c,pf);
}

DllExport int
win32_puts(const char *s)
{
    return puts(s);
}

DllExport int
win32_getchar(void)
{
    return getchar();
}

DllExport int
win32_putchar(int c)
{
    return putchar(c);
}

#ifdef MYMALLOC

#ifndef USE_PERL_SBRK

static char *committed = NULL;		/* XXX threadead */
static char *base      = NULL;		/* XXX threadead */
static char *reserved  = NULL;		/* XXX threadead */
static char *brk       = NULL;		/* XXX threadead */
static DWORD pagesize  = 0;		/* XXX threadead */

void *
sbrk(ptrdiff_t need)
{
 void *result;
 if (!pagesize)
  {SYSTEM_INFO info;
   GetSystemInfo(&info);
   /* Pretend page size is larger so we don't perpetually
    * call the OS to commit just one page ...
    */
   pagesize = info.dwPageSize << 3;
  }
 if (brk+need >= reserved)
  {
   DWORD size = brk+need-reserved;
   char *addr;
   char *prev_committed = NULL;
   if (committed && reserved && committed < reserved)
    {
     /* Commit last of previous chunk cannot span allocations */
     addr = (char *) VirtualAlloc(committed,reserved-committed,MEM_COMMIT,PAGE_READWRITE);
     if (addr)
      {
      /* Remember where we committed from in case we want to decommit later */
      prev_committed = committed;
      committed = reserved;
      }
    }
   /* Reserve some (more) space
    * Contiguous blocks give us greater efficiency, so reserve big blocks -
    * this is only address space not memory...
    * Note this is a little sneaky, 1st call passes NULL as reserved
    * so lets system choose where we start, subsequent calls pass
    * the old end address so ask for a contiguous block
    */
sbrk_reserve:
   if (size < 64*1024*1024)
    size = 64*1024*1024;
   size = ((size + pagesize - 1) / pagesize) * pagesize;
   addr  = (char *) VirtualAlloc(reserved,size,MEM_RESERVE,PAGE_NOACCESS);
   if (addr)
    {
     reserved = addr+size;
     if (!base)
      base = addr;
     if (!committed)
      committed = base;
     if (!brk)
      brk = committed;
    }
   else if (reserved)
    {
      /* The existing block could not be extended far enough, so decommit
       * anything that was just committed above and start anew */
      if (prev_committed)
       {
       if (!VirtualFree(prev_committed,reserved-prev_committed,MEM_DECOMMIT))
        return (void *) -1;
       }
      reserved = base = committed = brk = NULL;
      size = need;
      goto sbrk_reserve;
    }
   else
    {
     return (void *) -1;
    }
  }
 result = brk;
 brk += need;
 if (brk > committed)
  {
   DWORD size = ((brk-committed + pagesize -1)/pagesize) * pagesize;
   char *addr;
   if (committed+size > reserved)
    size = reserved-committed;
   addr = (char *) VirtualAlloc(committed,size,MEM_COMMIT,PAGE_READWRITE);
   if (addr)
    committed += size;
   else
    return (void *) -1;
  }
 return result;
}

#endif
#endif

DllExport void*
win32_malloc(size_t size)
{
    return malloc(size);
}

DllExport void*
win32_calloc(size_t numitems, size_t size)
{
    return calloc(numitems,size);
}

DllExport void*
win32_realloc(void *block, size_t size)
{
    return realloc(block,size);
}

DllExport void
win32_free(void *block)
{
    free(block);
}


DllExport int
win32_open_osfhandle(intptr_t handle, int flags)
{
#ifdef USE_FIXED_OSFHANDLE
    if (IsWin95())
	return my_open_osfhandle(handle, flags);
#endif
    return _open_osfhandle(handle, flags);
}

DllExport intptr_t
win32_get_osfhandle(int fd)
{
    return (intptr_t)_get_osfhandle(fd);
}

DllExport FILE *
win32_fdupopen(FILE *pf)
{
    FILE* pfdup;
    fpos_t pos;
    char mode[3];
    int fileno = win32_dup(win32_fileno(pf));

    /* open the file in the same mode */
#ifdef __BORLANDC__
    if((pf)->flags & _F_READ) {
	mode[0] = 'r';
	mode[1] = 0;
    }
    else if((pf)->flags & _F_WRIT) {
	mode[0] = 'a';
	mode[1] = 0;
    }
    else if((pf)->flags & _F_RDWR) {
	mode[0] = 'r';
	mode[1] = '+';
	mode[2] = 0;
    }
#else
    if((pf)->_flag & _IOREAD) {
	mode[0] = 'r';
	mode[1] = 0;
    }
    else if((pf)->_flag & _IOWRT) {
	mode[0] = 'a';
	mode[1] = 0;
    }
    else if((pf)->_flag & _IORW) {
	mode[0] = 'r';
	mode[1] = '+';
	mode[2] = 0;
    }
#endif

    /* it appears that the binmode is attached to the
     * file descriptor so binmode files will be handled
     * correctly
     */
    pfdup = win32_fdopen(fileno, mode);

    /* move the file pointer to the same position */
    if (!fgetpos(pf, &pos)) {
	fsetpos(pfdup, &pos);
    }
    return pfdup;
}

DllExport void*
win32_dynaload(const char* filename)
{
    dTHX;
    char buf[MAX_PATH+1];
    char *first;

    /* LoadLibrary() doesn't recognize forward slashes correctly,
     * so turn 'em back. */
    first = strchr(filename, '/');
    if (first) {
	STRLEN len = strlen(filename);
	if (len <= MAX_PATH) {
	    strcpy(buf, filename);
	    filename = &buf[first - filename];
	    while (*filename) {
		if (*filename == '/')
		    *(char*)filename = '\\';
		++filename;
	    }
	    filename = buf;
	}
    }
    return LoadLibraryExA(PerlDir_mapA(filename), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
}

XS(w32_SetChildShowWindow)
{
    dXSARGS;
    BOOL use_showwindow = w32_use_showwindow;
    /* use "unsigned short" because Perl has redefined "WORD" */
    unsigned short showwindow = w32_showwindow;

    if (items > 1)
	Perl_croak(aTHX_ "usage: Win32::SetChildShowWindow($showwindow)");

    if (items == 0 || !SvOK(ST(0)))
        w32_use_showwindow = FALSE;
    else {
        w32_use_showwindow = TRUE;
        w32_showwindow = (unsigned short)SvIV(ST(0));
    }

    EXTEND(SP, 1);
    if (use_showwindow)
        ST(0) = sv_2mortal(newSViv(showwindow));
    else
        ST(0) = &PL_sv_undef;
    XSRETURN(1);
}

void
Perl_init_os_extras(void)
{
    dTHX;
    char *file = __FILE__;

    /* Initialize Win32CORE if it has been statically linked. */
    void (*pfn_init)(pTHX);
#if defined(__BORLANDC__)
    /* makedef.pl seems to have given up on fixing this issue in the .def file */
    pfn_init = (void (*)(pTHX))GetProcAddress((HMODULE)w32_perldll_handle, "_init_Win32CORE");
#else
    pfn_init = (void (*)(pTHX))GetProcAddress((HMODULE)w32_perldll_handle, "init_Win32CORE");
#endif
    if (pfn_init)
        pfn_init(aTHX);

    newXS("Win32::SetChildShowWindow", w32_SetChildShowWindow, file);
}

void *
win32_signal_context(void)
{
    dTHX;
#ifdef MULTIPLICITY
    if (!my_perl) {
	my_perl = PL_curinterp;
	PERL_SET_THX(my_perl);
    }
    return my_perl;
#else
    return PL_curinterp;
#endif
}


BOOL WINAPI
win32_ctrlhandler(DWORD dwCtrlType)
{
#ifdef MULTIPLICITY
    dTHXa(PERL_GET_SIG_CONTEXT);

    if (!my_perl)
	return FALSE;
#endif

    switch(dwCtrlType) {
    case CTRL_CLOSE_EVENT:
     /*  A signal that the system sends to all processes attached to a console when
         the user closes the console (either by choosing the Close command from the
         console window's System menu, or by choosing the End Task command from the
         Task List
      */
	if (do_raise(aTHX_ 1))	      /* SIGHUP */
	    sig_terminate(aTHX_ 1);
	return TRUE;

    case CTRL_C_EVENT:
	/*  A CTRL+c signal was received */
	if (do_raise(aTHX_ SIGINT))
	    sig_terminate(aTHX_ SIGINT);
	return TRUE;

    case CTRL_BREAK_EVENT:
	/*  A CTRL+BREAK signal was received */
	if (do_raise(aTHX_ SIGBREAK))
	    sig_terminate(aTHX_ SIGBREAK);
	return TRUE;

    case CTRL_LOGOFF_EVENT:
      /*  A signal that the system sends to all console processes when a user is logging
          off. This signal does not indicate which user is logging off, so no
          assumptions can be made.
       */
	break;
    case CTRL_SHUTDOWN_EVENT:
      /*  A signal that the system sends to all console processes when the system is
          shutting down.
       */
	if (do_raise(aTHX_ SIGTERM))
	    sig_terminate(aTHX_ SIGTERM);
	return TRUE;
    default:
	break;
    }
    return FALSE;
}


#ifdef SET_INVALID_PARAMETER_HANDLER
#  include <crtdbg.h>
#endif

static void
ansify_path(void)
{
    size_t len;
    char *ansi_path;
    WCHAR *wide_path;
    WCHAR *wide_dir;

    /* win32_ansipath() requires Windows 2000 or later */
    if (!IsWin2000())
        return;

    /* fetch Unicode version of PATH */
    len = 2000;
    wide_path = win32_malloc(len*sizeof(WCHAR));
    while (wide_path) {
        size_t newlen = GetEnvironmentVariableW(L"PATH", wide_path, len);
        if (newlen < len)
            break;
        len = newlen;
        wide_path = win32_realloc(wide_path, len*sizeof(WCHAR));
    }
    if (!wide_path)
        return;

    /* convert to ANSI pathnames */
    wide_dir = wide_path;
    ansi_path = NULL;
    while (wide_dir) {
        WCHAR *sep = wcschr(wide_dir, ';');
        char *ansi_dir;
        size_t ansi_len;
        size_t wide_len;

        if (sep)
            *sep++ = '\0';

        /* remove quotes around pathname */
        if (*wide_dir == '"')
            ++wide_dir;
        wide_len = wcslen(wide_dir);
        if (wide_len && wide_dir[wide_len-1] == '"')
            wide_dir[wide_len-1] = '\0';

        /* append ansi_dir to ansi_path */
        ansi_dir = win32_ansipath(wide_dir);
        ansi_len = strlen(ansi_dir);
        if (ansi_path) {
            size_t newlen = len + 1 + ansi_len;
            ansi_path = win32_realloc(ansi_path, newlen+1);
            if (!ansi_path)
                break;
            ansi_path[len] = ';';
            memcpy(ansi_path+len+1, ansi_dir, ansi_len+1);
            len = newlen;
        }
        else {
            len = ansi_len;
            ansi_path = win32_malloc(5+len+1);
            if (!ansi_path)
                break;
            memcpy(ansi_path, "PATH=", 5);
            memcpy(ansi_path+5, ansi_dir, len+1);
            len += 5;
        }
        win32_free(ansi_dir);
        wide_dir = sep;
    }

    if (ansi_path) {
        /* Update C RTL environ array.  This will only have full effect if
         * perl_parse() is later called with `environ` as the `env` argument.
         * Otherwise S_init_postdump_symbols() will overwrite PATH again.
         *
         * We do have to ansify() the PATH before Perl has been fully
         * initialized because S_find_script() uses the PATH when perl
         * is being invoked with the -S option.  This happens before %ENV
         * is initialized in S_init_postdump_symbols().
         *
         * XXX Is this a bug? Should S_find_script() use the environment
         * XXX passed in the `env` arg to parse_perl()?
         */
        putenv(ansi_path);
        /* Keep system environment in sync because S_init_postdump_symbols()
         * will not call mg_set() if it initializes %ENV from `environ`.
         */
        SetEnvironmentVariableA("PATH", ansi_path+5);
        /* We are intentionally leaking the ansi_path string here because
         * the Borland runtime library puts it directly into the environ
         * array.  The Microsoft runtime library seems to make a copy,
         * but will leak the copy should it be replaced again later.
         * Since this code is only called once during PERL_SYS_INIT this
         * shouldn't really matter.
         */
    }
    win32_free(wide_path);
}

void
Perl_win32_init(int *argcp, char ***argvp)
{
    HMODULE module;

#ifdef SET_INVALID_PARAMETER_HANDLER
    _invalid_parameter_handler oldHandler, newHandler;
    newHandler = my_invalid_parameter_handler;
    oldHandler = _set_invalid_parameter_handler(newHandler);
    _CrtSetReportMode(_CRT_ASSERT, 0);
#endif
    /* Disable floating point errors, Perl will trap the ones we
     * care about.  VC++ RTL defaults to switching these off
     * already, but the Borland RTL doesn't.  Since we don't
     * want to be at the vendor's whim on the default, we set
     * it explicitly here.
     */
#if !defined(_ALPHA_) && !defined(__GNUC__)
    _control87(MCW_EM, MCW_EM);
#endif
    MALLOC_INIT;

    module = GetModuleHandle("ntdll.dll");
    if (module) {
        *(FARPROC*)&pfnZwQuerySystemInformation = GetProcAddress(module, "ZwQuerySystemInformation");
    }

    module = GetModuleHandle("kernel32.dll");
    if (module) {
        *(FARPROC*)&pfnCreateToolhelp32Snapshot = GetProcAddress(module, "CreateToolhelp32Snapshot");
        *(FARPROC*)&pfnProcess32First           = GetProcAddress(module, "Process32First");
        *(FARPROC*)&pfnProcess32Next            = GetProcAddress(module, "Process32Next");
    }

    g_osver.dwOSVersionInfoSize = sizeof(g_osver);
    GetVersionEx(&g_osver);

    ansify_path();
}

void
Perl_win32_term(void)
{
    dTHX;
    HINTS_REFCNT_TERM;
    OP_REFCNT_TERM;
    PERLIO_TERM;
    MALLOC_TERM;
}

void
win32_get_child_IO(child_IO_table* ptbl)
{
    ptbl->childStdIn	= GetStdHandle(STD_INPUT_HANDLE);
    ptbl->childStdOut	= GetStdHandle(STD_OUTPUT_HANDLE);
    ptbl->childStdErr	= GetStdHandle(STD_ERROR_HANDLE);
}

Sighandler_t
win32_signal(int sig, Sighandler_t subcode)
{
    dTHX;
    if (sig < SIG_SIZE) {
	int save_errno = errno;
	Sighandler_t result = signal(sig, subcode);
	if (result == SIG_ERR) {
	    result = w32_sighandler[sig];
	    errno = save_errno;
	}
	w32_sighandler[sig] = subcode;
	return result;
    }
    else {
	errno = EINVAL;
	return SIG_ERR;
    }
}

/* The PerlMessageWindowClass's WindowProc */
LRESULT CALLBACK
win32_message_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return win32_process_message(hwnd, msg, wParam, lParam) ?
        0 : DefWindowProc(hwnd, msg, wParam, lParam);
}

/* we use a message filter hook to process thread messages, passing any
 * messages that we don't process on to the rest of the hook chain
 * Anyone else writing a message loop that wants to play nicely with perl
 * should do
 *   CallMsgFilter(&msg, MSGF_***);
 * between their GetMessage and DispatchMessage calls.  */
LRESULT CALLBACK
win32_message_filter_proc(int code, WPARAM wParam, LPARAM lParam) {
    LPMSG pmsg = (LPMSG)lParam;

    /* we'll process it if code says we're allowed, and it's a thread message */
    if (code >= 0 && pmsg->hwnd == NULL
            && win32_process_message(pmsg->hwnd, pmsg->message,
                                     pmsg->wParam, pmsg->lParam))
    {
            return TRUE;
    }

    /* XXX: MSDN says that hhk is ignored, but we should really use the
     * return value from SetWindowsHookEx() in win32_create_message_window().  */
    return CallNextHookEx(NULL, code, wParam, lParam);
}

/* The real message handler. Can be called with
 * hwnd == NULL to process our thread messages. Returns TRUE for any messages
 * that it processes */
static LRESULT
win32_process_message(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    /* BEWARE. The context retrieved using dTHX; is the context of the
     * 'parent' thread during the CreateWindow() phase - i.e. for all messages
     * up to and including WM_CREATE.  If it ever happens that you need the
     * 'child' context before this, then it needs to be passed into
     * win32_create_message_window(), and passed to the WM_NCCREATE handler
     * from the lparam of CreateWindow().  It could then be stored/retrieved
     * using [GS]etWindowLongPtr(... GWLP_USERDATA ...), possibly eliminating
     * the dTHX calls here. */
    /* XXX For now it is assumed that the overhead of the dTHX; for what
     * are relativley infrequent code-paths, is better than the added
     * complexity of getting the correct context passed into
     * win32_create_message_window() */

    switch(msg) {

#ifdef USE_ITHREADS
        case WM_USER_MESSAGE: {
            long child = find_pseudo_pid((int)wParam);
            if (child >= 0) {
                dTHX;
                w32_pseudo_child_message_hwnds[child] = (HWND)lParam;
                return 1;
            }
            break;
        }
#endif

        case WM_USER_KILL: {
            dTHX;
            /* We use WM_USER_KILL to fake kill() with other signals */
            int sig = (int)wParam;
            if (do_raise(aTHX_ sig))
                sig_terminate(aTHX_ sig);

            return 1;
        }

        case WM_TIMER: {
            dTHX;
            /* alarm() is a one-shot but SetTimer() repeats so kill it */
            if (w32_timerid && w32_timerid==(UINT)wParam) {
                KillTimer(w32_message_hwnd, w32_timerid);
                w32_timerid=0;

                /* Now fake a call to signal handler */
                if (do_raise(aTHX_ 14))
                    sig_terminate(aTHX_ 14);

                return 1;
            }
            break;
        }

        default:
            break;

    } /* switch */

    /* Above or other stuff may have set a signal flag, and we may not have
     * been called from win32_async_check() (e.g. some other GUI's message
     * loop.  BUT DON'T dispatch signals here: If someone has set a SIGALRM
     * handler that die's, and the message loop that calls here is wrapped
     * in an eval, then you may well end up with orphaned windows - signals
     * are dispatched by win32_async_check() */

    return 0;
}

void
win32_create_message_window_class(void)
{
    /* create the window class for "message only" windows */
    WNDCLASS wc;

    Zero(&wc, 1, wc);
    wc.lpfnWndProc = win32_message_window_proc;
    wc.hInstance = (HINSTANCE)GetModuleHandle(NULL);
    wc.lpszClassName = "PerlMessageWindowClass";

    /* second and subsequent calls will fail, but class
     * will already be registered */
    RegisterClass(&wc);
}

HWND
win32_create_message_window(void)
{
    HWND hwnd = NULL;

    /* "message-only" windows have been implemented in Windows 2000 and later.
     * On earlier versions we'll continue to post messages to a specific
     * thread and use hwnd==NULL.  This is brittle when either an embedding
     * application or an XS module is also posting messages to hwnd=NULL
     * because once removed from the queue they cannot be delivered to the
     * "right" place with DispatchMessage() anymore, as there is no WindowProc
     * if there is no window handle.
     */
    /* Using HWND_MESSAGE appears to work under Win98, despite MSDN
     * documentation to the contrary, however, there is some evidence that
     * there may be problems with the implementation on Win98. As it is not
     * officially supported we take the cautious route and stick with thread
     * messages (hwnd == NULL) on platforms prior to Win2k.
     */
    if (IsWin2000()) {
        win32_create_message_window_class();

        hwnd = CreateWindow("PerlMessageWindowClass", "PerlMessageWindow",
                0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
    }

    /* If we din't create a window for any reason, then we'll use thread
     * messages for our signalling, so we install a hook which
     * is called by CallMsgFilter in win32_async_check(), or any other
     * modal loop (e.g. Win32::MsgBox or any other GUI extention, or anything
     * that use OLE, etc. */
    if(!hwnd) {
        SetWindowsHookEx(WH_MSGFILTER, win32_message_filter_proc,
                NULL, GetCurrentThreadId());
    }
  
    return hwnd;
}

#ifdef HAVE_INTERP_INTERN

static void
win32_csighandler(int sig)
{
#if 0
    dTHXa(PERL_GET_SIG_CONTEXT);
    Perl_warn(aTHX_ "Got signal %d",sig);
#endif
    /* Does nothing */
}

#if defined(__MINGW32__) && defined(__cplusplus)
#define CAST_HWND__(x) (HWND__*)(x)
#else
#define CAST_HWND__(x) x
#endif

void
Perl_sys_intern_init(pTHX)
{
    int i;

    w32_perlshell_tokens	= NULL;
    w32_perlshell_vec		= (char**)NULL;
    w32_perlshell_items		= 0;
    w32_fdpid			= newAV();
    Newx(w32_children, 1, child_tab);
    w32_num_children		= 0;
#  ifdef USE_ITHREADS
    w32_pseudo_id		= 0;
    Newx(w32_pseudo_children, 1, pseudo_child_tab);
    w32_num_pseudo_children	= 0;
#  endif
    w32_timerid                 = 0;
    w32_message_hwnd            = CAST_HWND__(INVALID_HANDLE_VALUE);
    w32_poll_count              = 0;
    for (i=0; i < SIG_SIZE; i++) {
    	w32_sighandler[i] = SIG_DFL;
    }
#  ifdef MULTIPLICITY
    if (my_perl == PL_curinterp) {
#  else
    {
#  endif
	/* Force C runtime signal stuff to set its console handler */
	signal(SIGINT,win32_csighandler);
	signal(SIGBREAK,win32_csighandler);

        /* We spawn asynchronous processes with the CREATE_NEW_PROCESS_GROUP
         * flag.  This has the side-effect of disabling Ctrl-C events in all
         * processes in this group.  At least on Windows NT and later we
         * can re-enable Ctrl-C handling by calling SetConsoleCtrlHandler()
         * with a NULL handler.  This is not valid on Windows 9X.
         */
        if (IsWinNT())
            SetConsoleCtrlHandler(NULL,FALSE);

	/* Push our handler on top */
	SetConsoleCtrlHandler(win32_ctrlhandler,TRUE);
    }
}

void
Perl_sys_intern_clear(pTHX)
{
    Safefree(w32_perlshell_tokens);
    Safefree(w32_perlshell_vec);
    /* NOTE: w32_fdpid is freed by sv_clean_all() */
    Safefree(w32_children);
    if (w32_timerid) {
    	KillTimer(w32_message_hwnd, w32_timerid);
    	w32_timerid = 0;
    }
    if (w32_message_hwnd != NULL && w32_message_hwnd != INVALID_HANDLE_VALUE)
        DestroyWindow(w32_message_hwnd);
#  ifdef MULTIPLICITY
    if (my_perl == PL_curinterp) {
#  else
    {
#  endif
	SetConsoleCtrlHandler(win32_ctrlhandler,FALSE);
    }
#  ifdef USE_ITHREADS
    Safefree(w32_pseudo_children);
#  endif
}

#  ifdef USE_ITHREADS

void
Perl_sys_intern_dup(pTHX_ struct interp_intern *src, struct interp_intern *dst)
{
    PERL_ARGS_ASSERT_SYS_INTERN_DUP;

    dst->perlshell_tokens	= NULL;
    dst->perlshell_vec		= (char**)NULL;
    dst->perlshell_items	= 0;
    dst->fdpid			= newAV();
    Newxz(dst->children, 1, child_tab);
    dst->pseudo_id		= 0;
    Newxz(dst->pseudo_children, 1, pseudo_child_tab);
    dst->timerid                = 0;
    dst->message_hwnd		= CAST_HWND__(INVALID_HANDLE_VALUE);
    dst->poll_count             = 0;
    Copy(src->sigtable,dst->sigtable,SIG_SIZE,Sighandler_t);
}
#  endif /* USE_ITHREADS */
#endif /* HAVE_INTERP_INTERN */
