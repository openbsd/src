#ifndef WIN32IOP_H
#define WIN32IOP_H


/*
 * Make this as close to original stdio as possible.
 */

/*
 * function prototypes for our own win32io layer
 */
EXT int * 	win32_errno(void);
EXT char *** 	win32_environ(void);
EXT FILE*	win32_stdin(void);
EXT FILE*	win32_stdout(void);
EXT FILE*	win32_stderr(void);
EXT int		win32_ferror(FILE *fp);
EXT int		win32_feof(FILE *fp);
EXT char*	win32_strerror(int e);

EXT int		win32_fprintf(FILE *pf, const char *format, ...);
EXT int		win32_printf(const char *format, ...);
EXT int		win32_vfprintf(FILE *pf, const char *format, va_list arg);
EXT int		win32_vprintf(const char *format, va_list arg);
EXT size_t	win32_fread(void *buf, size_t size, size_t count, FILE *pf);
EXT size_t	win32_fwrite(const void *buf, size_t size, size_t count, FILE *pf);
EXT FILE*	win32_fopen(const char *path, const char *mode);
EXT FILE*	win32_fdopen(int fh, const char *mode);
EXT FILE*	win32_freopen(const char *path, const char *mode, FILE *pf);
EXT int		win32_fclose(FILE *pf);
EXT int		win32_fputs(const char *s,FILE *pf);
EXT int		win32_fputc(int c,FILE *pf);
EXT int		win32_ungetc(int c,FILE *pf);
EXT int		win32_getc(FILE *pf);
EXT int		win32_fileno(FILE *pf);
EXT void	win32_clearerr(FILE *pf);
EXT int		win32_fflush(FILE *pf);
EXT long	win32_ftell(FILE *pf);
EXT int		win32_fseek(FILE *pf,long offset,int origin);
EXT int		win32_fgetpos(FILE *pf,fpos_t *p);
EXT int		win32_fsetpos(FILE *pf,const fpos_t *p);
EXT void	win32_rewind(FILE *pf);
EXT FILE*	win32_tmpfile(void);
EXT void	win32_abort(void);
EXT int  	win32_fstat(int fd,struct stat *bufptr);
EXT int  	win32_stat(const char *name,struct stat *bufptr);
EXT int		win32_pipe( int *phandles, unsigned int psize, int textmode );
EXT FILE*	win32_popen( const char *command, const char *mode );
EXT int		win32_pclose( FILE *pf);
EXT int		win32_setmode( int fd, int mode);
EXT long	win32_lseek( int fd, long offset, int origin);
EXT long	win32_tell( int fd);
EXT int		win32_dup( int fd);
EXT int		win32_dup2(int h1, int h2);
EXT int		win32_open(const char *path, int oflag,...);
EXT int		win32_close(int fd);
EXT int		win32_eof(int fd);
EXT int		win32_read(int fd, void *buf, unsigned int cnt);
EXT int		win32_write(int fd, const void *buf, unsigned int cnt);
EXT int		win32_spawnvp(int mode, const char *cmdname,
			      const char *const *argv);
EXT int		win32_mkdir(const char *dir, int mode);
EXT int		win32_rmdir(const char *dir);
EXT int		win32_chdir(const char *dir);
EXT int		win32_flock(int fd, int oper);
EXT int		win32_execvp(const char *cmdname, const char *const *argv);
EXT void	win32_perror(const char *str);
EXT void	win32_setbuf(FILE *pf, char *buf);
EXT int		win32_setvbuf(FILE *pf, char *buf, int type, size_t size);
EXT int		win32_flushall(void);
EXT int		win32_fcloseall(void);
EXT char*	win32_fgets(char *s, int n, FILE *pf);
EXT char*	win32_gets(char *s);
EXT int		win32_fgetc(FILE *pf);
EXT int		win32_putc(int c, FILE *pf);
EXT int		win32_puts(const char *s);
EXT int		win32_getchar(void);
EXT int		win32_putchar(int c);
EXT void*	win32_malloc(size_t size);
EXT void*	win32_calloc(size_t numitems, size_t size);
EXT void*	win32_realloc(void *block, size_t size);
EXT void	win32_free(void *block);

/*
 * these two are win32 specific but still io related
 */
int		stolen_open_osfhandle(long handle, int flags);
long		stolen_get_osfhandle(int fd);

/*
 * defines for flock emulation
 */
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8

#include <win32io.h>	/* pull in the io sub system structure */

EXT PWIN32_IOSUBSYSTEM	SetIOSubSystem(void	*piosubsystem);
EXT PWIN32_IOSUBSYSTEM	GetIOSubSystem(void);

/*
 * the following six(6) is #define in stdio.h
 */
#ifndef WIN32IO_IS_STDIO
#undef errno
#undef environ
#undef stderr
#undef stdin
#undef stdout
#undef ferror
#undef feof

#ifdef __BORLANDC__
#undef ungetc
#undef getc
#undef putc
#undef getchar
#undef putchar
#undef fileno
#endif

#define stderr				win32_stderr()
#define stdout				win32_stdout()
#define	stdin				win32_stdin()
#define feof(f)				win32_feof(f)
#define ferror(f)			win32_ferror(f)
#define errno 				(*win32_errno())
#define environ				(*win32_environ())
#define strerror			win32_strerror

/*
 * redirect to our own version
 */
#define	fprintf			win32_fprintf
#define	vfprintf		win32_vfprintf
#define	printf			win32_printf
#define	vprintf			win32_vprintf
#define fread(buf,size,count,f)	win32_fread(buf,size,count,f)
#define fwrite(buf,size,count,f)	win32_fwrite(buf,size,count,f)
#define fopen			win32_fopen
#define fdopen			win32_fdopen
#define freopen			win32_freopen
#define	fclose(f)		win32_fclose(f)
#define fputs(s,f)		win32_fputs(s,f)
#define fputc(c,f)		win32_fputc(c,f)
#define ungetc(c,f)		win32_ungetc(c,f)
#define getc(f)			win32_getc(f)
#define fileno(f)		win32_fileno(f)
#define clearerr(f)		win32_clearerr(f)
#define fflush(f)		win32_fflush(f)
#define ftell(f)		win32_ftell(f)
#define fseek(f,o,w)		win32_fseek(f,o,w)
#define fgetpos(f,p)		win32_fgetpos(f,p)
#define fsetpos(f,p)		win32_fsetpos(f,p)
#define rewind(f)		win32_rewind(f)
#define tmpfile()		win32_tmpfile()
#define abort()			win32_abort()
#define fstat(fd,bufptr)   	win32_fstat(fd,bufptr)
#define stat(pth,bufptr)   	win32_stat(pth,bufptr)
#define setmode(fd,mode)	win32_setmode(fd,mode)
#define lseek(fd,offset,orig)	win32_lseek(fd,offset,orig)
#define tell(fd)		win32_tell(fd)
#define dup(fd)			win32_dup(fd)
#define dup2(fd1,fd2)		win32_dup2(fd1,fd2)
#define open			win32_open
#define close(fd)		win32_close(fd)
#define eof(fd)			win32_eof(fd)
#define read(fd,b,s)		win32_read(fd,b,s)
#define write(fd,b,s)		win32_write(fd,b,s)
#define _open_osfhandle		stolen_open_osfhandle
#define _get_osfhandle		stolen_get_osfhandle
#define spawnvp			win32_spawnvp
#define mkdir			win32_mkdir
#define rmdir			win32_rmdir
#define chdir			win32_chdir
#define flock(fd,o)		win32_flock(fd,o)
#define execvp			win32_execvp
#define perror			win32_perror
#define setbuf			win32_setbuf
#define setvbuf			win32_setvbuf
#define flushall		win32_flushall
#define fcloseall		win32_fcloseall
#define fgets			win32_fgets
#define gets			win32_gets
#define fgetc			win32_fgetc
#define putc			win32_putc
#define puts			win32_puts
#define getchar			win32_getchar
#define putchar			win32_putchar
#define fscanf			(GetIOSubSystem()->pfnfscanf)
#define scanf			(GetIOSubSystem()->pfnscanf)
#define malloc			win32_malloc
#define calloc			win32_calloc
#define realloc			win32_realloc
#define free			win32_free
#endif /* WIN32IO_IS_STDIO */

#endif /* WIN32IOP_H */
