/* wince.h */

/* Time-stamp: <01/08/01 20:48:08 keuchel@w2k> */

/* This file includes extracts from the celib-headers, because */
/* the celib-headers produces macro conflicts with defines in */
/* win32iop.h etc */

#ifndef WINCE_H
#define WINCE_H 1

#include "celib_defs.h"

/* include local copies of celib headers... */
#include "errno.h"
#include "sys/stat.h"
#include "time.h"
#include "cectype.h"

#define _IOFBF          0x0000
#define _IOLBF          0x0040
#define _IONBF          0x0004

#if UNDER_CE <= 200
XCE_EXPORT double xceatof(const char *);
XCE_EXPORT int xcetoupper(int c);
XCE_EXPORT int xcetolower(int c);
#define atof xceatof
#define toupper xcetoupper
#define tolower xcetolower
#else
double atof(const char *);
#endif

XCE_EXPORT void XCEShowMessageA(const char *fmt, ...);

#define time xcetime
#define gmtime xcegmtime
#define localtime xcelocaltime
#define asctime xceasctime
/* #define utime xceutime */
#define futime xcefutime
#define ftime xceftime
#define ctime xcectime
#define gettimeofday xcegettimeofday

XCE_EXPORT int xcesetuid(uid_t id);
XCE_EXPORT int xceseteuid(uid_t id);
XCE_EXPORT int xcegetuid();
XCE_EXPORT int xcegeteuid();

XCE_EXPORT int xcesetgid(int id);
XCE_EXPORT int xcesetegid(int id);
XCE_EXPORT int xcegetgid();
XCE_EXPORT int xcegetegid();

#define setuid xcesetuid
#define getuid xcegetuid
#define geteuid xcegeteuid
#define seteuid xceseteuid

#define setgid xcesetgid
#define getgid xcegetgid
#define getegid xcegetegid
#define setegid xcesetegid

XCE_EXPORT int xcechown(const char *filename, int owner, int group);
#define chown xcechown

XCE_EXPORT char *xcestrrchr(const char * string, int ch);
#define strrchr xcestrrchr

XCE_EXPORT void (*xcesignal(int, void (*)(int)))(int);
XCE_EXPORT int xceraise(int);
#define signal xcesignal
#define raise xceraise

XCE_EXPORT int xcecreat(const char *filename, int pmode);
XCE_EXPORT int xceopen(const char *fname, int mode, ...);
XCE_EXPORT int xceread(int fd, void *buf, int size);
XCE_EXPORT int xcewrite(int fd, void *buf, int size);
XCE_EXPORT int xceclose(int fd);
XCE_EXPORT off_t xcelseek(int fd, int off, int whence);

XCE_EXPORT char *xcestrupr(char *string);
XCE_EXPORT char *xcestrlwr(char *string);
#define strupr xcestrupr
#define strlwr xcestrlwr

XCE_EXPORT double xcestrtod(const char *s, char **errorptr);
XCE_EXPORT long xcestrtol(const char *s, char **errorptr, int base);
XCE_EXPORT unsigned long xcestrtoul(const char *s, char **errorptr, int base);
#define strtod xcestrtod
#define strtol xcestrtol
#define strtoul xcestrtoul

XCE_EXPORT int xcestrnicmp(const char *first, const char *last, size_t count);
#define strnicmp xcestrnicmp

XCE_EXPORT int xceumask(int mask);
#define umask xceumask

XCE_EXPORT int xceisatty(int fd);
#define isatty xceisatty

XCE_EXPORT int xcechsize(int fd, unsigned long size);
#define chsize xcechsize

XCE_EXPORT char *xcegetlogin();
#define getlogin xcegetlogin

XCE_EXPORT DWORD XCEAPI XCEGetModuleFileNameA(HMODULE hModule, LPTSTR lpName, DWORD nSize);
XCE_EXPORT HMODULE XCEAPI XCEGetModuleHandleA(const char *lpName);
XCE_EXPORT FARPROC XCEAPI XCEGetProcAddressA(HMODULE hMod, const char *name);

/* //////////////////////////////////////////////////////////////////// */

#define getgid  xcegetgid
#define getegid xcegetegid
#define geteuid xcegeteuid
#define setgid  xcesetgid

#define strupr  xcestrupr
#define time    xcetime

XCE_EXPORT LPVOID XCEGetEnvironmentStrings(VOID);
XCE_EXPORT BOOL XCEFreeEnvironmentStrings(LPCSTR buf);
#define GetEnvironmentStrings XCEGetEnvironmentStrings
#define FreeEnvironmentStrings XCEFreeEnvironmentStrings

void wce_hitreturn();

#endif
