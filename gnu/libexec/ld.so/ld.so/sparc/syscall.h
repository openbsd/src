
extern inline volatile void _dl_exit(int status);
extern inline volatile void _dl_close(int fd);
extern inline int _dl_mmap(void * addr, unsigned int size,
				    unsigned int prot,
				    unsigned int flags, int fd,
				    unsigned int f_offset);
#ifndef _dl_MAX_ERRNO
#define _dl_MAX_ERRNO 4096
#endif
#define _dl_mmap_check_error(__res)	\
	(((int)__res) < 0 && ((int)__res) >= -_dl_MAX_ERRNO)
extern inline int _dl_open(char * addr, unsigned int flags);
extern inline int _dl_write(int fd, const char * buf, int len);
extern inline int _dl_read(int fd, const char * buf, int len);
extern inline int _dl_mprotect(const char * addr, int size, int prot);
#include <sys/stat.h>
extern inline int _dl_stat(char * filename, struct stat *st);
extern inline int _dl_munmap(char * addr, int size);

/* Here are the definitions for a bunch of syscalls that are required
   by the dynamic linker.  The idea is that we want to be able
   to call these before the errno symbol is dynamicly linked, so
   we use our own version here.  Note that we cannot assume any
   dynamic linking at all, so we cannot return any error codes.
   We just punt if there is an error. */

#ifdef SOLARIS_COMPATIBLE
#include "/usr/include/sys/syscall.h"
#endif

extern inline volatile void _dl_exit(int status)
{
  int __res;
  __asm__ volatile ("mov %1,%%g1\n"\
		     "or %%g0, %2, %%o0\n\t" \
		     "ta 8" \
		     :  "=r" (__res) : "i" (__NR_exit),"r" ((long)(status)));
}

extern inline volatile void _dl_close(int fd)
{
    int status;

  __asm__ volatile ("mov %1,%%g1\n"\
		     "or %%g0, %2, %%o0\n\t" \
		     "ta 8\n" \
		     "mov %%o0, %0\n\t"
		     : "=r" (status) \
		     : "i" (__NR_close),"r" ((long)(fd))
		     : "o0", "g1");
    
}

extern inline int _dl_mmap(void * addr, unsigned int size,
				    unsigned int prot,
				    unsigned int flags, int fd,
				    unsigned int f_offset)
{
  int malloc_buffer;
#ifdef SOLARIS_COMPATIBLE
  __asm__ volatile ("mov %1,%%g1\n"\
		     "or %%g0, %2, %%o0\n\t" \
		     "or %%g0, %3, %%o1\n\t" \
		     "or %%g0, %4, %%o2\n\t" \
		     "sethi %%hi(0x80000000), %%o3\n\t" \
		     "or %%o3, %5, %%o3\n\t" \
		     "or %%g0, %6, %%o4\n\t" \
		     "or %%g0, %7, %%o5\n\t" \
		     "ta 8\n\t" \
		     "mov %%o0, %0\n\t"
		    : "=r" (malloc_buffer) : "0" (SYS_mmap),
		    "r" (addr), "r" (size), "r" (prot), "r" (flags), 
		    "r" (fd), "r" (f_offset)
		     : "g1", "o0", "o1",  "o2", "o3",  "o4", "o5");
#else
  __asm__ volatile ("mov %1,%%g1\n"\
		     "or %%g0, %2, %%o0\n\t" \
		     "or %%g0, %3, %%o1\n\t" \
		     "or %%g0, %4, %%o2\n\t" \
		     "or %%g0, %5, %%o3\n\t" \
		     "or %%g0, %6, %%o4\n\t" \
		     "or %%g0, %7, %%o5\n\t" \
		     "ta 8\n\t" \
		     "mov %%o0, %0\n\t"
		    : "=r" (malloc_buffer) : "0" (__NR_mmap),
		    "r" (addr), "r" (size), "r" (prot), "r" (flags), 
		    "r" (fd), "r" (f_offset)
		     : "g1", "o0", "o1",  "o2", "o3",  "o4", "o5");
#endif
  return malloc_buffer;
}


extern inline int _dl_open(char * addr, unsigned int flags)
{
  int zfileno;
  __asm__ volatile ("mov %1,%%g1\n"\
		     "or %%g0, %2, %%o0\n\t" \
		     "or %%g0, %3, %%o1\n\t" \
		     "or %%g0, 0, %%o2\n\t" \
		     "ta 8\n" \
		     "mov %%o0, %0\n\t"
		    : "=r" (zfileno) \
		    : "i" (__NR_open),"r" ((long)(addr)),"r" ((long)(flags))
		     : "g1", "o0", "o1",  "o2");

  return zfileno;
}

extern inline int _dl_write(int fd, const char * buf, int len)
{
  int status;
  __asm__ volatile ("mov %1,%%g1\n"\
		     "or %%g0, %2, %%o0\n\t" \
		     "or %%g0, %3, %%o1\n\t" \
		     "or %%g0, %4, %%o2\n\t" \
		     "ta 8\n" \
		     "mov %%o0, %0\n\t"
		    : "=r" (status) \
		    : "i" (__NR_write),"r" ((long)(fd)),"r" ((long)(buf)),"r" ((long)(len))
		     : "g1", "o0", "o1",  "o2");
}


extern inline int _dl_read(int fd, const char * buf, int len)
{
  int status;
  __asm__ volatile ("mov %1,%%g1\n"\
		     "or %%g0, %2, %%o0\n\t" \
		     "or %%g0, %3, %%o1\n\t" \
		     "or %%g0, %4, %%o2\n\t" \
		     "ta 8\n" \
		     "mov %%o0, %0\n\t"
		    : "=r" (status) \
		    : "i" (__NR_read),"r" ((long)(fd)),"r" ((long)(buf)),"r" ((long)(len))
		     : "g1", "o0", "o1",  "o2");
}

extern inline int _dl_mprotect(const char * addr, int size, int prot)
{
  int status;
#ifdef SOLARIS_COMPATIBLE
  __asm__ volatile ( "or %%g0, %2, %%o0\n\t" \
		     "or %%g0, %3, %%o1\n\t" \
		     "or %%g0, %4, %%o2\n\t" \
		     "mov %1,%%g1\n"\
		     "ta 8\n" \
		     "mov %%o0, %0\n\t"
		    : "=r" (status) \
		    : "i" (SYS_mprotect),"r" ((long)(addr)),"r" ((long)(size)),"r" ((long)(prot))
		     : "g1", "o0", "o1",  "o2");
#else
  __asm__ volatile ( "or %%g0, %2, %%o0\n\t" \
		     "or %%g0, %3, %%o1\n\t" \
		     "or %%g0, %4, %%o2\n\t" \
		     "mov %1,%%g1\n"\
		     "ta 8\n" \
		     "mov %%o0, %0\n\t"
		    : "=r" (status) \
		    : "i" (__NR_mprotect),"r" ((long)(addr)),"r" ((long)(size)),"r" ((long)(prot))
		     : "g1", "o0", "o1",  "o2");
#endif
  return status;
}

extern inline int _dl_stat(char * filename, struct stat *st)
{
  int ret;
#ifdef SOLARIS_COMPATIBLE
  __asm__ volatile ("mov %1,%%g1\n"\
		     "or %%g0, %2, %%o0\n\t" \
		     "or %%g0, %3, %%o1\n\t" \
		     "ta 8\n" \
		     "mov %%o0, %0\n\t"
                    : "=r" (ret) \
                    : "i" (SYS_stat),"r" (filename),"r" (st)
		     : "g1", "o0", "o1");


#else
  __asm__ volatile ("mov %1,%%g1\n"\
		     "or %%g0, %2, %%o0\n\t" \
		     "or %%g0, %3, %%o1\n\t" \
		     "ta 8\n" \
		     "mov %%o0, %0\n\t"
                    : "=r" (ret) \
                    : "i" (__NR_stat),"r" (filename),"r" (st)
		     : "g1", "o0", "o1");


#endif
  return ret;
}

extern inline int _dl_munmap(char * addr, int size)
{
  int ret;
  __asm__ volatile ("mov %1,%%g1\n"\
		     "or %%g0, %2, %%o0\n\t" \
		     "or %%g0, %3, %%o1\n\t" \
		     "ta 8\n" \
		     "mov %%o0, %0\n\t"
		     : "=r" (ret) \
		     : "i" (__NR_munmap),"r" ((long)(addr)),"r" ((long)(size))
		     : "g1", "o0", "o1");

  return ret;
}

/*
 * Not an actual syscall, but we need something in assembly to say whether
 * this is OK or not.
 */

extern inline int _dl_suid_ok(void)
{
  unsigned int uid, euid, gid, egid;
  euid = egid = 0; /* So compiler does not warn us */

#ifdef SOLARIS_COMPATIBLE
  __asm__ volatile ("mov %2,%%g1\nta 8\n\t" 
		     "mov %%o0, %0\n\t"
		     "mov %%o1, %1\n\t"
		     : "=r" (uid) , "=r" (euid) : "i" (SYS_getuid) : "g1");
  __asm__ volatile ("mov %2,%%g1\nta 8\n\t"
		     "mov %%o0, %0\n\t"
		     "mov %%o1, %1\n\t"
		     : "=r" (gid), "=r" (euid)  : "i" (__NR_getgid) : "g1");
#else
  __asm__ volatile ("mov %1,%%g1\nta 8\n\t" 
		     "mov %%o0, %0\n\t"
		     : "=r" (uid) : "i" (__NR_getuid) : "g1");
  __asm__ volatile ("mov %1,%%g1\nta 8\n\t"
		     "mov %%o0, %0\n\t"
		     : "=r" (euid) : "i" (__NR_geteuid) : "g1");
  __asm__ volatile ("mov %1,%%g1\nta 8\n\t"
		     "mov %%o0, %0\n\t"
		     : "=r" (gid) : "i" (__NR_getgid) : "g1");
  __asm__ volatile ("mov %1,%%g1\nta 8\n\t"
		     "mov %%o0, %0\n\t"
		     : "=r" (egid) : "i" (__NR_getegid) : "g1");
#endif

    if(uid == euid && gid == egid)
      return 1;
    else
      return 0;
}
