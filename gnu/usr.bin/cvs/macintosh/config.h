/*
 * config.h --- configuration file for MacOS
 * Handbuilt for MetroWerks CodeWarrier 7 and GUSI 1.6.4
 *
 * This file lives in the CVSHOME/macintosh directory
 *
 * MDLadwig <mike@twinpeaks.prc.com> --- Nov 1995
 */

#ifdef MSL_LIBRARY
#include <sys/types.h>
#include <sys/cdefs.h>
#include <compat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#else
#include <GUSI.h>
#include <sys/errno.h>
#endif

/* Enumerated values can be trouble on 68K machines.  */
#if !__POWERPC__
#define ENUMS_CAN_BE_TROUBLE 1
#endif

/* Don't offer user (or server) the ability to try client-side gzip subprocessing */
#define NO_CLIENT_GZIP_PROCESS 1

/* Don't support signals on the mac */
#define DONT_USE_SIGNALS 1

/* Define if CR-LF conversion is broken.  */
#define BROKEN_READWRITE_CONVERSION 1

/* Define if 'patch' is not to be used.  */
#define DONT_USE_PATCH 1

/* Define if 'errno.h' is missing.  */
#define ERRNO_H_MISSING 1

/* Define if you support file names longer than 14 characters.  */
#define HAVE_LONG_FILE_NAMES 1

/* Define if utime(file, NULL) sets file's timestamp to the present.  */
#define HAVE_UTIME_NULL 1

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly. */
#define STAT_MACROS_BROKEN 1
 
/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you want CVS to be able to be a remote repository client.  */
#define CLIENT_SUPPORT 1

/* Define if you have the connect function.  */
#define HAVE_CONNECT

/* Define if you have the ftime function.  */
#define HAVE_FTIME 1

/* Define if you have the ftruncate function.  */
#undef HAVE_FTRUNCATE

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <errno.h> header file */
#define HAVE_ERRNO_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <utime.h> header file.  */
#define HAVE_UTIME_H 1

/* GUSI filesystem stuff doesn't take the last parameter (permissions).  */
#define CVS_MKDIR macos_mkdir
#define CVS_OPEN macos_open
#define CVS_CREAT macos_creat
#define CVS_FOPEN macos_fopen
#define CVS_CHDIR macos_chdir
#define CVS_ACCESS macos_access
#define CVS_OPENDIR macos_opendir
#define CVS_STAT macos_stat
#define CVS_RENAME macos_rename
#define CVS_RMDIR macos_rmdir
#define CVS_UNLINK macos_unlink
#define CVS_CHMOD macos_chmod

extern int macos_rmdir (const char *);
extern int macos_rename (const char *, const char *);
extern int macos_stat (const char *, struct stat *);
extern DIR * macos_opendir (const char *);
extern int macos_access(const char *, int);
extern int macos_chdir( const char *path );
extern FILE * macos_fopen( const char *path, const char *mode );
extern int macos_creat( const char *path, mode_t mode );
extern int macos_open( const char *path, int oflag, ... );
extern int macos_mkdir( const char *path, int oflag );
extern int macos_unlink (const char *);
extern int macos_chmod( const char *path, mode_t mode );

/* Initialize and cleanup routines */
#define SYSTEM_INITIALIZE MacOS_Initialize
#define SYSTEM_CLEANUP MacOS_Cleanup

extern void MacOS_Initialize ( int *, char *** );
extern void MacOS_Cleanup ( void );

/* Kludges from pwd.c  */
extern struct passwd *getpwnam (char *name);
extern pid_t getpid (void);

/* We have prototypes.  */
#define USE_PROTOTYPES 1

/* Compare filenames */
#define fncmp strcmp

/* Don't use rsh */
#define RSH_NOT_TRANSPARENT 1

#define START_SERVER macos_start_server
#define SHUTDOWN_SERVER macos_shutdown_server

extern void macos_start_server (int *tofd, int *fromfd,
			      char *client_user,
			      char *server_user,
			      char *server_host,
			      char *server_cvsroot);
extern void macos_shutdown_server (int to);

#ifdef MSL_LIBRARY

FILE *  fdopen(int fd, const char *type);
#define ENOTDIR				-1
#define EINVAL					-1
#define EEXIST					-1
#define ENOTEMPTY				-1
#define ENOSPC					-1

#endif /* MSL_LIBRARY */
