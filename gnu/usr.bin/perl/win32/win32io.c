
#ifdef __cplusplus
extern "C" {
#endif

#define WIN32_LEAN_AND_MEAN
#define WIN32IO_IS_STDIO
#define EXT
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <process.h>
#include <direct.h>
#include "win32iop.h"

/*
 * The following is just a basic wrapping of the stdio
 *
 *  redirected io subsystem for all XS modules
 */

static int *
dummy_errno(void)
{
    return (&(errno));
}

static char ***
dummy_environ(void)
{
    return (&(_environ));
}

/* the rest are the remapped stdio routines */
static FILE *
dummy_stderr(void)
{
    return stderr;
}

static FILE *
dummy_stdin(void)
{
    return stdin;
}

static FILE *
dummy_stdout(void)
{
    return stdout;
}

static int
dummy_globalmode(int mode)
{
    int o = _fmode;
    _fmode = mode;

    return o;
}

#if defined(_DLL) || defined(__BORLANDC__)
/* It may or may not be fixed (ok on NT), but DLL runtime
   does not export the functions used in the workround
*/
#define WIN95_OSFHANDLE_FIXED
#endif

#if defined(_WIN32) && !defined(WIN95_OSFHANDLE_FIXED) && defined(_M_IX86)

#	ifdef __cplusplus
#define EXT_C_FUNC	extern "C"
#	else
#define EXT_C_FUNC	extern
#	endif

EXT_C_FUNC int __cdecl _alloc_osfhnd(void);
EXT_C_FUNC int __cdecl _set_osfhnd(int fh, long value);
EXT_C_FUNC void __cdecl _lock_fhandle(int);
EXT_C_FUNC void __cdecl _unlock_fhandle(int);
EXT_C_FUNC void __cdecl _unlock(int);

#if	(_MSC_VER >= 1000)
typedef struct	{
    long osfhnd;    /* underlying OS file HANDLE */
    char osfile;    /* attributes of file (e.g., open in text mode?) */
    char pipech;    /* one char buffer for handles opened on pipes */
#if defined (_MT) && !defined (DLL_FOR_WIN32S)
    int lockinitflag;
    CRITICAL_SECTION lock;
#endif  /* defined (_MT) && !defined (DLL_FOR_WIN32S) */
}	ioinfo;

EXT_C_FUNC ioinfo * __pioinfo[];

#define IOINFO_L2E			5
#define IOINFO_ARRAY_ELTS	(1 << IOINFO_L2E)
#define _pioinfo(i)	(__pioinfo[i >> IOINFO_L2E] + (i & (IOINFO_ARRAY_ELTS - 1)))
#define _osfile(i)	(_pioinfo(i)->osfile)

#else	/* (_MSC_VER >= 1000) */
extern char _osfile[];
#endif	/* (_MSC_VER >= 1000) */

#define FOPEN			0x01	/* file handle open */
#define FAPPEND			0x20	/* file handle opened O_APPEND */
#define FDEV			0x40	/* file handle refers to device */
#define FTEXT			0x80	/* file handle is in text mode */

#define _STREAM_LOCKS   26		/* Table of stream locks */
#define _LAST_STREAM_LOCK  (_STREAM_LOCKS+_NSTREAM_-1)	/* Last stream lock */
#define _FH_LOCKS          (_LAST_STREAM_LOCK+1)	/* Table of fh locks */

/***
*int _patch_open_osfhandle(long osfhandle, int flags) - open C Runtime file handle
*
*Purpose:
*       This function allocates a free C Runtime file handle and associates
*       it with the Win32 HANDLE specified by the first parameter. This is a
*		temperary fix for WIN95's brain damage GetFileType() error on socket
*		we just bypass that call for socket
*
*Entry:
*       long osfhandle - Win32 HANDLE to associate with C Runtime file handle.
*       int flags      - flags to associate with C Runtime file handle.
*
*Exit:
*       returns index of entry in fh, if successful
*       return -1, if no free entry is found
*
*Exceptions:
*
*******************************************************************************/

int
my_open_osfhandle(long osfhandle, int flags)
{
    int fh;
    char fileflags;		/* _osfile flags */

    /* copy relevant flags from second parameter */
    fileflags = FDEV;

    if(flags & O_APPEND)
	fileflags |= FAPPEND;

    if(flags & O_TEXT)
	fileflags |= FTEXT;

    /* attempt to allocate a C Runtime file handle */
    if((fh = _alloc_osfhnd()) == -1) {
	errno = EMFILE;		/* too many open files */
	_doserrno = 0L;		/* not an OS error */
	return -1;		/* return error to caller */
    }

    /* the file is open. now, set the info in _osfhnd array */
    _set_osfhnd(fh, osfhandle);

    fileflags |= FOPEN;		/* mark as open */

#if (_MSC_VER >= 1000)
    _osfile(fh) = fileflags;	/* set osfile entry */
    _unlock_fhandle(fh);
#else
    _osfile[fh] = fileflags;	/* set osfile entry */
    _unlock(fh+_FH_LOCKS);		/* unlock handle */
#endif

    return fh;			/* return handle */
}
#else

int __cdecl
my_open_osfhandle(long osfhandle, int flags)
{
    return _open_osfhandle(osfhandle, flags);
}
#endif	/* _M_IX86 */

long
my_get_osfhandle( int filehandle )
{
    return _get_osfhandle(filehandle);
}

#ifdef __BORLANDC__
#define _chdir chdir
#endif

/* simulate flock by locking a range on the file */


#define LK_ERR(f,i)	((f) ? (i = 0) : (errno = GetLastError()))
#define LK_LEN		0xffff0000

int
my_flock(int fd, int oper)
{
    OVERLAPPED o;
    int i = -1;
    HANDLE fh;

    fh = (HANDLE)my_get_osfhandle(fd);
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

EXT int		my_fclose(FILE *pf);

#ifdef PERLDLL
__declspec(dllexport)
#endif
WIN32_IOSUBSYSTEM	win32stdio = {
    12345678L,		/* begin of structure; */
    dummy_errno,	/* (*pfunc_errno)(void); */
    dummy_environ,	/* (*pfunc_environ)(void); */
    dummy_stdin,	/* (*pfunc_stdin)(void); */
    dummy_stdout,	/* (*pfunc_stdout)(void); */
    dummy_stderr,	/* (*pfunc_stderr)(void); */
    ferror,		/* (*pfunc_ferror)(FILE *fp); */
    feof,		/* (*pfunc_feof)(FILE *fp); */
    strerror,		/* (*strerror)(int e); */
    vfprintf,		/* (*pfunc_vfprintf)(FILE *pf, const char *format, va_list arg); */
    vprintf,		/* (*pfunc_vprintf)(const char *format, va_list arg); */
    fread,		/* (*pfunc_fread)(void *buf, size_t size, size_t count, FILE *pf); */
    fwrite,		/* (*pfunc_fwrite)(void *buf, size_t size, size_t count, FILE *pf); */
    fopen,		/* (*pfunc_fopen)(const char *path, const char *mode); */
    fdopen,		/* (*pfunc_fdopen)(int fh, const char *mode); */
    freopen,		/* (*pfunc_freopen)(const char *path, const char *mode, FILE *pf); */
    my_fclose,		/* (*pfunc_fclose)(FILE *pf); */
    fputs,		/* (*pfunc_fputs)(const char *s,FILE *pf); */
    fputc,		/* (*pfunc_fputc)(int c,FILE *pf); */
    ungetc,		/* (*pfunc_ungetc)(int c,FILE *pf); */
    getc,		/* (*pfunc_getc)(FILE *pf); */
    fileno,		/* (*pfunc_fileno)(FILE *pf); */
    clearerr,		/* (*pfunc_clearerr)(FILE *pf); */
    fflush,		/* (*pfunc_fflush)(FILE *pf); */
    ftell,		/* (*pfunc_ftell)(FILE *pf); */
    fseek,		/* (*pfunc_fseek)(FILE *pf,long offset,int origin); */
    fgetpos,		/* (*pfunc_fgetpos)(FILE *pf,fpos_t *p); */
    fsetpos,		/* (*pfunc_fsetpos)(FILE *pf,fpos_t *p); */
    rewind,		/* (*pfunc_rewind)(FILE *pf); */
    tmpfile,		/* (*pfunc_tmpfile)(void); */
    abort,		/* (*pfunc_abort)(void); */
    fstat,  		/* (*pfunc_fstat)(int fd,struct stat *bufptr); */
    stat,		/* (*pfunc_stat)(const char *name,struct stat *bufptr); */
    _pipe,		/* (*pfunc_pipe)( int *phandles, unsigned int psize, int textmode ); */
    _popen,		/* (*pfunc_popen)( const char *command, const char *mode ); */
    _pclose,		/* (*pfunc_pclose)( FILE *pf); */
    setmode,		/* (*pfunc_setmode)( int fd, int mode); */
    lseek,		/* (*pfunc_lseek)( int fd, long offset, int origin); */
    tell,		/* (*pfunc_tell)( int fd); */
    dup,		/* (*pfunc_dup)( int fd); */
    dup2,		/* (*pfunc_dup2)(int h1, int h2); */
    open,		/* (*pfunc_open)(const char *path, int oflag,...); */
    close,		/* (*pfunc_close)(int fd); */
    eof,		/* (*pfunc_eof)(int fd); */
    read,		/* (*pfunc_read)(int fd, void *buf, unsigned int cnt); */
    write,		/* (*pfunc_write)(int fd, const void *buf, unsigned int cnt); */
    dummy_globalmode,	/* (*pfunc_globalmode)(int mode) */
    my_open_osfhandle,
    my_get_osfhandle,
    spawnvp,
    mkdir,
    rmdir,
    chdir,
    my_flock,		/* (*pfunc_flock)(int fd, int oper) */
    execvp,
    perror,
    setbuf,
    setvbuf,
    flushall,
    fcloseall,
    fgets,
    gets,
    fgetc,
    putc,
    puts,
    getchar,
    putchar,
    fscanf,
    scanf,
    malloc,
    calloc,
    realloc,
    free,
    87654321L,		/* end of structure */
};


#ifdef __cplusplus
}
#endif

