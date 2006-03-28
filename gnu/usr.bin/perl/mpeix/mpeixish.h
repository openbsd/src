/*
 * The following symbols are defined if your operating system supports
 * functions by that name.  All Unixes I know of support them, thus they
 * are not checked by the configuration script, but are directly defined
 * here.
 */

/* HAS_IOCTL:
 *	This symbol, if defined, indicates that the ioctl() routine is
 *	available to set I/O characteristics
 */
#define	HAS_IOCTL		/**/
 
/* HAS_UTIME:
 *	This symbol, if defined, indicates that the routine utime() is
 *	available to update the access and modification times of files.
 */
#define HAS_UTIME		/**/

/* HAS_GROUP
 *	This symbol, if defined, indicates that the getgrnam() and
 *	getgrgid() routines are available to get group entries.
 */
#define HAS_GROUP		/**/

/* HAS_PASSWD
 *	This symbol, if defined, indicates that the getpwnam() and
 *	getpwuid() routines are available to get password entries.
 */
#define HAS_PASSWD		/**/

#define HAS_KILL
#define HAS_WAIT
  
/* USEMYBINMODE
 *	This symbol, if defined, indicates that the program should
 *	use the routine my_binmode(FILE *fp, char iotype, int mode) to insure
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
 *	it as a command line if if finds the string "perl" in the first
 *	word; otherwise it's treated as the first line of code in the script.
 *	(IOW, Perl won't hand off to another interpreter via an alternate
 *	shebang sequence that might be legal Perl code.)
 */
/* #define ALTERNATE_SHEBANG "#!" / **/

#include <signal.h>

#ifndef SIGABRT
#    define SIGABRT SIGILL
#endif
#ifndef SIGILL
#    define SIGILL 6         /* blech */
#endif
#define ABORT() kill(PerlProc_getpid(),SIGABRT);

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

#ifndef PERL_SYS_INIT
#  define PERL_SYS_INIT(c,v)	PERL_FPU_INIT MALLOC_INIT
#endif

#ifndef PERL_SYS_TERM
#define PERL_SYS_TERM()		MALLOC_TERM
#endif

#define BIT_BUCKET "/dev/null"

#define dXSUB_SYS

/* pw_passwd, pw_gecos, pw_age, pw_comment exist in the struct passwd
 * but they contain uninitialized (as in "accessing them will crash perl")
 * pointers.  Stay away from them. */

#undef PWGECOS
#undef PRPASSWD
#undef PWAGE
#undef PWCOMMENT

/* various missing external function declarations */

#include <sys/ipc.h>
extern key_t ftok (char *pathname, char id);
extern char *gcvt (double value, int ndigit, char *buf);
extern int isnan (double value);
extern void srand48(long int seedval);
extern double drand48(void);
extern double erand48(unsigned short xsubi[3]);
extern long jrand48(unsigned short xsubi[3]);
extern void lcong48(unsigned short param[7]);
extern long lrand48(void);
extern long mrand48(void);
extern long nrand48(unsigned short xsubi[3]);
extern unsigned short *seed48(unsigned short seed16v[3]);

/* various missing constants -- define 'em */

#define PF_UNSPEC 0

/* declarations for wrappers in mpeix.c */

#include <time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>


extern int ftruncate(int fd, long wantsize);
extern int gettimeofday( struct timeval *tp, struct timezone *tpz );
extern int truncate(const char *pathname, off_t length);

extern int mpe_read(int filedes, void *buffer, size_t len);
extern int mpe_write(int filedes, const void *buffer, size_t len);
extern int mpe_send(int socket, const void *buffer, size_t len, int flags);
extern int mpe_sendto(int socket, const void *buffer, size_t len,
       int flags, const struct sockaddr *dest_addr,
       size_t dest_len);
extern int mpe_recv(int socket, void *buffer, size_t length, int flags);
extern int mpe_recvfrom(int socket, void *buffer, size_t length,
           int flags, struct sockaddr *address,
           size_t *address_len) ;
extern int mpe_bind(int socket, const struct sockaddr *address,
   size_t address_len);
extern int mpe_getsockname(int socket, struct sockaddr *address,
  size_t *address_len);
extern int mpe_getpeername(int socket, struct sockaddr *address, 
  size_t *address_len);

/* Replacements to fix various socket problems -- see mpeix.c */
#define fcntl mpe_fcntl
#define read mpe_read
#define write mpe_write
#define send mpe_send
#define sendto mpe_sendto
#define recv mpe_recv
#define recvfrom mpe_recvfrom
#define bind mpe_bind
#define getsockname mpe_getsockname
#define getpeername mpe_getpeername
