#ifndef WIN32IO_H
#define WIN32IO_H

#ifdef __BORLANDC__
#include <stdarg.h>
#endif

typedef struct {
int	signature_begin;
int *	(*pfnerrno)(void);
char ***(*pfnenviron)(void);
FILE*	(*pfnstdin)(void);
FILE*	(*pfnstdout)(void);
FILE*	(*pfnstderr)(void);
int	(*pfnferror)(FILE *fp);
int	(*pfnfeof)(FILE *fp);
char*	(*pfnstrerror)(int e);
int	(*pfnvfprintf)(FILE *pf, const char *format, va_list arg);
int	(*pfnvprintf)(const char *format, va_list arg);
size_t	(*pfnfread)(void *buf, size_t size, size_t count, FILE *pf);
size_t	(*pfnfwrite)(const void *buf, size_t size, size_t count, FILE *pf);
FILE*	(*pfnfopen)(const char *path, const char *mode);
FILE*	(*pfnfdopen)(int fh, const char *mode);
FILE*	(*pfnfreopen)(const char *path, const char *mode, FILE *pf);
int	(*pfnfclose)(FILE *pf);
int	(*pfnfputs)(const char *s,FILE *pf);
int	(*pfnfputc)(int c,FILE *pf);
int	(*pfnungetc)(int c,FILE *pf);
int	(*pfngetc)(FILE *pf);
int	(*pfnfileno)(FILE *pf);
void	(*pfnclearerr)(FILE *pf);
int	(*pfnfflush)(FILE *pf);
long	(*pfnftell)(FILE *pf);
int	(*pfnfseek)(FILE *pf,long offset,int origin);
int	(*pfnfgetpos)(FILE *pf,fpos_t *p);
int	(*pfnfsetpos)(FILE *pf,const fpos_t *p);
void	(*pfnrewind)(FILE *pf);
FILE*	(*pfntmpfile)(void);
void	(*pfnabort)(void);
int  	(*pfnfstat)(int fd,struct stat *bufptr);
int  	(*pfnstat)(const char *name,struct stat *bufptr);
int	(*pfnpipe)( int *phandles, unsigned int psize, int textmode );
FILE*	(*pfnpopen)( const char *command, const char *mode );
int	(*pfnpclose)( FILE *pf);
int	(*pfnsetmode)( int fd, int mode);
long	(*pfnlseek)( int fd, long offset, int origin);
long	(*pfntell)( int fd);
int	(*pfndup)( int fd);
int	(*pfndup2)(int h1, int h2);
int	(*pfnopen)(const char *path, int oflag,...);
int	(*pfnclose)(int fd);
int	(*pfneof)(int fd);
int	(*pfnread)(int fd, void *buf, unsigned int cnt);
int	(*pfnwrite)(int fd, const void *buf, unsigned int cnt);
int	(*pfnopenmode)(int mode);
int	(*pfn_open_osfhandle)(long handle, int flags);
long	(*pfn_get_osfhandle)(int fd);
int	(*pfnspawnvp)(int mode, const char *cmdname, const char *const *argv);
int	(*pfnmkdir)(const char *path);
int	(*pfnrmdir)(const char *path);
int	(*pfnchdir)(const char *path);
int	(*pfnflock)(int fd, int oper);
int	(*pfnexecvp)(const char *cmdname, const char *const *argv);
void	(*pfnperror)(const char *str);
void	(*pfnsetbuf)(FILE *pf, char *buf);
int	(*pfnsetvbuf)(FILE *pf, char *buf, int type, size_t size);
int	(*pfnflushall)(void);
int	(*pfnfcloseall)(void);
char*	(*pfnfgets)(char *s, int n, FILE *pf);
char*	(*pfngets)(char *s);
int	(*pfnfgetc)(FILE *pf);
int	(*pfnputc)(int c, FILE *pf);
int	(*pfnputs)(const char *s);
int	(*pfngetchar)(void);
int	(*pfnputchar)(int c);
int	(*pfnfscanf)(FILE *pf, const char *format, ...);
int	(*pfnscanf)(const char *format, ...);
void*	(*pfnmalloc)(size_t size);
void*	(*pfncalloc)(size_t numitems, size_t size);
void*	(*pfnrealloc)(void *block, size_t size);
void	(*pfnfree)(void *block);
int	signature_end;
} WIN32_IOSUBSYSTEM; 

typedef WIN32_IOSUBSYSTEM	*PWIN32_IOSUBSYSTEM;

#endif /* WIN32IO_H */
