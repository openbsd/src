/*  vmsish.h
 *
 * VMS-specific C header file for perl5.
 *
 * Last revised: 18-Feb-1997 by Charles Bailey  bailey@genetics.upenn.edu
 * Version: 5.3.28
 */

#ifndef __vmsish_h_included
#define __vmsish_h_included

#include <descrip.h> /* for dirent struct definitions */
#include <libdef.h>  /* status codes for various places */
#include <rmsdef.h>  /* at which errno and vaxc$errno are */
#include <ssdef.h>   /* explicitly set in the perl source code */
#include <stsdef.h>  /* bitmasks for exit status testing */

/* Suppress compiler warnings from DECC for VMS-specific extensions:
 * ADDRCONSTEXT,NEEDCONSTEXT: initialization of data with non-constant values
 *                            (e.g. pointer fields of descriptors)
 */
#ifdef __DECC
#  pragma message disable (ADDRCONSTEXT,NEEDCONSTEXT)
#endif

/* DEC's C compilers and gcc use incompatible definitions of _to(upp|low)er() */
#ifdef _toupper
#  undef _toupper
#endif
#define _toupper(c) (((c) < 'a' || (c) > 'z') ? (c) : (c) & ~040)
#ifdef _tolower
#  undef _tolower
#endif
#define _tolower(c) (((c) < 'A' || (c) > 'Z') ? (c) : (c) | 040)
/* DECC 1.3 has a funny definition of abs; it's fixed in DECC 4.0, so this
 * can go away once DECC 1.3 isn't in use any more. */
#if defined(__ALPHA) && defined(__DECC)
#undef abs
#define abs(__x)        __ABS(__x)
#undef labs
#define labs(__x)        __LABS(__x)
#endif /* __ALPHA && __DECC */

/* Assorted things to look like Unix */
#ifdef __GNUC__
#ifndef _IOLBF /* gcc's stdio.h doesn't define this */
#define _IOLBF 1
#endif
#endif
#include <processes.h> /* for vfork() */
#include <unixio.h>
#include <unixlib.h>
#include <file.h>  /* it's not <sys/file.h>, so don't use I_SYS_FILE */
#if defined(__DECC) && defined(__DECC_VER) && __DECC_VER > 20000000
#  include <unistd.h> /* DECC has this; VAXC and gcc don't */
#endif

/* VAXC doesn't have a unary plus operator, so we need to get there indirectly */
#if defined(VAXC) && !defined(__DECC)
#  define NO_UNARY_PLUS
#endif

#ifdef NO_PERL_TYPEDEFS /* a2p; we don't want Perl's special routines */
#  define DONT_MASK_RTL_CALLS
#endif

    /* defined for vms.c so we can see CRTL |  defined for a2p */
#ifndef DONT_MASK_RTL_CALLS
#  ifdef getenv
#    undef getenv
#  endif
#  define getenv(v) my_getenv(v)  /* getenv used for regular logical names */
#endif

/* DECC introduces this routine in the RTL as of VMS 7.0; for now,
 * we'll use ours, since it gives us the full VMS exit status. */
#define waitpid my_waitpid

/* Don't redeclare standard RTL routines in Perl's header files;
 * VMS history or extensions makes some of the formal protoypes
 * differ from the common Unix forms.
 */
#define DONT_DECLARE_STD 1

/* Our own contribution to PerlShr's global symbols . . . */
#ifdef EMBED
#  define my_trnlnm		Perl_my_trnlnm
#  define my_getenv		Perl_my_getenv
#  define prime_env_iter	Perl_prime_env_iter
#  define my_setenv		Perl_my_setenv
#  define my_crypt		Perl_my_crypt
#  define my_waitpid		Perl_my_waitpid
#  define my_gconvert		Perl_my_gconvert
#  define do_rmdir		Perl_do_rmdir
#  define kill_file		Perl_kill_file
#  define my_mkdir		Perl_my_mkdir
#  define my_utime		Perl_my_utime
#  define rmsexpand	Perl_rmsexpand
#  define rmsexpand_ts	Perl_rmsexpand_ts
#  define fileify_dirspec	Perl_fileify_dirspec
#  define fileify_dirspec_ts	Perl_fileify_dirspec_ts
#  define pathify_dirspec	Perl_pathify_dirspec
#  define pathify_dirspec_ts	Perl_pathify_dirspec_ts
#  define tounixspec		Perl_tounixspec
#  define tounixspec_ts		Perl_tounixspec_ts
#  define tovmsspec		Perl_tovmsspec
#  define tovmsspec_ts		Perl_tovmsspec_ts
#  define tounixpath		Perl_tounixpath
#  define tounixpath_ts		Perl_tounixpath_ts
#  define tovmspath		Perl_tovmspath
#  define tovmspath_ts		Perl_tovmspath_ts
#  define vms_image_init	Perl_vms_image_init
#  define opendir		Perl_opendir
#  define readdir		Perl_readdir
#  define telldir		Perl_telldir
#  define seekdir		Perl_seekdir
#  define closedir		Perl_closedir
#  define vmsreaddirversions	Perl_vmsreaddirversions
#  define my_gmtime		Perl_my_gmtime
#  define my_localtime		Perl_my_localtime
#  define my_time		Perl_my_time
#  define my_sigemptyset        Perl_my_sigemptyset
#  define my_sigfillset         Perl_my_sigfillset
#  define my_sigaddset          Perl_my_sigaddset
#  define my_sigdelset          Perl_my_sigdelset
#  define my_sigismember        Perl_my_sigismember
#  define my_sigprocmask        Perl_my_sigprocmask
#  define cando_by_name		Perl_cando_by_name
#  define flex_fstat		Perl_flex_fstat
#  define flex_stat		Perl_flex_stat
#  define trim_unixpath		Perl_trim_unixpath
#  define my_vfork		Perl_my_vfork
#  define vms_do_aexec		Perl_vms_do_aexec
#  define vms_do_exec		Perl_vms_do_exec
#  define do_aspawn		Perl_do_aspawn
#  define do_spawn		Perl_do_spawn
#  define my_fwrite		Perl_my_fwrite
#  define my_flush		Perl_my_flush
#  define my_binmode		Perl_my_binmode
#  define my_getpwnam		Perl_my_getpwnam
#  define my_getpwuid		Perl_my_getpwuid
#  define my_getpwent		Perl_my_getpwent
#  define my_endpwent		Perl_my_endpwent
#  define my_getlogin		Perl_my_getlogin
#  define rmscopy		Perl_rmscopy
#  define init_os_extras	Perl_init_os_extras
#endif

/* Delete if at all possible, changing protections if necessary. */
#define unlink kill_file

/* 
 * Intercept calls to fork, so we know whether subsequent calls to
 * exec should be handled in VMSish or Unixish style.
 */
#define fork my_vfork
#ifndef DONT_MASK_RTL_CALLS     /* #defined in vms.c so we see real vfork */
#  ifdef vfork
#    undef vfork
#  endif
#  define vfork my_vfork
#endif

/* BIG_TIME:
 *	This symbol is defined if Time_t is an unsigned type on this system.
 */
#define BIG_TIME

/* ACME_MESS:
 *	This symbol, if defined, indicates that error messages should be 
 *	should be generated in a format that allows the use of the Acme
 *	GUI/editor's autofind feature.
 */
#undef ACME_MESS	/**/

/* ALTERNATE_SHEBANG:
 *	This symbol, if defined, contains a "magic" string which may be used
 *	as the first line of a Perl program designed to be executed directly
 *	by name, instead of the standard Unix #!.  If ALTERNATE_SHEBANG
 *	begins with a character other then #, then Perl will only treat
 *	it as a command line if if finds the string "perl" in the first
 *	word; otherwise it's treated as the first line of code in the script.
 *	(IOW, Perl won't hand off to another interpreter via an alternate
 *	shebang sequence that might be legal Perl code.)
 */
#define ALTERNATE_SHEBANG "$"

/* Macros to set errno using the VAX thread-safe calls, if present */
#if (defined(__DECC) || defined(__DECCXX)) && !defined(__ALPHA)
#  define set_errno(v)      (cma$tis_errno_set_value(v))
   void cma$tis_errno_set_value(int __value);  /* missing in some errno.h */
#  define set_vaxc_errno(v) (vaxc$errno = (v))
#else
#  define set_errno(v)      (errno = (v))
#  define set_vaxc_errno(v) (vaxc$errno = (v))
#endif

/* Support for 'vmsish' behaviors enabled with C<use vmsish> pragma */

#define COMPLEX_STATUS	1	/* We track both "POSIX" and VMS values */

#define HINT_V_VMSISH		24
#define HINT_M_VMSISH_STATUS	0x01000000 /* system, $? return VMS status */
#define HINT_M_VMSISH_EXIT	0x02000000 /* exit(1) ==> SS$_NORMAL */
#define HINT_M_VMSISH_TIME	0x04000000 /* times are local, not UTC */
#define NATIVE_HINTS		(PL_hints >> HINT_V_VMSISH)  /* used in op.c */

#define TEST_VMSISH(h)	(PL_curcop->op_private & ((h) >> HINT_V_VMSISH))
#define VMSISH_STATUS	TEST_VMSISH(HINT_M_VMSISH_STATUS)
#define VMSISH_EXIT	TEST_VMSISH(HINT_M_VMSISH_EXIT)
#define VMSISH_TIME	TEST_VMSISH(HINT_M_VMSISH_TIME)

/* Handy way to vet calls to VMS system services and RTL routines. */
#define _ckvmssts(call) STMT_START { register unsigned long int __ckvms_sts; \
  if (!((__ckvms_sts=(call))&1)) { \
  set_errno(EVMSERR); set_vaxc_errno(__ckvms_sts); \
  croak("Fatal VMS error (status=%d) at %s, line %d", \
  __ckvms_sts,__FILE__,__LINE__); } } STMT_END

/* Same thing, but don't call back to Perl's croak(); useful for errors
 * occurring during startup, before Perl's state is initialized */
#define _ckvmssts_noperl(call) STMT_START { register unsigned long int __ckvms_sts; \
  if (!((__ckvms_sts=(call))&1)) { \
  set_errno(EVMSERR); set_vaxc_errno(__ckvms_sts); \
  fprintf(Perl_debug_log,"Fatal VMS error (status=%d) at %s, line %d", \
  __ckvms_sts,__FILE__,__LINE__); lib$signal(__ckvms_sts); } } STMT_END

#ifdef VMS_DO_SOCKETS
#include "sockadapt.h"
#endif

#define BIT_BUCKET "_NLA0:"
#define PERL_SYS_INIT(c,v)	vms_image_init((c),(v)); MALLOC_INIT
#define PERL_SYS_TERM()		MALLOC_TERM
#define dXSUB_SYS
#define HAS_KILL
#define HAS_WAIT

/* VMS:
 *	This symbol, if defined, indicates that the program is running under
 *	VMS.  It's a symbol automagically defined by all VMS C compilers I've seen.
 * Just in case, however . . . */
#ifndef VMS
#define VMS		/**/
#endif

/* HAS_IOCTL:
 *	This symbol, if defined, indicates that the ioctl() routine is
 *	available to set I/O characteristics
 */
#undef	HAS_IOCTL		/**/
 
/* HAS_UTIME:
 *	This symbol, if defined, indicates that the routine utime() is
 *	available to update the access and modification times of files.
 */
#define HAS_UTIME		/**/

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
#define HAS_PASSWD		/**/

#define HAS_KILL
#define HAS_WAIT
  
/* USEMYBINMODE
 *	This symbol, if defined, indicates that the program should
 *	use the routine my_binmode(FILE *fp, char iotype) to insure
 *	that a file is in "binary" mode -- that is, that no translation
 *	of bytes occurs on read or write operations.
 */
#define USEMYBINMODE

/* Stat_t:
 *	This symbol holds the type used to declare buffers for information
 *	returned by stat().  It's usually just struct stat.  It may be necessary
 *	to include <sys/stat.h> and <sys/types.h> to get any typedef'ed
 *	information.
 */
/* VMS:
 * We need this typedef to point to the new type even if DONT_MASK_RTL_CALLS
 * is in effect, since Perl's thread.h embeds one of these structs in its
 * thread data struct, and our struct mystat is a different size from the
 * regular struct stat (cf. note above about having to pad struct to work
 * around bug in compiler.)
 * It's OK to pass one of these to the RTL's stat(), though, since the
 * fields it fills are the same in each struct.
 */
#define Stat_t struct mystat

/* USE_STAT_RDEV:
*	This symbol is defined if this system has a stat structure declaring
*	st_rdev
*	VMS: Field exists in POSIXish version of struct stat(), but is not used.
*/
#undef USE_STAT_RDEV		/**/

/*
 * fwrite1() should be a routine with the same calling sequence as fwrite(),
 * but which outputs all of the bytes requested as a single stream (unlike
 * fwrite() itself, which on some systems outputs several distinct records
 * if the number_of_items parameter is >1).
 */
#define fwrite1 my_fwrite

/* By default, flush data all the way to disk, not just to RMS buffers */
#define Fflush(fp) my_flush(fp)

/* Use our own rmdir() */
#define rmdir(name) do_rmdir(name)

/* Assorted fiddling with sigs . . . */
# include <signal.h>
#define ABORT() abort()
    /* VAXC's signal.h doesn't #define SIG_ERR, but provides BADSIG instead. */
#if !defined(SIG_ERR) && defined(BADSIG)
#  define SIG_ERR BADSIG
#endif


/* Used with our my_utime() routine in vms.c */
struct utimbuf {
  time_t actime;
  time_t modtime;
};
#define utime my_utime

/* This is what times() returns, but <times.h> calls it tbuffer_t on VMS
 * prior to v7.0.  We check the DECC manifest to see whether it's already
 * done this for us, relying on the fact that perl.h #includes <time.h>
 * before it #includes "vmsish.h".
 */

#ifndef __TMS
  struct tms {
    clock_t tms_utime;    /* user time */
    clock_t tms_stime;    /* system time - always 0 on VMS */
    clock_t tms_cutime;   /* user time, children */
    clock_t tms_cstime;   /* system time, children - always 0 on VMS */
  };
#else
   /* The new headers change the times() prototype to tms from tbuffer */
#  define tbuffer_t struct tms
#endif

/* Substitute our own routines for gmtime(), localtime(), and time(),
 * which allow us to implement the vmsish 'time' pragma, and work
 * around absence of system-level UTC support on old versions of VMS.
 */
#define gmtime(t) my_gmtime(t)
#define localtime(t) my_localtime(t)
#define time(t) my_time(t)

/* If we're using an older version of VMS whose Unix signal emulation
 * isn't very POSIXish, then roll our own.
 */
#if __VMS_VER < 70000000 || __DECC_VER < 50200000
#  define HOMEGROWN_POSIX_SIGNALS
#endif
#ifdef HOMEGROWN_POSIX_SIGNALS
#  define sigemptyset(t) my_sigemptyset(t)
#  define sigfillset(t) my_sigfillset(t)
#  define sigaddset(t, u) my_sigaddset(t, u)
#  define sigdelset(t, u) my_sigdelset(t, u)
#  define sigismember(t, u) my_sigismember(t, u)
#  define sigprocmask(t, u, v) my_sigprocmask(t, u, v)
#  ifndef _SIGSET_T
   typedef int sigset_t;
#  endif
   /* The tools for sigprocmask() are there, just not the routine itself */
#  ifndef SIG_UNBLOCK
#    define SIG_UNBLOCK 1
#  endif
#  ifndef SIG_BLOCK
#    define SIG_BLOCK 2
#  endif
#  ifndef SIG_SETMASK
#    define SIG_SETMASK 3
#  endif
#  define sigaction sigvec
#  define sa_flags sv_onstack
#  define sa_handler sv_handler
#  define sa_mask sv_mask
#  define sigsuspend(set) sigpause(*set)
#  define sigpending(a) (not_here("sigpending"),0)
#endif

/* VMS doesn't use a real sys_nerr, but we need this when scanning for error
 * messages in text strings . . .
 */

#define sys_nerr EVMSERR  /* EVMSERR is as high as we can go. */

/* Look up new %ENV values on the fly */
#define DYNAMIC_ENV_FETCH 1
#define ENV_HV_NAME "%EnV%VmS%"
  /* Special getenv function for retrieving %ENV elements. */
#define ENV_getenv(v) my_getenv(v)


/* Thin jacket around cuserid() tomatch Unix' calling sequence */
#define getlogin my_getlogin

/* Ditto for sys$hash_passwrod() . . . */
#define crypt  my_crypt

/* Tweak arg to mkdir first, so we can tolerate trailing /. */
#define Mkdir(dir,mode) my_mkdir((dir),(mode))

/* Use our own stat() clones, which handle Unix-style directory names */
#define Stat(name,bufptr) flex_stat(name,bufptr)
#define Fstat(fd,bufptr) flex_fstat(fd,bufptr)

/* Setup for the dirent routines:
 * opendir(), closedir(), readdir(), seekdir(), telldir(), and
 * vmsreaddirversions(), and preprocessor stuff on which these depend:
 *    Written by Rich $alz, <rsalz@bbn.com> in August, 1990.
 */
    /* Data structure returned by READDIR(). */
struct dirent {
    char	d_name[256];		/* File name		*/
    int		d_namlen;			/* Length of d_name */
    int		vms_verscount;		/* Number of versions	*/
    int		vms_versions[20];	/* Version numbers	*/
};

    /* Handle returned by opendir(), used by the other routines.  You
     * are not supposed to care what's inside this structure. */
typedef struct _dirdesc {
    long			context;
    int				vms_wantversions;
    unsigned long int           count;
    char			*pattern;
    struct dirent		entry;
    struct dsc$descriptor_s	pat;
} DIR;

#define rewinddir(dirp)		seekdir((dirp), 0)

/* used for our emulation of getpw* */
struct passwd {
        char    *pw_name;    /* Username */
        char    *pw_passwd;
        Uid_t   pw_uid;      /* UIC member number */
        Gid_t   pw_gid;      /* UIC group  number */
        char    *pw_comment; /* Default device/directory (Unix-style) */
        char    *pw_gecos;   /* Owner */
        char    *pw_dir;     /* Default device/directory (VMS-style) */
        char    *pw_shell;   /* Default CLI name (eg. DCL) */
};
#define pw_unixdir pw_comment  /* Default device/directory (Unix-style) */
#define getpwnam my_getpwnam
#define getpwuid my_getpwuid
#define getpwent my_getpwent
#define endpwent my_endpwent
#define setpwent my_endpwent

/* Our own stat_t substitute, since we play with st_dev and st_ino -
 * we want atomic types so Unix-bound code which compares these fields
 * for two files will work most of the time under VMS.
 * N.B. 1. The st_ino hack assumes that sizeof(unsigned short[3]) ==
 * sizeof(unsigned) + sizeof(unsigned short).  We can't use a union type
 * to map the unsigned int we want and the unsigned short[3] the CRTL
 * returns into the same member, since gcc has different ideas than DECC
 * and VAXC about sizing union types.
 * N.B 2. The routine cando() in vms.c assumes that &stat.st_ino is the
 * address of a FID.
 */
/* First, grab the system types, so we don't clobber them later */
#include <stat.h>
/* Since we've got to match the size of the CRTL's stat_t, we need
 * to mimic DECC's alignment settings.
 */
#if defined(__DECC) || defined(__DECCXX)
#  pragma __member_alignment __save
#  pragma __nomember_alignment
#endif
#if defined(__DECC) 
#  pragma __message __save
#  pragma __message disable (__MISALGNDSTRCT)
#  pragma __message disable (__MISALGNDMEM)
#endif
struct mystat
{
        char *st_devnam;  /* pointer to device name */
        unsigned st_ino;    /* hack - CRTL uses unsigned short[3] for */
        unsigned short rvn; /* FID (num,seq,rvn) */
        unsigned short st_mode;	/* file "mode" i.e. prot, dir, reg, etc. */
        int	st_nlink;	/* for compatibility - not really used */
        unsigned st_uid;	/* from ACP - QIO uic field */
        unsigned short st_gid;	/* group number extracted from st_uid */
        dev_t   st_rdev;	/* for compatibility - always zero */
        off_t   st_size;	/* file size in bytes */
        unsigned st_atime;	/* file access time; always same as st_mtime */
        unsigned st_mtime;	/* last modification time */
        unsigned st_ctime;	/* file creation time */
        char	st_fab_rfm;	/* record format */
        char	st_fab_rat;	/* record attributes */
        char	st_fab_fsz;	/* fixed header size */
        unsigned st_dev;	/* encoded device name */
        /* Pad struct out to integral number of longwords, since DECC 5.6/VAX
         * has a bug in dealing with offsets in structs in which are embedded
         * other structs whose size is an odd number of bytes.  (An even
         * number of bytes is enough to make it happy, but we go for natural
         * alignment anyhow.)
         */
        char	st_fill1[sizeof(void *) - (3*sizeof(unsigned short) + 3*sizeof(char))%sizeof(void *)];
};
typedef unsigned mydev_t;
typedef unsigned myino_t;
#ifndef DONT_MASK_RTL_CALLS  /* defined for vms.c so we can see RTL calls */
#  ifdef stat
#    undef stat
#  endif
#  define stat mystat
#  define dev_t mydev_t
#  define ino_t myino_t
#endif
#if defined(__DECC) || defined(__DECCXX)
#  pragma __member_alignment __restore
#endif
#if defined(__DECC) 
#  pragma __message __restore
#endif
/* Cons up a 'delete' bit for testing access */
#define S_IDUSR (S_IWUSR | S_IXUSR)
#define S_IDGRP (S_IWGRP | S_IXGRP)
#define S_IDOTH (S_IWOTH | S_IXOTH)

/* Prototypes for functions unique to vms.c.  Don't include replacements
 * for routines in the mainline source files excluded by #ifndef VMS;
 * their prototypes are already in proto.h.
 *
 * In order to keep Gen_ShrFls.Pl happy, functions which are to be made
 * available to images linked to PerlShr.Exe must be declared between the
 * __VMS_PROTOTYPES__ and __VMS_SEPYTOTORP__ lines, and must be in the form
 *    <data type><TAB>name<WHITESPACE>_((<prototype args>));
 */

#ifdef NO_PERL_TYPEDEFS
  /* We don't have Perl typedefs available (e.g. when building a2p), so
     we fake them here.  N.B.  There is *no* guarantee that the faked
     prototypes will actually match the real routines.  If you want to
     call Perl routines, include perl.h to get the real typedefs.  */
#  ifndef bool
#    define bool int
#    define __MY_BOOL_TYPE_FAKE
#  endif
#  ifndef I32
#    define I32  int
#    define __MY_I32_TYPE_FAKE
#  endif
#  ifndef SV
#    define SV   void   /* Since we only see SV * in prototypes */
#    define __MY_SV_TYPE_FAKE
#  endif
#endif

void	prime_env_iter _((void));
void	init_os_extras _(());
/* prototype section start marker; `typedef' passes through cpp */
typedef char  __VMS_PROTOTYPES__;
int	my_trnlnm _((char *, char *, unsigned long int));
char *	my_getenv _((char *));
char *	my_crypt _((const char *, const char *));
Pid_t	my_waitpid _((Pid_t, int *, int));
char *	my_gconvert _((double, int, int, char *));
int	do_rmdir _((char *));
int	kill_file _((char *));
int	my_mkdir _((char *, Mode_t));
int	my_utime _((char *, struct utimbuf *));
char *	rmsexpand _((char *, char *, char *, unsigned));
char *	rmsexpand_ts _((char *, char *, char *, unsigned));
char *	fileify_dirspec _((char *, char *));
char *	fileify_dirspec_ts _((char *, char *));
char *	pathify_dirspec _((char *, char *));
char *	pathify_dirspec_ts _((char *, char *));
char *	tounixspec _((char *, char *));
char *	tounixspec_ts _((char *, char *));
char *	tovmsspec _((char *, char *));
char *	tovmsspec_ts _((char *, char *));
char *	tounixpath _((char *, char *));
char *	tounixpath_ts _((char *, char *));
char *	tovmspath _((char *, char *));
char *	tovmspath_ts _((char *, char *));
void	vms_image_init _((int *, char ***));
DIR *	opendir _((char *));
struct dirent *	readdir _((DIR *));
long	telldir _((DIR *));
void	seekdir _((DIR *, long));
void	closedir _((DIR *));
void	vmsreaddirversions _((DIR *, int));
struct tm *	my_gmtime _((const time_t *));
struct tm *	my_localtime _((const time_t *));
time_t	my_time _((time_t *));
#ifdef HOMEGROWN_POSIX_SIGNALS
int     my_sigemptyset _((sigset_t *));
int     my_sigfillset  _((sigset_t *));
int     my_sigaddset   _((sigset_t *, int));
int     my_sigdelset   _((sigset_t *, int));
int     my_sigismember _((sigset_t *, int));
int     my_sigprocmask _((int, sigset_t *, sigset_t *));
#endif
I32	cando_by_name _((I32, I32, char *));
int	flex_fstat _((int, Stat_t *));
int	flex_stat _((char *, Stat_t *));
int	trim_unixpath _((char *, char*, int));
int	my_vfork _(());
bool	vms_do_aexec _((SV *, SV **, SV **));
bool	vms_do_exec _((char *));
unsigned long int	do_aspawn _((void *, void **, void **));
unsigned long int	do_spawn _((char *));
int	my_fwrite _((void *, size_t, size_t, FILE *));
int	my_flush _((FILE *));
FILE *	my_binmode _((FILE *, char));
struct passwd *	my_getpwnam _((char *name));
struct passwd *	my_getpwuid _((Uid_t uid));
struct passwd *	my_getpwent _(());
void	my_endpwent _(());
char *	my_getlogin _(());
int	rmscopy _((char *, char *, int));
typedef char __VMS_SEPYTOTORP__;
/* prototype section end marker; `typedef' passes through cpp */

#ifdef NO_PERL_TYPEDEFS  /* We'll try not to scramble later files */
#  ifdef __MY_BOOL_TYPE_FAKE
#    undef bool
#    undef __MY_BOOL_TYPE_FAKE
#  endif
#  ifdef __MY_I32_TYPE_FAKE
#    undef I32
#    undef __MY_I32_TYPE_FAKE
#  endif
#  ifdef __MY_SV_TYPE_FAKE
#    undef SV
#    undef __MY_SV_TYPE_FAKE
#  endif
#endif

#ifndef VMS_DO_SOCKETS
/* This relies on tricks in perl.h to pick up that these manifest constants
 * are undefined and set up conversion routines.  It will then redefine
 * these manifest constants, so the actual values will match config.h
 */
#undef HAS_HTONS
#undef HAS_NTOHS
#undef HAS_HTONL
#undef HAS_NTOHL
#endif

#define TMPPATH "sys$scratch:perl-eXXXXXX"

#endif  /* __vmsish_h_included */
