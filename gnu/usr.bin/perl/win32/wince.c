/*  WINCE.C - stuff for Windows CE
 *
 *  Time-stamp: <26/10/01 15:25:20 keuchel@keuchelnt>
 *
 *  You may distribute under the terms of either the GNU General Public
 *  License or the Artistic License, as specified in the README file.
 */

#define WIN32_LEAN_AND_MEAN
#define WIN32IO_IS_STDIO
#include <windows.h>
#include <signal.h>

#define PERLIO_NOT_STDIO 0

#if !defined(PERLIO_IS_STDIO) && !defined(USE_SFIO)
#define PerlIO FILE
#endif

#define wince_private
#include "errno.h"

#include "EXTERN.h"
#include "perl.h"

#define NO_XSLOCKS
#define PERL_NO_GET_CONTEXT
#include "XSUB.h"

#include "win32iop.h"
#include <string.h>
#include <stdarg.h>
#include <float.h>
#include <shellapi.h>
#include <process.h>

#define perl
#include "celib_defs.h"
#include "cewin32.h"
#include "cecrt.h"
#include "cewin32_defs.h"
#include "cecrt_defs.h"

#define GetCurrentDirectoryW XCEGetCurrentDirectoryW

#ifdef PALM_SIZE
#include "stdio-palmsize.h"
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
#  undef do_spawn
#  define do_spawn g_do_spawn
#  undef getlogin
#  define getlogin g_getlogin
#endif

static void		get_shell(void);
static long		tokenize(const char *str, char **dest, char ***destv);
static int		do_spawn2(pTHX_ char *cmd, int exectype);
static BOOL		has_shell_metachars(char *ptr);
static long		filetime_to_clock(PFILETIME ft);
static BOOL		filetime_from_time(PFILETIME ft, time_t t);
static char *		get_emd_part(SV **leading, char *trailing, ...);
static void		remove_dead_process(long deceased);
static long		find_pid(int pid);
static char *		qualified_path(const char *cmd);
static char *		win32_get_xlib(const char *pl, const char *xlib,
				       const char *libname);

#ifdef USE_ITHREADS
static void		remove_dead_pseudo_process(long child);
static long		find_pseudo_pid(int pid);
#endif

int _fmode = O_TEXT; /* celib do not provide _fmode, so we define it here */

START_EXTERN_C
HANDLE	w32_perldll_handle = INVALID_HANDLE_VALUE;
char	w32_module_name[MAX_PATH+1];
END_EXTERN_C

static DWORD	w32_platform = (DWORD)-1;

int
IsWin95(void)
{
  return (win32_os_id() == VER_PLATFORM_WIN32_WINDOWS);
}

int
IsWinNT(void)
{
  return (win32_os_id() == VER_PLATFORM_WIN32_NT);
}

int
IsWinCE(void)
{
  return (win32_os_id() == VER_PLATFORM_WIN32_CE);
}

EXTERN_C void
set_w32_module_name(void)
{
  char* ptr;
  XCEGetModuleFileNameA((HMODULE)((w32_perldll_handle == INVALID_HANDLE_VALUE)
				  ? XCEGetModuleHandleA(NULL)
				  : w32_perldll_handle),
			w32_module_name, sizeof(w32_module_name));

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

    retval = XCERegOpenKeyExA(hkey, subkey, 0, KEY_READ, &handle);
    if (retval == ERROR_SUCCESS) {
	DWORD datalen;
	retval = XCERegQueryValueExA(handle, valuename, 0, &type, NULL, &datalen);
	if (retval == ERROR_SUCCESS && type == REG_SZ) {
	    dTHX;
	    if (!*svp)
		*svp = sv_2mortal(newSVpvn("",0));
	    SvGROW(*svp, datalen);
	    retval = XCERegQueryValueExA(handle, valuename, 0, NULL,
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
    int oldsize, newsize;
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
    if (XCEGetFileAttributesA(mod_name) != (DWORD) -1) {
	/* directory exists */
	dTHX;
	if (!*prev_pathp)
	    *prev_pathp = sv_2mortal(newSVpvn("",0));
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
    DWORD datalen;
    int len, newsize;
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
has_shell_metachars(char *ptr)
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
  printf("popen(%s)\n", cmd);

  Perl_croak(aTHX_ PL_no_func, "popen");
  return NULL;
}

long
Perl_my_pclose(pTHX_ PerlIO *fp)
{
  Perl_croak(aTHX_ PL_no_func, "pclose");
  return -1;
}
#endif

DllExport unsigned long
win32_os_id(void)
{
    static OSVERSIONINFOA osver;

    if (osver.dwPlatformId != w32_platform) {
	memset(&osver, 0, sizeof(OSVERSIONINFOA));
	osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
	XCEGetVersionExA(&osver);
	w32_platform = osver.dwPlatformId;
    }
    return (unsigned long)w32_platform;
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
    pid = xcegetpid();
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

DllExport int
win32_pipe(int *pfd, unsigned int size, int mode)
{
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "pipe");
  return -1;
}

DllExport int
win32_times(struct tms *timebuf)
{
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "times");
  return -1;
}

Sighandler_t
win32_signal(int sig, Sighandler_t subcode)
{
  return xcesignal(sig, subcode);
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
  PERL_ARGS_ASSERT_DO_ASPAWN;

  Perl_croak(aTHX_ PL_no_func, "aspawn");
  return -1;
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

#if 1
static int
do_spawn2(pTHX_ char *cmd, int exectype)
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
	argv[i++] = cmd;
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
	if (IsWin95())
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
    HANDLE		fh;
    char		buffer[MAX_PATH*2];
    WCHAR		wbuffer[MAX_PATH+1];
    char*		ptr;

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
    fh = FindFirstFile(PerlDir_mapA(scanname), &aFindData);
    dirp->handle = fh;
    if (fh == INVALID_HANDLE_VALUE) {
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

    /* now allocate the first part of the string table for
     * the filenames that we find.
     */
    ptr = aFindData.cFileName;
    idx = strlen(ptr)+1;
    if (idx < 256)
	dirp->size = 128;
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
	    char*		ptr;
	    BOOL		res;
	    WIN32_FIND_DATAW	wFindData;
	    WIN32_FIND_DATAA	aFindData;
	    char		buffer[MAX_PATH*2];

	    /* finding the next file that matches the wildcard
	     * (which should be all of them in this directory!).
	     */
            res = FindNextFile(dirp->handle, &aFindData);
	    if (res)
	        ptr = aFindData.cFileName;
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

#else
/////!!!!!!!!!!! return here and do right stuff!!!!

DllExport DIR *
win32_opendir(const char *filename)
{
  return opendir(filename);
}

DllExport struct direct *
win32_readdir(DIR *dirp)
{
  return readdir(dirp);
}

DllExport long
win32_telldir(DIR *dirp)
{
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "telldir");
  return -1;
}

DllExport void
win32_seekdir(DIR *dirp, long loc)
{
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "seekdir");
}

DllExport void
win32_rewinddir(DIR *dirp)
{
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "rewinddir");
}

DllExport int
win32_closedir(DIR *dirp)
{
  closedir(dirp);
  return 0;
}
#endif   // 1

DllExport int
win32_kill(int pid, int sig)
{
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "kill");
  return -1;
}

DllExport int
win32_stat(const char *path, struct stat *sbuf)
{
  return xcestat(path, sbuf);
}

DllExport char *
win32_longpath(char *path)
{
  return path;
}

DllExport char *
win32_getenv(const char *name)
{
  return xcegetenv(name);
}

DllExport int
win32_putenv(const char *name)
{
  return xceputenv(name);
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
  return xceunlink(filename);
}

DllExport int
win32_utime(const char *filename, struct utimbuf *times)
{
  return xceutime(filename, (struct _utimbuf *) times);
}

DllExport int
win32_gettimeofday(struct timeval *tp, void *not_used)
{
    return xcegettimeofday(tp,not_used);
}

DllExport int
win32_uname(struct utsname *name)
{
    struct hostent *hep;
    STRLEN nodemax = sizeof(name->nodename)-1;
    OSVERSIONINFOA osver;

    memset(&osver, 0, sizeof(OSVERSIONINFOA));
    osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    if (XCEGetVersionExA(&osver)) {
	/* sysname */
	switch (osver.dwPlatformId) {
	case VER_PLATFORM_WIN32_CE:
	    strcpy(name->sysname, "Windows CE");
	    break;
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
		osver.dwMajorVersion, osver.dwMinorVersion);

	/* version */
	sprintf(name->version, "Build %d",
		osver.dwPlatformId == VER_PLATFORM_WIN32_NT
		? osver.dwBuildNumber : (osver.dwBuildNumber & 0xffff));
	if (osver.szCSDVersion[0]) {
	    char *buf = name->version + strlen(name->version);
	    sprintf(buf, " (%s)", osver.szCSDVersion);
	}
    }
    else {
	*name->sysname = '\0';
	*name->version = '\0';
	*name->release = '\0';
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
	if (!XCEGetComputerNameA(name->nodename, &sz))
	    *name->nodename = '\0';
    }

    /* machine (architecture) */
    {
	SYSTEM_INFO info;
	char *arch;
	GetSystemInfo(&info);

	switch (info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL:
	    arch = "x86"; break;
	case PROCESSOR_ARCHITECTURE_MIPS:
	    arch = "mips"; break;
	case PROCESSOR_ARCHITECTURE_ALPHA:
	    arch = "alpha"; break;
	case PROCESSOR_ARCHITECTURE_PPC:
	    arch = "ppc"; break;
	case PROCESSOR_ARCHITECTURE_ARM:
	    arch = "arm"; break;
	case PROCESSOR_HITACHI_SH3:
	    arch = "sh3"; break;
	case PROCESSOR_SHx_SH3:
	    arch = "sh3"; break;

	default:
	    arch = "unknown"; break;
	}
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
    int ours = 1;
    /* Passing PeekMessage -1 as HWND (2nd arg) only get PostThreadMessage() messages
     * and ignores window messages - should co-exist better with windows apps e.g. Tk
     */
    while (PeekMessage(&msg, (HWND)-1, 0, 0, PM_REMOVE|PM_NOYIELD)) {
	int sig;
	switch(msg.message) {

#if 0
    /* Perhaps some other messages could map to signals ? ... */
        case WM_CLOSE:
        case WM_QUIT:
	    /* Treat WM_QUIT like SIGHUP?  */
	    sig = SIGHUP;
	    goto Raise;
	    break;
#endif

	/* We use WM_USER to fake kill() with other signals */
	case WM_USER: {
	    sig = msg.wParam;
	Raise:
	    if (do_raise(aTHX_ sig)) {
		   sig_terminate(aTHX_ sig);
	    }
	    break;
	}

	case WM_TIMER: {
	    /* alarm() is a one-shot but SetTimer() repeats so kill it */
	    if (w32_timerid) {
	    	KillTimer(NULL,w32_timerid);
	    	w32_timerid=0;
	    }
	    /* Now fake a call to signal handler */
	    if (do_raise(aTHX_ 14)) {
	    	sig_terminate(aTHX_ 14);
	    }
	    break;
	}

	/* Otherwise do normal Win32 thing - in case it is useful */
	default:
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	    ours = 0;
	    break;
	}
    }
    w32_poll_count = 0;

    /* Above or other stuff may have set a signal flag */
    if (PL_sig_pending) {
	despatch_signals();
    }
    return ours;
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
	DWORD result = MsgWaitForMultipleObjects(count,handles,FALSE,timeout-ticks, QS_ALLEVENTS);
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

static UINT timerid = 0;

static VOID CALLBACK TimerProc(HWND win, UINT msg, UINT id, DWORD time)
{
    dTHX;
    KillTimer(NULL,timerid);
    timerid=0;
    sighandler(14);
}

DllExport unsigned int
win32_sleep(unsigned int t)
{
  return xcesleep(t);
}

DllExport unsigned int
win32_alarm(unsigned int sec)
{
    /*
     * the 'obvious' implentation is SetTimer() with a callback
     * which does whatever receiving SIGALRM would do
     * we cannot use SIGALRM even via raise() as it is not
     * one of the supported codes in <signal.h>
     *
     * Snag is unless something is looking at the message queue
     * nothing happens :-(
     */
    dTHX;
    if (sec)
     {
      timerid = SetTimer(NULL,timerid,sec*1000,(TIMERPROC)TimerProc);
      if (!timerid)
       Perl_croak_nocontext("Cannot set timer");
     }
    else
     {
      if (timerid)
       {
        KillTimer(NULL,timerid);
        timerid=0;
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
    dTHR;
    return des_fcrypt(txt, salt, w32_crypt_buffer);
#else
    Perl_croak(aTHX_ "The crypt() function is unimplemented due to excessive paranoia.");
    return NULL;
#endif
}


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
  return (&(environ));
}

/* the rest are the remapped stdio routines */
DllExport FILE *
win32_stderr(void)
{
    return (stderr);
}

char *g_getlogin() {
    return "no-getlogin";
}

DllExport FILE *
win32_stdin(void)
{
    return (stdin);
}

DllExport FILE *
win32_stdout()
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
  return xcestrerror(e);
}

DllExport void
win32_str_os_error(void *sv, DWORD dwErr)
{
  dTHX;

  sv_setpvn((SV*)sv, "Error", 5);
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

DllExport FILE *
win32_fopen(const char *filename, const char *mode)
{
  return xcefopen(filename, mode);
}

DllExport FILE *
win32_fdopen(int handle, const char *mode)
{
  return palm_fdopen(handle, mode);
}

DllExport FILE *
win32_freopen(const char *path, const char *mode, FILE *stream)
{
  return xcefreopen(path, mode, stream);
}

DllExport int
win32_fclose(FILE *pf)
{
  return xcefclose(pf);
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
  return palm_fileno(pf);
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

DllExport long
win32_ftell(FILE *pf)
{
  return ftell(pf);
}

DllExport int
win32_fseek(FILE *pf, Off_t offset,int origin)
{
  return fseek(pf, offset, origin);
}

/* fpos_t seems to be int64 on hpc pro! Really stupid. */
/* But maybe someday there will be such large disks in a hpc... */
DllExport int
win32_fgetpos(FILE *pf, fpos_t *p)
{
  return fgetpos(pf, p);
}

DllExport int
win32_fsetpos(FILE *pf, const fpos_t *p)
{
  return fsetpos(pf, p);
}

DllExport void
win32_rewind(FILE *pf)
{
  fseek(pf, 0, SEEK_SET);
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
  xceabort();

  return;
}

DllExport int
win32_fstat(int fd, struct stat *sbufptr)
{
  return xcefstat(fd, sbufptr);
}

DllExport int
win32_link(const char *oldname, const char *newname)
{
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "link");

  return -1;
}

DllExport int
win32_rename(const char *oname, const char *newname)
{
  return xcerename(oname, newname);
}

DllExport int
win32_setmode(int fd, int mode)
{
    /* currently 'celib' seem to have this function in src, but not
     * exported. When it will be, we'll uncomment following line.
     */
    /* return xcesetmode(fd, mode); */
    return 0;
}

DllExport int
win32_chsize(int fd, Off_t size)
{
    return chsize(fd, size);
}

DllExport long
win32_lseek(int fd, Off_t offset, int origin)
{
  return xcelseek(fd, offset, origin);
}

DllExport long
win32_tell(int fd)
{
  return xcelseek(fd, 0, SEEK_CUR);
}

DllExport int
win32_open(const char *path, int flag, ...)
{
  int pmode;
  va_list ap;

  va_start(ap, flag);
  pmode = va_arg(ap, int);
  va_end(ap);

  return xceopen(path, flag, pmode);
}

DllExport int
win32_close(int fd)
{
  return xceclose(fd);
}

DllExport int
win32_eof(int fd)
{
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "eof");
  return -1;
}

DllExport int
win32_dup(int fd)
{
  return xcedup(fd); /* from celib/ceio.c; requires some more work on it */
}

DllExport int
win32_dup2(int fd1,int fd2)
{
  return xcedup2(fd1,fd2);
}

DllExport int
win32_read(int fd, void *buf, unsigned int cnt)
{
  return xceread(fd, buf, cnt);
}

DllExport int
win32_write(int fd, const void *buf, unsigned int cnt)
{
  return xcewrite(fd, (void *) buf, cnt);
}

DllExport int
win32_mkdir(const char *dir, int mode)
{
  return xcemkdir(dir);
}

DllExport int
win32_rmdir(const char *dir)
{
  return xcermdir(dir);
}

DllExport int
win32_chdir(const char *dir)
{
  return xcechdir(dir);
}

DllExport  int
win32_access(const char *path, int mode)
{
  return xceaccess(path, mode);
}

DllExport  int
win32_chmod(const char *path, int mode)
{
  return xcechmod(path, mode);
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

    if (bat_file) {
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
    Newx(fullcmd, MAX_PATH+1, char);
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
		    if ((STRLEN)(curfullcmd-fullcmd) < MAX_PATH-cmdlen-5)
			*curfullcmd++ = *pathstr;
		    pathstr++;
		}
		if (*pathstr)
		    pathstr++;		/* skip trailing '"' */
	    }
	    else {
		if ((STRLEN)(curfullcmd-fullcmd) < MAX_PATH-cmdlen-5)
		    *curfullcmd++ = *pathstr;
		pathstr++;
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
	    xcesetenv(cur, "", 0);
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
        /* not supported on CE create |= CREATE_NEW_PROCESS_GROUP; */
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
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "execv");
  return -1;
}

DllExport int
win32_execvp(const char *cmdname, const char *const *argv)
{
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "execvp");
  return -1;
}

DllExport void
win32_perror(const char *str)
{
  xceperror(str);
}

DllExport void
win32_setbuf(FILE *pf, char *buf)
{
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "setbuf");
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

static char *committed = NULL;
static char *base      = NULL;
static char *reserved  = NULL;
static char *brk       = NULL;
static DWORD pagesize  = 0;
static DWORD allocsize = 0;

void *
sbrk(int need)
{
 void *result;
 if (!pagesize)
  {SYSTEM_INFO info;
   GetSystemInfo(&info);
   /* Pretend page size is larger so we don't perpetually
    * call the OS to commit just one page ...
    */
   pagesize = info.dwPageSize << 3;
   allocsize = info.dwAllocationGranularity;
  }
 /* This scheme fails eventually if request for contiguous
  * block is denied so reserve big blocks - this is only
  * address space not memory ...
  */
 if (brk+need >= reserved)
  {
   DWORD size = 64*1024*1024;
   char *addr;
   if (committed && reserved && committed < reserved)
    {
     /* Commit last of previous chunk cannot span allocations */
     addr = (char *) VirtualAlloc(committed,reserved-committed,MEM_COMMIT,PAGE_READWRITE);
     if (addr)
      committed = reserved;
    }
   /* Reserve some (more) space
    * Note this is a little sneaky, 1st call passes NULL as reserved
    * so lets system choose where we start, subsequent calls pass
    * the old end address so ask for a contiguous block
    */
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
   char *addr = (char *) VirtualAlloc(committed,size,MEM_COMMIT,PAGE_READWRITE);
   if (addr)
    {
     committed += size;
    }
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

int
win32_open_osfhandle(intptr_t osfhandle, int flags)
{
    int fh;
    char fileflags=0;		/* _osfile flags */

    Perl_croak_nocontext("win32_open_osfhandle() TBD on this platform");
    return 0;
}

int
win32_get_osfhandle(int fd)
{
    int fh;
    char fileflags=0;		/* _osfile flags */

    Perl_croak_nocontext("win32_get_osfhandle() TBD on this platform");
    return 0;
}

FILE *
win32_fdupopen(FILE *pf)
{
    FILE* pfdup;
    fpos_t pos;
    char mode[3];
    int fileno = win32_dup(win32_fileno(pf));
    int fmode = palm_fgetmode(pfdup);

    fprintf(stderr,"DEBUG for win32_fdupopen()\n");

    /* open the file in the same mode */
    if(fmode & O_RDONLY) {
	mode[0] = 'r';
	mode[1] = 0;
    }
    else if(fmode & O_APPEND) {
	mode[0] = 'a';
	mode[1] = 0;
    }
    else if(fmode & O_RDWR) {
	mode[0] = 'r';
	mode[1] = '+';
	mode[2] = 0;
    }

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
    HMODULE hModule;

    hModule = XCELoadLibraryA(filename);

    return hModule;
}

/* this is needed by Cwd.pm... */

static
XS(w32_GetCwd)
{
  dXSARGS;
  char buf[MAX_PATH];
  SV *sv = sv_newmortal();

  xcegetcwd(buf, sizeof(buf));

  sv_setpv(sv, xcestrdup(buf));
  EXTEND(SP,1);
  SvPOK_on(sv);
  ST(0) = sv;
#ifndef INCOMPLETE_TAINTS
  SvTAINTED_on(ST(0));
#endif
  XSRETURN(1);
}

static
XS(w32_SetCwd)
{
  dXSARGS;

  if (items != 1)
    Perl_croak(aTHX_ "usage: Win32::SetCwd($cwd)");

  if (!xcechdir(SvPV_nolen(ST(0))))
    XSRETURN_YES;

  XSRETURN_NO;
}

static
XS(w32_GetTickCount)
{
    dXSARGS;
    DWORD msec = GetTickCount();
    EXTEND(SP,1);
    if ((IV)msec > 0)
	XSRETURN_IV(msec);
    XSRETURN_NV(msec);
}

static
XS(w32_GetOSVersion)
{
    dXSARGS;
    OSVERSIONINFOA osver;

    osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    if (!XCEGetVersionExA(&osver)) {
      XSRETURN_EMPTY;
    }
    mXPUSHp(osver.szCSDVersion, strlen(osver.szCSDVersion));
    mXPUSHi(osver.dwMajorVersion);
    mXPUSHi(osver.dwMinorVersion);
    mXPUSHi(osver.dwBuildNumber);
    /* WINCE = 3 */
    mXPUSHi(osver.dwPlatformId);
    PUTBACK;
}

static
XS(w32_IsWinNT)
{
    dXSARGS;
    EXTEND(SP,1);
    XSRETURN_IV(IsWinNT());
}

static
XS(w32_IsWin95)
{
    dXSARGS;
    EXTEND(SP,1);
    XSRETURN_IV(IsWin95());
}

static
XS(w32_IsWinCE)
{
    dXSARGS;
    EXTEND(SP,1);
    XSRETURN_IV(IsWinCE());
}

static
XS(w32_GetOemInfo)
{
  dXSARGS;
  wchar_t wbuf[126];
  char buf[126];

  if(SystemParametersInfoW(SPI_GETOEMINFO, sizeof(wbuf), wbuf, FALSE))
    WideCharToMultiByte(CP_ACP, 0, wbuf, -1, buf, sizeof(buf), 0, 0);
  else
    sprintf(buf, "SystemParametersInfo failed: %d", GetLastError());

  EXTEND(SP,1);
  XSRETURN_PV(buf);
}

static
XS(w32_Sleep)
{
    dXSARGS;
    if (items != 1)
	Perl_croak(aTHX_ "usage: Win32::Sleep($milliseconds)");
    Sleep(SvIV(ST(0)));
    XSRETURN_YES;
}

static
XS(w32_CopyFile)
{
    dXSARGS;
    BOOL bResult;
    if (items != 3)
	Perl_croak(aTHX_ "usage: Win32::CopyFile($from, $to, $overwrite)");

    {
      char szSourceFile[MAX_PATH+1];
      strcpy(szSourceFile, PerlDir_mapA(SvPV_nolen(ST(0))));
      bResult = XCECopyFileA(szSourceFile, SvPV_nolen(ST(1)),
			     !SvTRUE(ST(2)));
    }

    if (bResult)
	XSRETURN_YES;

    XSRETURN_NO;
}

static
XS(w32_MessageBox)
{
    dXSARGS;

    char *txt;
    unsigned int res;
    unsigned int flags = MB_OK;

    txt = SvPV_nolen(ST(0));

    if (items < 1 || items > 2)
	Perl_croak(aTHX_ "usage: Win32::MessageBox($txt, [$flags])");

    if(items == 2)
      flags = SvIV(ST(1));

    res = XCEMessageBoxA(NULL, txt, "Perl", flags);

    XSRETURN_IV(res);
}

static
XS(w32_GetPowerStatus)
{
  dXSARGS;

  SYSTEM_POWER_STATUS_EX sps;

  if(GetSystemPowerStatusEx(&sps, TRUE) == FALSE)
    {
      XSRETURN_EMPTY;
    }

  mXPUSHi(sps.ACLineStatus);
  mXPUSHi(sps.BatteryFlag);
  mXPUSHi(sps.BatteryLifePercent);
  mXPUSHi(sps.BatteryLifeTime);
  mXPUSHi(sps.BatteryFullLifeTime);
  mXPUSHi(sps.BackupBatteryFlag);
  mXPUSHi(sps.BackupBatteryLifePercent);
  mXPUSHi(sps.BackupBatteryLifeTime);
  mXPUSHi(sps.BackupBatteryFullLifeTime);

  PUTBACK;
}

#if UNDER_CE > 200
static
XS(w32_ShellEx)
{
  dXSARGS;

  char buf[126];
  SHELLEXECUTEINFO si;
  char *file, *verb;
  wchar_t wfile[MAX_PATH];
  wchar_t wverb[20];

  if (items != 2)
    Perl_croak(aTHX_ "usage: Win32::ShellEx($file, $verb)");

  file = SvPV_nolen(ST(0));
  verb = SvPV_nolen(ST(1));

  memset(&si, 0, sizeof(si));
  si.cbSize = sizeof(si);
  si.fMask = SEE_MASK_FLAG_NO_UI;

  MultiByteToWideChar(CP_ACP, 0, verb, -1,
		      wverb, sizeof(wverb)/2);
  si.lpVerb = (TCHAR *)wverb;

  MultiByteToWideChar(CP_ACP, 0, file, -1,
		      wfile, sizeof(wfile)/2);
  si.lpFile = (TCHAR *)wfile;

  if(ShellExecuteEx(&si) == FALSE)
    {
      XSRETURN_NO;
    }
  XSRETURN_YES;
}
#endif

void
Perl_init_os_extras(void)
{
    dTHX;
    char *file = __FILE__;
    dXSUB_SYS;

    w32_perlshell_tokens = NULL;
    w32_perlshell_items = -1;
    w32_fdpid = newAV(); /* XX needs to be in Perl_win32_init()? */
    Newx(w32_children, 1, child_tab);
    w32_num_children = 0;

    newXS("Win32::GetCwd", w32_GetCwd, file);
    newXS("Win32::SetCwd", w32_SetCwd, file);
    newXS("Win32::GetTickCount", w32_GetTickCount, file);
    newXS("Win32::GetOSVersion", w32_GetOSVersion, file);
#if UNDER_CE > 200
    newXS("Win32::ShellEx", w32_ShellEx, file);
#endif
    newXS("Win32::IsWinNT", w32_IsWinNT, file);
    newXS("Win32::IsWin95", w32_IsWin95, file);
    newXS("Win32::IsWinCE", w32_IsWinCE, file);
    newXS("Win32::CopyFile", w32_CopyFile, file);
    newXS("Win32::Sleep", w32_Sleep, file);
    newXS("Win32::MessageBox", w32_MessageBox, file);
    newXS("Win32::GetPowerStatus", w32_GetPowerStatus, file);
    newXS("Win32::GetOemInfo", w32_GetOemInfo, file);
}

void
myexit(void)
{
  char buf[126];

  puts("Hit return");
  fgets(buf, sizeof(buf), stdin);
}

void
Perl_win32_init(int *argcp, char ***argvp)
{
#ifdef UNDER_CE
  char *p;

  if((p = xcegetenv("PERLDEBUG")) && (p[0] == 'y' || p[0] == 'Y'))
    atexit(myexit);
#endif

  MALLOC_INIT;
}

DllExport void
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

win32_flock(int fd, int oper)
{
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "flock");
  return -1;
}

DllExport int
win32_waitpid(int pid, int *status, int flags)
{
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "waitpid");
  return -1;
}

DllExport int
win32_wait(int *status)
{
  dTHX;
  Perl_croak(aTHX_ PL_no_func, "wait");
  return -1;
}

int
wce_reopen_stdout(char *fname)
{
  if(xcefreopen(fname, "w", stdout) == NULL)
    return -1;

  return 0;
}

void
wce_hitreturn()
{
  char buf[126];

  printf("Hit RETURN");
  fflush(stdout);
  fgets(buf, sizeof(buf), stdin);
  return;
}

/* //////////////////////////////////////////////////////////////////// */

#undef getcwd

char *
getcwd(char *buf, size_t size)
{
  return xcegetcwd(buf, size);
}

int
isnan(double d)
{
  return _isnan(d);
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
    return _popen(command, mode);
}

/*
 * pclose() clone
 */

DllExport int
win32_pclose(PerlIO *pf)
{
    return _pclose(pf);
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
    Newx(w32_pseudo_children, 1, child_tab);
    w32_num_pseudo_children	= 0;
#  endif
    w32_init_socktype		= 0;
    w32_timerid                 = 0;
    w32_poll_count              = 0;
}

void
Perl_sys_intern_clear(pTHX)
{
    Safefree(w32_perlshell_tokens);
    Safefree(w32_perlshell_vec);
    /* NOTE: w32_fdpid is freed by sv_clean_all() */
    Safefree(w32_children);
    if (w32_timerid) {
    	KillTimer(NULL,w32_timerid);
    	w32_timerid=0;
    }
#  ifdef USE_ITHREADS
    Safefree(w32_pseudo_children);
#  endif
}

#  ifdef USE_ITHREADS

void
Perl_sys_intern_dup(pTHX_ struct interp_intern *src, struct interp_intern *dst)
{
    dst->perlshell_tokens	= NULL;
    dst->perlshell_vec		= (char**)NULL;
    dst->perlshell_items	= 0;
    dst->fdpid			= newAV();
    Newxz(dst->children, 1, child_tab);
    dst->pseudo_id		= 0;
    Newxz(dst->pseudo_children, 1, child_tab);
    dst->thr_intern.Winit_socktype = 0;
    dst->timerid                 = 0;
    dst->poll_count              = 0;
    Copy(src->sigtable,dst->sigtable,SIG_SIZE,Sighandler_t);
}
#  endif /* USE_ITHREADS */
#endif /* HAVE_INTERP_INTERN */

// added to remove undefied symbol error in CodeWarrior compilation
int
Perl_Ireentrant_buffer_ptr(aTHX)
{
	return 0;
}
