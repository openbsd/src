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

#define perl
#include "celib_defs.h"
#include "cewin32.h"
#include "cecrt.h"
#include "cewin32_defs.h"
#include "cecrt_defs.h"

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

static long		filetime_to_clock(PFILETIME ft);
static BOOL		filetime_from_time(PFILETIME ft, time_t t);
static char *		get_emd_part(SV **leading, char *trailing, ...);
static char *		win32_get_xlib(const char *pl, const char *xlib,
				       const char *libname);

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
    char *str = Nullch;
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

    return Nullch;
}

char *
win32_get_privlib(const char *pl)
{
    dTHX;
    char *stdlib = "lib";
    char buffer[MAX_PATH+1];
    SV *sv = Nullsv;

    /* $stdlib = $HKCU{"lib-$]"} || $HKLM{"lib-$]"} || $HKCU{"lib"} || $HKLM{"lib"} || "";  */
    sprintf(buffer, "%s-%s", stdlib, pl);
    if (!get_regstr(buffer, &sv))
	(void)get_regstr(stdlib, &sv);

    /* $stdlib .= ";$EMD/../../lib" */
    return get_emd_part(&sv, stdlib, ARCHNAME, "bin", Nullch);
}

static char *
win32_get_xlib(const char *pl, const char *xlib, const char *libname)
{
    dTHX;
    char regstr[40];
    char pathstr[MAX_PATH+1];
    DWORD datalen;
    int len, newsize;
    SV *sv1 = Nullsv;
    SV *sv2 = Nullsv;

    /* $HKCU{"$xlib-$]"} || $HKLM{"$xlib-$]"} . ---; */
    sprintf(regstr, "%s-%s", xlib, pl);
    (void)get_regstr(regstr, &sv1);

    /* $xlib .=
     * ";$EMD/" . ((-d $EMD/../../../$]) ? "../../.." : "../.."). "/$libname/$]/lib";  */
    sprintf(pathstr, "%s/%s/lib", libname, pl);
    (void)get_emd_part(&sv1, pathstr, ARCHNAME, "bin", pl, Nullch);

    /* $HKCU{$xlib} || $HKLM{$xlib} . ---; */
    (void)get_regstr(xlib, &sv2);

    /* $xlib .=
     * ";$EMD/" . ((-d $EMD/../../../$]) ? "../../.." : "../.."). "/$libname/lib";  */
    sprintf(pathstr, "%s/lib", libname);
    (void)get_emd_part(&sv2, pathstr, ARCHNAME, "bin", pl, Nullch);

    if (!sv1 && !sv2)
	return Nullch;
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

#if !defined(PERL_IMPLICIT_SYS)
/* since the current process environment is being updated in util.c
 * the library functions will get the correct environment
 */
PerlIO *
Perl_my_popen(pTHX_ char *cmd, char *mode)
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
  return xcegetpid();
}

bool
Perl_do_exec(pTHX_ char *cmd)
{
  Perl_croak_nocontext("exec() unimplemented on this platform");
  return FALSE;
}

DllExport int
win32_pipe(int *pfd, unsigned int size, int mode)
{
  Perl_croak(aTHX_ PL_no_func, "pipe");
  return -1;
}

DllExport int
win32_times(struct tms *timebuf)
{
  Perl_croak(aTHX_ PL_no_func, "times");
  return -1;
}

/* TODO */
bool
win32_signal()
{
  Perl_croak_nocontext("signal() TBD on this platform");
  return FALSE;
}
DllExport void
win32_clearenv()
{
  return;
}


DllExport char ***
win32_environ(void)
{
  return (&(environ));
}

DllExport DIR *
win32_opendir(char *filename)
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
  Perl_croak(aTHX_ PL_no_func, "telldir");
  return -1;
}

DllExport void
win32_seekdir(DIR *dirp, long loc)
{
  Perl_croak(aTHX_ PL_no_func, "seekdir");
}

DllExport void
win32_rewinddir(DIR *dirp)
{
  Perl_croak(aTHX_ PL_no_func, "rewinddir");
}

DllExport int
win32_closedir(DIR *dirp)
{
  closedir(dirp);
  return 0;
}

DllExport int
win32_kill(int pid, int sig)
{
  Perl_croak(aTHX_ PL_no_func, "kill");
  return -1;
}

DllExport unsigned int
win32_sleep(unsigned int t)
{
  return xcesleep(t);
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

#ifndef USE_WIN32_RTL_ENV

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

#endif

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

static UINT timerid = 0;

static VOID CALLBACK TimerProc(HWND win, UINT msg, UINT id, DWORD time)
{
    dTHX;
    KillTimer(NULL,timerid);
    timerid=0;  
    sighandler(14);
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
    return Nullch;
#endif
}

/* C doesn't like repeat struct definitions */

#if defined(USE_FIXED_OSFHANDLE) || defined(PERL_MSVCRT_READFIX)

#ifndef _CRTIMP
#define _CRTIMP __declspec(dllimport)
#endif

/*
 * Control structure for lowio file handles
 */
typedef struct {
    long osfhnd;    /* underlying OS file HANDLE */
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

#endif

/*
 *  redirected io subsystem for all XS modules
 *
 */

DllExport int *
win32_errno(void)
{
    return (&errno);
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
win32_fseek(FILE *pf,long offset,int origin)
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

DllExport FILE*
win32_tmpfile(void)
{
  Perl_croak(aTHX_ PL_no_func, "tmpfile");

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

DllExport long
win32_lseek(int fd, long offset, int origin)
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
  Perl_croak(aTHX_ PL_no_func, "eof");
  return -1;
}

DllExport int
win32_dup(int fd)
{
  Perl_croak(aTHX_ PL_no_func, "dup");
  return -1;
}

DllExport int
win32_dup2(int fd1,int fd2)
{
  Perl_croak(aTHX_ PL_no_func, "dup2");
  return -1;
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

DllExport void
win32_perror(const char *str)
{
  xceperror(str);
}

DllExport void
win32_setbuf(FILE *pf, char *buf)
{
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

DllExport int
win32_execv(const char *cmdname, const char *const *argv)
{
  Perl_croak(aTHX_ PL_no_func, "execv");
  return -1;
}

DllExport int
win32_execvp(const char *cmdname, const char *const *argv)
{
  Perl_croak(aTHX_ PL_no_func, "execvp");
  return -1;
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
    XPUSHs(newSVpvn(osver.szCSDVersion, strlen(osver.szCSDVersion)));
    XPUSHs(newSViv(osver.dwMajorVersion));
    XPUSHs(newSViv(osver.dwMinorVersion));
    XPUSHs(newSViv(osver.dwBuildNumber));
    /* WINCE = 3 */
    XPUSHs(newSViv(osver.dwPlatformId));
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

  XPUSHs(newSViv(sps.ACLineStatus));
  XPUSHs(newSViv(sps.BatteryFlag));
  XPUSHs(newSViv(sps.BatteryLifePercent));
  XPUSHs(newSViv(sps.BatteryLifeTime));
  XPUSHs(newSViv(sps.BatteryFullLifeTime));
  XPUSHs(newSViv(sps.BackupBatteryFlag));
  XPUSHs(newSViv(sps.BackupBatteryLifePercent));
  XPUSHs(newSViv(sps.BackupBatteryLifeTime));
  XPUSHs(newSViv(sps.BackupBatteryFullLifeTime));

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

    w32_perlshell_tokens = Nullch;
    w32_perlshell_items = -1;
    w32_fdpid = newAV(); /* XX needs to be in Perl_win32_init()? */
    New(1313, w32_children, 1, child_tab);
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

DllExport int
win32_flock(int fd, int oper)
{
  Perl_croak(aTHX_ PL_no_func, "flock");
  return -1;
}

DllExport int
win32_waitpid(int pid, int *status, int flags)
{
  Perl_croak(aTHX_ PL_no_func, "waitpid");
  return -1;
}

DllExport int
win32_wait(int *status)
{
  Perl_croak(aTHX_ PL_no_func, "wait");
  return -1;
}

int
do_spawn(char *cmd)
{
  return xcesystem(cmd);
}

int
do_aspawn(void *vreally, void **vmark, void **vsp)
{
  Perl_croak(aTHX_ PL_no_func, "aspawn");
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

void
win32_argv2utf8(int argc, char** argv)
{
  /* do nothing... */
}

void
Perl_sys_intern_init(pTHX)
{
    w32_perlshell_tokens	= Nullch;
    w32_perlshell_vec		= (char**)NULL;
    w32_perlshell_items		= 0;
    w32_fdpid			= newAV();
    New(1313, w32_children, 1, child_tab);
    w32_num_children		= 0;
#  ifdef USE_ITHREADS
    w32_pseudo_id		= 0;
    New(1313, w32_pseudo_children, 1, child_tab);
    w32_num_pseudo_children	= 0;
#  endif

#ifndef UNDER_CE
    w32_init_socktype		= 0;
#endif
}

void
Perl_sys_intern_clear(pTHX)
{
    Safefree(w32_perlshell_tokens);
    Safefree(w32_perlshell_vec);
    /* NOTE: w32_fdpid is freed by sv_clean_all() */
    Safefree(w32_children);
#  ifdef USE_ITHREADS
    Safefree(w32_pseudo_children);
#  endif
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

int
win32_open_osfhandle(intptr_t osfhandle, int flags)
{
    int fh;
    char fileflags=0;		/* _osfile flags */

    XCEMessageBoxA(NULL, "NEED TO IMPLEMENT in wince/wince.c(win32_open_osfhandle)", "error", 0);
    Perl_croak_nocontext("win32_open_osfhandle() TBD on this platform");
    return 0;
}

int
win32_get_osfhandle(intptr_t osfhandle, int flags)
{
    int fh;
    char fileflags=0;		/* _osfile flags */

    XCEMessageBoxA(NULL, "NEED TO IMPLEMENT in wince/wince.c(win32_get_osfhandle)", "error", 0);
    Perl_croak_nocontext("win32_get_osfhandle() TBD on this platform");
    return 0;
}

/*
 * a popen() clone that respects PERL5SHELL
 *
 * changed to return PerlIO* rather than FILE * by BKS, 11-11-2000
 */

DllExport PerlIO*
win32_popen(const char *command, const char *mode)
{
    XCEMessageBoxA(NULL, "NEED TO IMPLEMENT in wince/wince.c(win32_popen)", "error", 0);
    Perl_croak_nocontext("win32_popen() TBD on this platform");
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

FILE *
win32_fdupopen(FILE *pf)
{
    FILE* pfdup;
    fpos_t pos;
    char mode[3];
    int fileno = win32_dup(win32_fileno(pf));

    XCEMessageBoxA(NULL, "NEED TO IMPLEMENT a place in .../wince/wince.c(win32_fdupopen)", "Perl(developer)", 0);
    Perl_croak_nocontext("win32_fdupopen() TBD on this platform");

#if 0
    /* open the file in the same mode */
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

    /* it appears that the binmode is attached to the
     * file descriptor so binmode files will be handled
     * correctly
     */
    pfdup = win32_fdopen(fileno, mode);

    /* move the file pointer to the same position */
    if (!fgetpos(pf, &pos)) {
	fsetpos(pfdup, &pos);
    }
#endif
    return pfdup;
}
