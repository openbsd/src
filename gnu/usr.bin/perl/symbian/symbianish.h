/*
 *	symbianish.h
 *
 *	Copyright (c) Nokia 2004-2005.  All rights reserved.
 *      This code is licensed under the same terms as Perl itself.
 *
 */

#include "symbian/symbian_port.h"

/*
 * The following symbols are defined if your operating system supports
 * functions by that name.  All Unixes I know of support them, thus they
 * are not checked by the configuration script, but are directly defined
 * here.
 */

#ifndef PERL_MICRO

/* HAS_IOCTL:
 *	This symbol, if defined, indicates that the ioctl() routine is
 *	available to set I/O characteristics
 */
#define	HAS_IOCTL		/ **/

/* HAS_UTIME:
 *	This symbol, if defined, indicates that the routine utime() is
 *	available to update the access and modification times of files.
 */
/* #define HAS_UTIME		/ **/

/* HAS_GROUP
 *	This symbol, if defined, indicates that the getgrnam() and
 *	getgrgid() routines are available to get group entries.
 *	The getgrent() has a separate definition, HAS_GETGRENT.
 */
#undef HAS_GROUP		/**/

/* HAS_PASSWD
 *	This symbol, if defined, indicates that the getpwnam() and
 *	getpwuid() routines are available to get password entries.
 *	The getpwent() has a separate definition, HAS_GETPWENT.
 */
#undef HAS_PASSWD		/**/

#undef HAS_KILL
#undef HAS_WAIT

#endif /* !PERL_MICRO */

/* USEMYBINMODE
 *	This symbol, if defined, indicates that the program should
 *	use the routine my_binmode(FILE *fp, char iotype) to insure
 *	that a file is in "binary" mode -- that is, that no translation
 *	of bytes occurs on read or write operations.
 */
#undef USEMYBINMODE

/* Stat_t:
 *	This symbol holds the type used to declare buffers for information
 *	returned by stat().  It's usually just struct stat.  It may be necessary
 *	to include <sys/stat.h> and <sys/types.h> to get any typedef'ed
 *	information.
 */
#define Stat_t struct stat

/* USE_STAT_RDEV:
 *	This symbol is defined if this system has a stat structure declaring
 *	st_rdev
 */
#define USE_STAT_RDEV 	/**/

/* ACME_MESS:
 *	This symbol, if defined, indicates that error messages should be
 *	should be generated in a format that allows the use of the Acme
 *	GUI/editor's autofind feature.
 */
#undef ACME_MESS	/**/

/* UNLINK_ALL_VERSIONS:
 *	This symbol, if defined, indicates that the program should arrange
 *	to remove all versions of a file if unlink() is called.  This is
 *	probably only relevant for VMS.
 */
/* #define UNLINK_ALL_VERSIONS		/ **/

/* VMS:
 *	This symbol, if defined, indicates that the program is running under
 *	VMS.  It is currently automatically set by cpps running under VMS,
 *	and is included here for completeness only.
 */
/* #define VMS		/ **/

/* ALTERNATE_SHEBANG:
 *	This symbol, if defined, contains a "magic" string which may be used
 *	as the first line of a Perl program designed to be executed directly
 *	by name, instead of the standard Unix #!.  If ALTERNATE_SHEBANG
 *	begins with a character other then #, then Perl will only treat
 *	it as a command line if it finds the string "perl" in the first
 *	word; otherwise it's treated as the first line of code in the script.
 *	(IOW, Perl won't hand off to another interpreter via an alternate
 *	shebang sequence that might be legal Perl code.)
 */
/* #define ALTERNATE_SHEBANG "#!" / **/

#include <signal.h>
#define ABORT() abort()

/*
 * fwrite1() should be a routine with the same calling sequence as fwrite(),
 * but which outputs all of the bytes requested as a single stream (unlike
 * fwrite() itself, which on some systems outputs several distinct records
 * if the number_of_items parameter is >1).
 */
#define fwrite1 fwrite

#define Stat(fname,bufptr) stat((fname),(bufptr))
#define Fstat(fd,bufptr)   fstat((fd),(bufptr))
#define Fflush(fp)         fflush(fp)
#define Mkdir(path,mode)   mkdir((path),(mode))

#ifndef PERL_SYS_TERM_BODY
#define PERL_SYS_TERM_BODY()	HINTS_REFCNT_TERM; OP_REFCNT_TERM; \
				PERLIO_TERM; MALLOC_TERM; CloseSTDLIB();
#endif

#define BIT_BUCKET "NUL:"

#define dXSUB_SYS

#define NO_ENVIRON_ARRAY

int kill(pid_t pid, int signo);
pid_t wait(int *status);

#ifdef PERL_GLOBAL_STRUCT_PRIVATE
#  undef PERL_GET_VARS
#  undef PERL_SET_VARS
#  undef PERL_UNSET_VARS
#  define PERL_GET_VARS()    symbian_get_vars()
#  define PERL_SET_VARS(v)   symbian_set_vars(v)
#  define PERL_UNSET_VARS(v) symbian_unset_vars()
#endif /* #ifdef PERL_GLOBAL_STRUCT_PRIVATE */

#undef PERL_EXPORT_C
#define PERL_EXPORT_C EXPORT_C /* for perlio.h */
#define PERL_CALLCONV EXPORT_C /* for proto.h */
#undef PERL_XS_EXPORT_C
#define PERL_XS_EXPORT_C EXPORT_C

#ifndef PERL_CORE
#define PERL_CORE /* for WINS builds under VC */
#endif

#ifdef USE_PERLIO
#define PERL_NEED_APPCTX /* need storing the PerlBase* */
#define PERLIO_STD_SPECIAL
#define PERLIO_STD_IN(f, b, n)  symbian_read_stdin(f, b, n)
#define PERLIO_STD_OUT(f, b, n) symbian_write_stdout(f, b, n)
/* The console (the STD*) streams are seen by Perl in UTF-8. */
#define PERL_SYMBIAN_CONSOLE_UTF8

#endif

#undef Strerror
#undef strerror
#define Strerror(eno) ((eno) < 0 ? symbian_get_error_string(eno) : strerror(eno))

#define PERL_NEED_TIMESBASE

#define times(b)  symbian_times(b)
#define usleep(u) symbian_usleep(u)

#define PERL_SYS_INIT_BODY(c, v) symbian_sys_init(c, v)

#ifdef __SERIES60_1X__
#  error "Unfortunately Perl does not work in S60 1.2 (see FAQ-0929)"
#endif

#ifdef _MSC_VER

/* The Symbian SDK insists on the /W4 flag for Visual C.
 * The Perl sources are not _that_ clean (Perl builds for Win32 use
 * the /W3 flag, and gcc builds always use -Wall, so the sources are
 * quite clean).  To avoid a flood of warnings let's shut up most
 * (for VC 6.0 SP 5). */

#pragma warning(disable: 4054) /* function pointer to data pointer */
#pragma warning(disable: 4055) /* data pointer to function pointer */
#pragma warning(disable: 4100) /* unreferenced formal parameter */
#pragma warning(disable: 4101) /* unreferenced local variable */
#pragma warning(disable: 4102) /* unreferenced label */
#pragma warning(disable: 4113) /* prototype difference */
#pragma warning(disable: 4127) /* conditional expression is constant */
#pragma warning(disable: 4132) /* const object should be initialized */
#pragma warning(disable: 4133) /* incompatible types */
#pragma warning(disable: 4189) /* initialized but not referenced */
#pragma warning(disable: 4244) /* conversion from ... possible loss ... */
#pragma warning(disable: 4245) /* signed/unsigned char */
#pragma warning(disable: 4310) /* cast truncates constant value */
#pragma warning(disable: 4505) /* function has been removed */
#pragma warning(disable: 4510) /* default constructor could not ... */
#pragma warning(disable: 4610) /* struct ... can never be instantiated */
#pragma warning(disable: 4701) /* used without having been initialized */
#pragma warning(disable: 4702) /* unreachable code */
#pragma warning(disable: 4706) /* assignment within conditional */
#pragma warning(disable: 4761) /* integral size mismatch */

#endif /* _MSC_VER */

#ifdef __MWERKS__
/* No good way of using the CodeWarrior #pragma unused(varname) with Perl
 * source code (e.g. PERL_UNUSED_DECL doesn't work with the pragma syntax).
 * Therefore we brutally  turn off these particular warnings since there
 * is a lot of this in Perl code (pTHX, for example).  TOther compilers
 * will have to detect these naughty bits. */
#pragma warn_unusedarg off
#pragma warn_unusedvar off
#pragma warn_emptydecl off
#endif
