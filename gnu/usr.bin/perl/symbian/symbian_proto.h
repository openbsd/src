/*
 *	symbian_proto.h
 *
 *	Copyright (c) Nokia 2004-2005.  All rights reserved.
 *      This code is licensed under the same terms as Perl itself.
 *
 */

#ifndef SYMBIAN_PROTO_H
#define SYMBIAN_PROTO_H

#include <sys/types.h>
#include <sys/times.h>

#if defined(PERL_CORE) || defined(PERL_EXT)

/* We can't include the <string.h> unconditionally
 * since it has prototypes conflicting with the gcc builtins. */
extern void  *memchr(const void *s, int c, size_t n);
#ifndef DL_SYMBIAN_XS
/* dl_symbian.xs needs to see the C++ prototype of memset() instead */
extern void  *memset(void *s, int c, size_t n);
extern size_t strlen(const char *s);
#endif
extern void  *memmove(void *dst, const void *src, size_t n);
extern char  *strcat(char *dst, const char *src);
extern char  *strchr(const char *s, int c);
extern char  *strerror(int errnum);
extern int    strncmp(const char *s1, const char *s2, size_t n);
extern char  *strrchr(const char *s, int c);

extern int setmode(int fd, long flags);

#ifndef __GNUC__
#define memcpy _e32memcpy /* GCC intrinsic */
extern void  *memcpy(const void *s1, const void *s2, size_t n);
extern int    strcmp(const char *s1, const char *s2);
extern char*  strcpy(char *dst, const char *src);
extern char*  strncpy(char *dst, const char *src, size_t n);
#endif

#endif /* PERL_CORE || PERL_EXT */

#if defined(SYMBIAN_DLL_CPP) || defined(SYMBIAN_UTILS_CPP) || defined(PERLBASE_CPP) || defined(PERLUTIL_CPP)
#  define PERL_SYMBIAN_START_EXTERN_C extern "C" {
#  define PERL_SYMBIAN_IMPORT_C       IMPORT_C /* Declarations have IMPORT_C, definitions have EXPORT_C. */
#  define PERL_SYMBIAN_END_EXTERN_C   }
#else
#  define PERL_SYMBIAN_START_EXTERN_C
#  define PERL_SYMBIAN_IMPORT_C
#  define PERL_SYMBIAN_END_EXTERN_C
#endif

PERL_SYMBIAN_START_EXTERN_C
PERL_SYMBIAN_IMPORT_C int   symbian_sys_init(int *argcp, char ***argvp);
PERL_SYMBIAN_IMPORT_C void  init_os_extras(void);
PERL_SYMBIAN_IMPORT_C void* symbian_get_vars(void);
PERL_SYMBIAN_IMPORT_C void  symbian_set_vars(const void *);
PERL_SYMBIAN_IMPORT_C void  symbian_unset_vars(void);
PERL_SYMBIAN_IMPORT_C SSize_t symbian_read_stdin(const int fd, char *b, int n);
PERL_SYMBIAN_IMPORT_C SSize_t symbian_write_stdout(const int fd, const char *b, int n);
PERL_SYMBIAN_IMPORT_C char* symbian_get_error_string(const int error);
PERL_SYMBIAN_IMPORT_C void symbian_sleep_usec(const long usec);
PERL_SYMBIAN_IMPORT_C int symbian_get_cpu_time(long* sec, long* usec);
PERL_SYMBIAN_IMPORT_C clock_t symbian_times(struct tms* buf);
PERL_SYMBIAN_IMPORT_C int symbian_usleep(unsigned int usec);
PERL_SYMBIAN_IMPORT_C int symbian_do_aspawn(void* vreally, void *vmark, void* sp);
PERL_SYMBIAN_IMPORT_C int symbian_do_spawn(const char* command);
PERL_SYMBIAN_IMPORT_C int symbian_do_spawn_nowait(const char* command);
PERL_SYMBIAN_END_EXTERN_C

#endif /* !SYMBIAN_PROTO_H */

