
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	nw5iop.h
 * DESCRIPTION	:	Redefined functions for NetWare.
 * Author		:	SGP, HYAK
 * Date			:	January 2001.
 *
 */



#ifndef NW5IOP_H
#define NW5IOP_H


#ifndef START_EXTERN_C
#ifdef __cplusplus
#  define START_EXTERN_C extern "C" {
#  define END_EXTERN_C }
#  define EXTERN_C extern "C"
#else
#  define START_EXTERN_C 
#  define END_EXTERN_C 
#  define EXTERN_C
#endif
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
#  include <sys/utime.h>
#else
#  include <utime.h>
#endif

/*
 * defines for flock emulation
 */
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8


/*
 * Make this as close to original stdio as possible.
 */

/*
 * function prototypes for our own win32io layer
 */
/********CHKSGP ****/
//making DLLExport as nothing
#define DllExport	
/*******************/

START_EXTERN_C

int * 	nw_errno(void);
char *** 	nw_environ(void);

FILE*	nw_stdin(void);
FILE*	nw_stdout(void);
FILE*	nw_stderr(void);
int		nw_ferror(FILE *fp);
int		nw_feof(FILE *fp);

char*	nw_strerror(int e);

int		nw_fprintf(FILE *pf, const char *format, ...);
int		nw_printf(const char *format, ...);
int		nw_vfprintf(FILE *pf, const char *format, va_list arg);
int		nw_vprintf(const char *format, va_list arg);

size_t	nw_fread(void *buf, size_t size, size_t count, FILE *pf);
size_t	nw_fwrite(const void *buf, size_t size, size_t count, FILE *pf);
FILE*	nw_fopen(const char *path, const char *mode);
FILE*	nw_fdopen(int fh, const char *mode);
FILE*	nw_freopen(const char *path, const char *mode, FILE *pf);
int		nw_fclose(FILE *pf);

int		nw_fputs(const char *s,FILE *pf);
int		nw_fputc(int c,FILE *pf);
int		nw_ungetc(int c,FILE *pf);
int		nw_getc(FILE *pf);
int		nw_fileno(FILE *pf);
void	nw_clearerr(FILE *pf);
int		nw_fflush(FILE *pf);
long	nw_ftell(FILE *pf);
int		nw_fseek(FILE *pf,long offset,int origin);
int		nw_fgetpos(FILE *pf,fpos_t *p);
int		nw_fsetpos(FILE *pf,const fpos_t *p);
void	nw_rewind(FILE *pf);
FILE*	nw_tmpfile(void);

void	nw_abort(void);

int  	nw_stat(const char *name,struct stat *sbufptr);

FILE* nw_Popen(char* command, char* mode, int* e);
int nw_Pclose(FILE* file, int* e);
int nw_Pipe(int* a, int* e);

int		nw_rename( const char *oname, const char *newname);
//int		nw_setmode( int fd, int mode);
int		nw_setmode( FILE *fp, int mode);
long	nw_lseek( int fd, long offset, int origin);
int		nw_dup( int fd);
int		nw_dup2(int h1, int h2);
int		nw_open(const char *path, int oflag,...);
int		nw_close(int fd);
int		nw_read(int fd, void *buf, unsigned int cnt);
int		nw_write(int fd, const void *buf, unsigned int cnt);

int nw_spawnvp(int mode, char *cmdname, char **argv);

int		nw_rmdir(const char *dir);
int		nw_chdir(const char *dir);
int		nw_flock(int fd, int oper);

int nw_execv(char *cmdname, char **argv);
int nw_execvp(char *cmdname, char **argv);

void	nw_setbuf(FILE *pf, char *buf);
int		nw_setvbuf(FILE *pf, char *buf, int type, size_t size);
char*	nw_fgets(char *s, int n, FILE *pf);

int		nw_fgetc(FILE *pf);

int		nw_putc(int c, FILE *pf);

int		nw_open_osfhandle(long handle, int flags);
long	nw_get_osfhandle(int fd);

DIR*	nw_opendir(char *filename);
struct direct*	nw_readdir(DIR *dirp);
long nw_telldir(DIR *dirp);
void nw_seekdir(DIR *dirp, long loc);
void nw_rewinddir(DIR *dirp);
int		nw_closedir(DIR *dirp);

unsigned int 	nw_sleep(unsigned int);
int		nw_times(struct tms *timebuf);

int		nw_stat(const char *path, struct stat *buf);
int nw_link(const char *oldname, const char *newname);
int		nw_unlink(const char *f);
int		nw_utime(const char *f, struct utimbuf *t);
DllExport  int		nw_uname(struct utsname *n);

int		nw_wait(int *status);

int nw_waitpid(int pid, int *status, int flags);
int nw_kill(int pid, int sig);

unsigned long	nw_os_id(void);
void*	nw_dynaload(const char*filename);

int		nw_access(const char *path, int mode);
int		nw_chmod(const char *path, int mode);
int		nw_getpid(void);

char *	nw_crypt(const char *txt, const char *salt);

int nw_isatty(int fd);
char* nw_mktemp(char *Template);
int nw_chsize(int handle, long size);
END_EXTERN_C


/*
 * the following six(6) is #define in stdio.h
 */
#ifndef WIN32IO_IS_STDIO
#undef environ
#undef feof
#undef pipe
#undef pause
#undef sleep
#undef times
#undef alarm
#undef ioctl
#undef unlink
#undef utime
#undef uname
#undef wait

#ifdef __BORLANDC__
#undef ungetc
#undef getc
#undef putc
#undef getchar
#undef putchar
#undef fileno
#endif

#define environ				(*nw_environ())


#if !defined(MYMALLOC) || !defined(PERL_CORE)

#endif


#endif /* WIN32IO_IS_STDIO */
#endif /* NW5IOP_H */

