/*  vmsish.h
 *
 * VMS-specific C header file for perl5.
 *
 * Last revised: 01-Oct-1995 by Charles Bailey  bailey@genetics.upenn.edu
 * Version: 5.1.6
 */

#ifndef __vmsish_h_included
#define __vmsish_h_included

#include <descrip.h> /* for dirent struct definitions */
#include <libdef.h>  /* status codes for various places */
#include <rmsdef.h>  /* at which errno and vaxc$errno are */
#include <ssdef.h>   /* explicitly set in the perl source code */

/* Suppress compiler warnings from DECC for VMS-specific extensions:
 * GLOBALEXT, NOSHAREEXT: global[dr]ef declarations
 * ADDRCONSTEXT: initialization of data with non-constant values
 *               (e.g. pointer fields of descriptors)
 */
#ifdef __DECC
#  pragma message disable (GLOBALEXT,NOSHAREEXT,ADDRCONSTEXT)
#endif

/* Suppress compiler warnings from DECC for VMS-specific extensions:
 * GLOBALEXT, NOSHAREEXT: global[dr]ef declarations
 * ADDRCONSTEXT,NEEDCONSTEXT: initialization of data with non-constant values
 *                            (e.g. pointer fields of descriptors)
 */
#ifdef __DECC
#  pragma message disable (GLOBALEXT,NOSHAREEXT,ADDRCONSTEXT,NEEDCONSTEXT)
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

/* Our own contribution to PerlShr's global symbols . . . */
#ifdef EMBED
#  define my_trnlnm		Perl_my_trnlnm
#  define my_getenv		Perl_my_getenv
#  define my_crypt		Perl_my_crypt
#  define waitpid		Perl_waitpid
#  define my_gconvert		Perl_my_gconvert
#  define do_rmdir		Perl_do_rmdir
#  define kill_file		Perl_kill_file
#  define my_utime		Perl_my_utime
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
#  define getredirection	Perl_getredirection
#  define opendir		Perl_opendir
#  define readdir		Perl_readdir
#  define telldir		Perl_telldir
#  define seekdir		Perl_seekdir
#  define closedir		Perl_closedir
#  define vmsreaddirversions	Perl_vmsreaddirversions
#  define getredirection	Perl_getredirection
#  define my_gmtime		Perl_my_gmtime
#  define cando_by_name		Perl_cando_by_name
#  define flex_fstat		Perl_flex_fstat
#  define flex_stat		Perl_flex_stat
#  define trim_unixpath		Perl_trim_unixpath
#  define vms_do_aexec		Perl_vms_do_aexec
#  define vms_do_exec		Perl_vms_do_exec
#  define do_aspawn		Perl_do_aspawn
#  define do_spawn		Perl_do_spawn
#  define my_fwrite		Perl_my_fwrite
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

/*  The VMS C RTL has vfork() but not fork().  Both actually work in a way
 *  that's somewhere between Unix vfork() and VMS lib$spawn(), so it's
 *  probably not a good idea to use them much.  That said, we'll try to
 *  use vfork() in either case.
 */
#define fork vfork

/* Macros to set errno using the VAX thread-safe calls, if present */
#if (defined(__DECC) || defined(__DECCXX)) && !defined(__ALPHA)
#  define set_errno(v)      (cma$tis_errno_set_value(v))
#  define set_vaxc_errno(v) (vaxc$errno = (v))
#else
#  define set_errno(v)      (errno = (v))
#  define set_vaxc_errno(v) (vaxc$errno = (v))
#endif

/* Handy way to vet calls to VMS system services and RTL routines. */
#define _ckvmssts(call) STMT_START { register unsigned long int __ckvms_sts; \
  if (!((__ckvms_sts=(call))&1)) { \
  set_errno(EVMSERR); set_vaxc_errno(__ckvms_sts); \
  croak("Fatal VMS error (status=%d) at %s, line %d", \
  __ckvms_sts,__FILE__,__LINE__); } } STMT_END

#ifdef VMS_DO_SOCKETS
#include "sockadapt.h"
#endif

#define BIT_BUCKET "_NLA0:"
#define PERL_SYS_INIT(c,v)  getredirection((c),(v))
#define PERL_SYS_TERM()
#define dXSUB_SYS int dummy
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
 *	This symbol, if defined, indicates that the getgrnam(),
 *	getgrgid(), and getgrent() routines are available to 
 *	get group entries.
 */
#undef HAS_GROUP		/**/

/* HAS_PASSWD
 *	This symbol, if defined, indicates that the getpwnam(),
 *	getpwuid(), and getpwent() routines are available to 
 *	get password entries.
 */
#define HAS_PASSWD		/**/

#define HAS_KILL
#define HAS_WAIT
  
/*
 * fwrite1() should be a routine with the same calling sequence as fwrite(),
 * but which outputs all of the bytes requested as a single stream (unlike
 * fwrite() itself, which on some systems outputs several distinct records
 * if the number_of_items parameter is >1).
 */
#define fwrite1 my_fwrite

/* Use our own rmdir() */
#define rmdir(name) do_rmdir(name)

/* Assorted fiddling with sigs . . . */
# include <signal.h>
#define ABORT() abort()

/* Used with our my_utime() routine in vms.c */
struct utimbuf {
  time_t actime;
  time_t modtime;
};
#define utime my_utime

/* This is what times() returns, but <times.h> calls it tbuffer_t on VMS */

struct tms {
  clock_t tms_utime;    /* user time */
  clock_t tms_stime;    /* system time - always 0 on VMS */
  clock_t tms_cutime;   /* user time, children */
  clock_t tms_cstime;   /* system time, children - always 0 on VMS */
};

/* Prior to VMS 7.0, the CRTL gmtime() routine was a stub which always
 * returned NULL.  Substitute our own routine, which uses the logical
 * SYS$TIMEZONE_DIFFERENTIAL, whcih the native UTC support routines
 * in VMS 6.0 or later use.*
 */
#define gmtime(t) my_gmtime(t)

/* VMS doesn't use a real sys_nerr, but we need this when scanning for error
 * messages in text strings . . .
 */

#define sys_nerr EVMSERR  /* EVMSERR is as high as we can go. */

/* Look up new %ENV values on the fly */
#define DYNAMIC_ENV_FETCH 1
#define ENV_HV_NAME "%EnV%VmS%"

/* Thin jacket around cuserid() tomatch Unix' calling sequence */
#define getlogin my_getlogin

/* Ditto for sys$hash_passwrod() . . . */
#define crypt  my_crypt

/* Use our own stat() clones, which handle Unix-style directory names */
#define Stat(name,bufptr) flex_stat(name,bufptr)
#define Fstat(fd,bufptr) flex_fstat(fd,bufptr)

/* By default, flush data all the way to disk, not just to RMS buffers */
#define Fflush(fp) ((fflush(fp) || fsync(fileno(fp))) ? EOF : 0)

/* Setup for the dirent routines:
 * opendir(), closedir(), readdir(), seekdir(), telldir(), and
 * vmsreaddirversions(), and preprocessor stuff on which these depend:
 *    Written by Rich $alz, <rsalz@bbn.com> in August, 1990.
 *    This code has no copyright.
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
};
#define stat mystat
typedef unsigned mydev_t;
#define dev_t mydev_t
typedef unsigned myino_t;
#define ino_t myino_t
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
/* prototype section start marker; `typedef' passes through cpp */
typedef char  __VMS_PROTOTYPES__;
int	my_trnlnm _((char *, char *, unsigned long int));
char *	my_getenv _((char *));
char *	my_crypt _((const char *, const char *));
unsigned long int	waitpid _((unsigned long int, int *, int));
char *	my_gconvert _((double, int, int, char *));
int	do_rmdir _((char *));
int	kill_file _((char *));
int	my_utime _((char *, struct utimbuf *));
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
void	getredirection _(());
DIR *	opendir _((char *));
struct dirent *	readdir _((DIR *));
long	telldir _((DIR *));
void	seekdir _((DIR *, long));
void	closedir _((DIR *));
void	vmsreaddirversions _((DIR *, int));
void	getredirection _((int *, char ***));
struct tm *my_gmtime _((const time_t *));
I32	cando_by_name _((I32, I32, char *));
int	flex_fstat _((int, struct stat *));
int	flex_stat _((char *, struct stat *));
int	trim_unixpath _((char *, char*));
bool	vms_do_aexec _((SV *, SV **, SV **));
bool	vms_do_exec _((char *));
unsigned long int	do_aspawn _((SV *, SV **, SV **));
unsigned long int	do_spawn _((char *));
int	my_fwrite _((void *, size_t, size_t, FILE *));
struct passwd *	my_getpwnam _((char *name));
struct passwd *	my_getpwuid _((Uid_t uid));
struct passwd *	my_getpwent _(());
void	my_endpwent _(());
char *	my_getlogin _(());
int	rmscopy _((char *, char *, int));
void	init_os_extras _(());
typedef char __VMS_SEPYTOTORP__;
/* prototype section end marker; `typedef' passes through cpp */

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
