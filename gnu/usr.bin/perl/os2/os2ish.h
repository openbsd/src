#include <signal.h>

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

#define HAS_KILL
#define HAS_WAIT
#define HAS_DLERROR
#define HAS_WAITPID_RUNTIME (_emx_env & 0x200)

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
#define ALTERNATE_SHEBANG "extproc "

#ifndef SIGABRT
#    define SIGABRT SIGILL
#endif
#ifndef SIGILL
#    define SIGILL 6         /* blech */
#endif
#define ABORT() kill(PerlProc_getpid(),SIGABRT);

#define BIT_BUCKET "/dev/nul"  /* Will this work? */

/* Apparently TCPIPV4 defines may be included even with only IAK present */

#if !defined(NO_TCPIPV4) && !defined(TCPIPV4)
#  define TCPIPV4
#  define TCPIPV4_FORCED		/* Just in case */
#endif

#if defined(I_SYS_UN) && !defined(TCPIPV4)
/* It is not working without TCPIPV4 defined. */
# undef I_SYS_UN
#endif 

#ifdef USE_THREADS

#define do_spawn(a)      os2_do_spawn(aTHX_ (a))
#define do_aspawn(a,b,c) os2_do_aspawn(aTHX_ (a),(b),(c))

#define OS2_ERROR_ALREADY_POSTED 299	/* Avoid os2.h */

extern int rc;

#define MUTEX_INIT(m) \
    STMT_START {						\
	int rc;							\
	if ((rc = _rmutex_create(m,0)))				\
	    Perl_croak_nocontext("panic: MUTEX_INIT: rc=%i", rc);	\
    } STMT_END
#define MUTEX_LOCK(m) \
    STMT_START {						\
	int rc;							\
	if ((rc = _rmutex_request(m,_FMR_IGNINT)))		\
	    Perl_croak_nocontext("panic: MUTEX_LOCK: rc=%i", rc);	\
    } STMT_END
#define MUTEX_UNLOCK(m) \
    STMT_START {						\
	int rc;							\
	if ((rc = _rmutex_release(m)))				\
	    Perl_croak_nocontext("panic: MUTEX_UNLOCK: rc=%i", rc);	\
    } STMT_END
#define MUTEX_DESTROY(m) \
    STMT_START {						\
	int rc;							\
	if ((rc = _rmutex_close(m)))				\
	    Perl_croak_nocontext("panic: MUTEX_DESTROY: rc=%i", rc);	\
    } STMT_END

#define COND_INIT(c) \
    STMT_START {						\
	int rc;							\
	if ((rc = DosCreateEventSem(NULL,c,0,0)))		\
	    Perl_croak_nocontext("panic: COND_INIT: rc=%i", rc);	\
    } STMT_END
#define COND_SIGNAL(c) \
    STMT_START {						\
	int rc;							\
	if ((rc = DosPostEventSem(*(c))) && rc != OS2_ERROR_ALREADY_POSTED)\
	    Perl_croak_nocontext("panic: COND_SIGNAL, rc=%ld", rc);	\
    } STMT_END
#define COND_BROADCAST(c) \
    STMT_START {						\
	int rc;							\
	if ((rc = DosPostEventSem(*(c))) && rc != OS2_ERROR_ALREADY_POSTED)\
	    Perl_croak_nocontext("panic: COND_BROADCAST, rc=%i", rc);	\
    } STMT_END
/* #define COND_WAIT(c, m) \
    STMT_START {						\
	if (WaitForSingleObject(*(c),INFINITE) == WAIT_FAILED)	\
	    Perl_croak_nocontext("panic: COND_WAIT");		\
    } STMT_END
*/
#define COND_WAIT(c, m) os2_cond_wait(c,m)

#define COND_WAIT_win32(c, m) \
    STMT_START {						\
	int rc;							\
	if ((rc = SignalObjectAndWait(*(m),*(c),INFINITE,FALSE)))	\
	    Perl_croak_nocontext("panic: COND_WAIT");			\
	else							\
	    MUTEX_LOCK(m);					\
    } STMT_END
#define COND_DESTROY(c) \
    STMT_START {						\
	int rc;							\
	if ((rc = DosCloseEventSem(*(c))))			\
	    Perl_croak_nocontext("panic: COND_DESTROY, rc=%i", rc);	\
    } STMT_END
/*#define THR ((struct thread *) TlsGetValue(PL_thr_key))
*/

#ifdef USE_SLOW_THREAD_SPECIFIC
#  define pthread_getspecific(k)	(*_threadstore())
#  define pthread_setspecific(k,v)	(*_threadstore()=v,0)
#  define pthread_key_create(keyp,flag)	(*keyp=_gettid(),0)
#else /* USE_SLOW_THREAD_SPECIFIC */
#  define pthread_getspecific(k)	(*(k))
#  define pthread_setspecific(k,v)	(*(k)=(v),0)
#  define pthread_key_create(keyp,flag)			\
	( DosAllocThreadLocalMemory(1,(U32*)keyp)	\
	  ? Perl_croak_nocontext("LocalMemory"),1	\
	  : 0						\
	)
#endif /* USE_SLOW_THREAD_SPECIFIC */
#define pthread_key_delete(keyp)
#define pthread_self()			_gettid()
#define YIELD				DosSleep(0)

#ifdef PTHREADS_INCLUDED		/* For ./x2p stuff. */
int pthread_join(pthread_t tid, void **status);
int pthread_detach(pthread_t tid);
int pthread_create(pthread_t *tid, const pthread_attr_t *attr,
		   void *(*start_routine)(void*), void *arg);
#endif /* PTHREAD_INCLUDED */

#define THREADS_ELSEWHERE

#else /* USE_THREADS */

#define do_spawn(a)      os2_do_spawn(a)
#define do_aspawn(a,b,c) os2_do_aspawn((a),(b),(c))

#endif /* USE_THREADS */
 
void Perl_OS2_init(char **);

/* XXX This code hideously puts env inside: */

#ifdef PERL_CORE
#  define PERL_SYS_INIT3(argcp, argvp, envp) STMT_START {	\
    _response(argcp, argvp);			\
    _wildcard(argcp, argvp);			\
    Perl_OS2_init(*envp);	} STMT_END
#  define PERL_SYS_INIT(argcp, argvp) STMT_START {	\
    _response(argcp, argvp);			\
    _wildcard(argcp, argvp);			\
    Perl_OS2_init(NULL);	} STMT_END
#else  /* Compiling embedded Perl or Perl extension */
#  define PERL_SYS_INIT3(argcp, argvp, envp) STMT_START {	\
    Perl_OS2_init(*envp);	} STMT_END
#  define PERL_SYS_INIT(argcp, argvp) STMT_START {	\
    Perl_OS2_init(NULL);	} STMT_END
#endif

#ifndef __EMX__
#  define PERL_CALLCONV _System
#endif

#define PERL_SYS_TERM()		MALLOC_TERM

/* #define PERL_SYS_TERM() STMT_START {	\
    if (Perl_HAB_set) WinTerminate(Perl_hab);	} STMT_END */

#define dXSUB_SYS OS2_XS_init()

#ifdef PERL_IS_AOUT
/* #  define HAS_FORK */
/* #  define HIDEMYMALLOC */
/* #  define PERL_SBRK_VIA_MALLOC */ /* gets off-page sbrk... */
#else /* !PERL_IS_AOUT */
#  ifndef PERL_FOR_X2P
#    ifdef EMX_BAD_SBRK
#      define USE_PERL_SBRK
#    endif 
#  else
#    define PerlIO FILE
#  endif 
#  define SYSTEM_ALLOC(a) sys_alloc(a)

void *sys_alloc(int size);

#endif /* !PERL_IS_AOUT */
#if !defined(PERL_CORE) && !defined(PerlIO) /* a2p */
#  define PerlIO FILE
#endif 

/* os2ish is used from a2p/a2p.h without pTHX/pTHX_ first being
 * defined.  Hack around this to get us to compile.
*/
#ifdef PTHX_UNUSED
# ifndef pTHX
#  define pTHX
# endif
# ifndef pTHX_
#  define pTHX_
# endif
#endif

#define TMPPATH1 "plXXXXXX"
extern char *tmppath;
PerlIO *my_syspopen(pTHX_ char *cmd, char *mode);
/* Cannot prototype with I32 at this point. */
int my_syspclose(PerlIO *f);
FILE *my_tmpfile (void);
char *my_tmpnam (char *);
int my_mkdir (__const__ char *, long);
int my_rmdir (__const__ char *);

#undef L_tmpnam
#define L_tmpnam MAXPATHLEN

#define tmpfile	my_tmpfile
#define tmpnam	my_tmpnam
#define isatty	_isterm
#define rand	random
#define srand	srandom
#define strtoll	_strtoll
#define strtoull	_strtoull

/*
 * fwrite1() should be a routine with the same calling sequence as fwrite(),
 * but which outputs all of the bytes requested as a single stream (unlike
 * fwrite() itself, which on some systems outputs several distinct records
 * if the number_of_items parameter is >1).
 */
#define fwrite1 fwrite

#define my_getenv(var) getenv(var)
#define flock	my_flock
#define rmdir	my_rmdir
#define mkdir	my_mkdir

void *emx_calloc (size_t, size_t);
void emx_free (void *);
void *emx_malloc (size_t);
void *emx_realloc (void *, size_t);

/*****************************************************************************/

#include <stdlib.h>	/* before the following definitions */
#include <unistd.h>	/* before the following definitions */

#define chdir	_chdir2
#define getcwd	_getcwd2

/* This guy is needed for quick stdstd  */

#if defined(USE_STDIO_PTR) && defined(STDIO_PTR_LVALUE) && defined(STDIO_CNT_LVALUE)
	/* Perl uses ungetc only with successful return */
#  define ungetc(c,fp) \
	(FILE_ptr(fp) > FILE_base(fp) && c == (int)*(FILE_ptr(fp) - 1) \
	 ? (--FILE_ptr(fp), ++FILE_cnt(fp), (int)c) : ungetc(c,fp))
#endif

/* ctermid is missing from emx0.9d */
char *ctermid(char *s);

#define OP_BINARY O_BINARY

#define OS2_STAT_HACK 1
#if OS2_STAT_HACK

#define Stat(fname,bufptr) os2_stat((fname),(bufptr))
#define Fstat(fd,bufptr)   fstat((fd),(bufptr))
#define Fflush(fp)         fflush(fp)
#define Mkdir(path,mode)   mkdir((path),(mode))

#undef S_IFBLK
#undef S_ISBLK
#define S_IFBLK		0120000
#define S_ISBLK(mode)	(((mode) & S_IFMT) == S_IFBLK)

#else

#define Stat(fname,bufptr) stat((fname),(bufptr))
#define Fstat(fd,bufptr)   fstat((fd),(bufptr))
#define Fflush(fp)         fflush(fp)
#define Mkdir(path,mode)   mkdir((path),(mode))

#endif

/* With SD386 it is impossible to debug register variables. */
#if !defined(PERL_IS_AOUT) && defined(DEBUGGING) && !defined(register)
#  define register
#endif

/* Our private OS/2 specific data. */

typedef struct OS2_Perl_data {
  unsigned long flags;
  unsigned long phab;
  int (*xs_init)();
  unsigned long rc;
  unsigned long severity;
  unsigned long	phmq;			/* Handle to message queue */
  unsigned long	phmq_refcnt;
  unsigned long	phmq_servers;
  unsigned long	initial_mode;		/* VIO etc. mode we were started in */
} OS2_Perl_data_t;

extern OS2_Perl_data_t OS2_Perl_data;

#define Perl_hab		((HAB)OS2_Perl_data.phab)
#define Perl_rc			(OS2_Perl_data.rc)
#define Perl_severity		(OS2_Perl_data.severity)
#define errno_isOS2		12345678
#define errno_isOS2_set		12345679
#define OS2_Perl_flags	(OS2_Perl_data.flags)
#define Perl_HAB_set_f	1
#define Perl_HAB_set	(OS2_Perl_flags & Perl_HAB_set_f)
#define set_Perl_HAB_f	(OS2_Perl_flags |= Perl_HAB_set_f)
#define set_Perl_HAB(h) (set_Perl_HAB_f, Perl_hab = h)
#define _obtain_Perl_HAB (init_PMWIN_entries(),				\
			  Perl_hab = (*PMWIN_entries.Initialize)(0),	\
			  set_Perl_HAB_f, Perl_hab)
#define perl_hab_GET()	(Perl_HAB_set ? Perl_hab : _obtain_Perl_HAB)
#define Acquire_hab()	perl_hab_GET()
#define Perl_hmq	((HMQ)OS2_Perl_data.phmq)
#define Perl_hmq_refcnt	(OS2_Perl_data.phmq_refcnt)
#define Perl_hmq_servers	(OS2_Perl_data.phmq_servers)
#define Perl_os2_initial_mode	(OS2_Perl_data.initial_mode)

unsigned long Perl_hab_GET();
unsigned long Perl_Register_MQ(int serve);
void	Perl_Deregister_MQ(int serve);
int	Perl_Serve_Messages(int force);
/* Cannot prototype with I32 at this point. */
int	Perl_Process_Messages(int force, long *cntp);
char	*os2_execname(pTHX);

struct _QMSG;
struct PMWIN_entries_t {
    unsigned long (*Initialize)( unsigned long fsOptions );
    unsigned long (*CreateMsgQueue)(unsigned long hab, long cmsg);
    int (*DestroyMsgQueue)(unsigned long hmq);
    int (*PeekMsg)(unsigned long hab, struct _QMSG *pqmsg,
		   unsigned long hwndFilter, unsigned long msgFilterFirst,
		   unsigned long msgFilterLast, unsigned long fl);
    int (*GetMsg)(unsigned long hab, struct _QMSG *pqmsg,
		  unsigned long hwndFilter, unsigned long msgFilterFirst,
		  unsigned long msgFilterLast);
    void * (*DispatchMsg)(unsigned long hab, struct _QMSG *pqmsg);
    unsigned long (*GetLastError)(unsigned long hab);
    unsigned long (*CancelShutdown)(unsigned long hmq, unsigned long fCancelAlways);
};
extern struct PMWIN_entries_t PMWIN_entries;
void init_PMWIN_entries(void);

#define perl_hmq_GET(serve)	Perl_Register_MQ(serve)
#define perl_hmq_UNSET(serve)	Perl_Deregister_MQ(serve)

#define OS2_XS_init() (*OS2_Perl_data.xs_init)(aTHX)

#if _EMX_CRT_REV_ >= 60
# define os2_setsyserrno(rc)	(Perl_rc = rc, errno = errno_isOS2_set, \
				_setsyserrno(rc))
#else
# define os2_setsyserrno(rc)	(Perl_rc = rc, errno = errno_isOS2)
#endif

/* The expressions below return true on error. */
/* INCL_DOSERRORS needed. rc should be declared outside. */
#define CheckOSError(expr) (!(rc = (expr)) ? 0 : (FillOSError(rc), 1))
/* INCL_WINERRORS needed. */
#define SaveWinError(expr) ((expr) ? : (FillWinError, 0))
#define CheckWinError(expr) ((expr) ? 0: (FillWinError, 1))
#define FillOSError(rc) (os2_setsyserrno(rc),				\
			Perl_severity = SEVERITY_ERROR) 

/* At this moment init_PMWIN_entries() should be a nop (WinInitialize should
   be called already, right?), so we do not risk stepping over our own error */
#define FillWinError (	init_PMWIN_entries(),				\
			Perl_rc=(*PMWIN_entries.GetLastError)(perl_hab_GET()),\
			Perl_severity = ERRORIDSEV(Perl_rc),		\
			Perl_rc = ERRORIDERROR(Perl_rc),		\
			os2_setsyserrno(Perl_rc))

#define STATIC_FILE_LENGTH 127

#define PERLLIB_MANGLE(s, n) perllib_mangle((s), (n))
char *perllib_mangle(char *, unsigned int);

char *os2error(int rc);

/* ************************************************************ */
#define Dos32QuerySysState DosQuerySysState
#define QuerySysState(flags, pid, buf, bufsz) \
	Dos32QuerySysState(flags, 0,  pid, 0, buf, bufsz)

#define QSS_PROCESS	1
#define QSS_MODULE	4
#define QSS_SEMAPHORES	2
#define QSS_FILE	8		/* Buggy until fixpack18 */
#define QSS_SHARED	16

#ifdef _OS2_H

APIRET APIENTRY Dos32QuerySysState(ULONG func,ULONG arg1,ULONG pid,
			ULONG _res_,PVOID buf,ULONG bufsz);
typedef struct {
	ULONG	threadcnt;
	ULONG	proccnt;
	ULONG	modulecnt;
} QGLOBAL, *PQGLOBAL;

typedef struct {
	ULONG	rectype;
	USHORT	threadid;
	USHORT	slotid;
	ULONG	sleepid;
	ULONG	priority;
	ULONG	systime;
	ULONG	usertime;
	UCHAR	state;
	UCHAR	_reserved1_;	/* padding to ULONG */
	USHORT	_reserved2_;	/* padding to ULONG */
} QTHREAD, *PQTHREAD;

typedef struct {
	USHORT	sfn;
	USHORT	refcnt;
	USHORT	flags1;
	USHORT	flags2;
	USHORT	accmode1;
	USHORT	accmode2;
	ULONG	filesize;
	USHORT  volhnd;
	USHORT	attrib;
	USHORT	_reserved_;
} QFDS, *PQFDS;

typedef struct qfile {
	ULONG		rectype;
	struct qfile	*next;
	ULONG		opencnt;
	PQFDS		filedata;
	char		name[1];
} QFILE, *PQFILE;

typedef struct {
	ULONG	rectype;
	PQTHREAD threads;
	USHORT	pid;
	USHORT	ppid;
	ULONG	type;
	ULONG	state;
	ULONG	sessid;
	USHORT	hndmod;
	USHORT	threadcnt;
	ULONG	privsem32cnt;
	ULONG	_reserved2_;
	USHORT	sem16cnt;
	USHORT	dllcnt;
	USHORT	shrmemcnt;
	USHORT	fdscnt;
	PUSHORT	sem16s;
	PUSHORT	dlls;
	PUSHORT	shrmems;
	PUSHORT	fds;
} QPROCESS, *PQPROCESS;

typedef struct sema {
	struct sema *next;
	USHORT	refcnt;
	UCHAR	sysflags;
	UCHAR	sysproccnt;
	ULONG	_reserved1_;
	USHORT	index;
	CHAR	name[1];
} QSEMA, *PQSEMA;

typedef struct {
	ULONG	rectype;
	ULONG	_reserved1_;
	USHORT	_reserved2_;
	USHORT	syssemidx;
	ULONG	index;
	QSEMA	sema;
} QSEMSTRUC, *PQSEMSTRUC;

typedef struct {
	USHORT	pid;
	USHORT	opencnt;
} QSEMOWNER32, *PQSEMOWNER32;

typedef struct {
	PQSEMOWNER32	own;
	PCHAR		name;
	PVOID		semrecs; /* array of associated sema's */
	USHORT		flags;
	USHORT		semreccnt;
	USHORT		waitcnt;
	USHORT		_reserved_;	/* padding to ULONG */
} QSEMSMUX32, *PQSEMSMUX32;

typedef struct {
	PQSEMOWNER32	own;
	PCHAR		name;
	PQSEMSMUX32	mux;
	USHORT		flags;
	USHORT		postcnt;
} QSEMEV32, *PQSEMEV32;

typedef struct {
	PQSEMOWNER32	own;
	PCHAR		name;
	PQSEMSMUX32	mux;
	USHORT		flags;
	USHORT		refcnt;
	USHORT		thrdnum;
	USHORT		_reserved_;	/* padding to ULONG */
} QSEMMUX32, *PQSEMMUX32;

typedef struct semstr32 {
	struct semstr *next;
	QSEMEV32 evsem;
	QSEMMUX32  muxsem;
	QSEMSMUX32 smuxsem;
} QSEMSTRUC32, *PQSEMSTRUC32;

typedef struct shrmem {
	struct shrmem *next;
	USHORT	hndshr;
	USHORT	selshr;
	USHORT	refcnt;
	CHAR	name[1];
} QSHRMEM, *PQSHRMEM;

typedef struct module {
	struct module *next;
	USHORT	hndmod;
	USHORT	type;
	ULONG	refcnt;
	ULONG	segcnt;
	PVOID	_reserved_;
	PCHAR	name;
	USHORT	modref[1];
} QMODULE, *PQMODULE;

typedef struct {
	PQGLOBAL	gbldata;
	PQPROCESS	procdata;
	PQSEMSTRUC	semadata;
	PQSEMSTRUC32	sem32data;
	PQSHRMEM	shrmemdata;
	PQMODULE	moddata;
	PVOID		_reserved2_;
	PQFILE		filedata;
} QTOPLEVEL, *PQTOPLEVEL;
/* ************************************************************ */

PQTOPLEVEL get_sysinfo(ULONG pid, ULONG flags);

#endif /* _OS2_H */

