#ifdef USE_CACHE
#include <sys/stat.h>
#endif

extern inline void _dl_exit (int status) __attribute__ ((noreturn));
extern inline int _dl_close (int fd);
extern inline int _dl_mmap (void *addr, unsigned int size,
			    unsigned int prot,
			    unsigned int flags, int fd,
			    unsigned int f_offset);
#ifndef _dl_MAX_ERRNO
#define _dl_MAX_ERRNO 4096
#endif
#define _dl_mmap_check_error(__res)	\
  ((int) __res < 0 && (int) __res >= -_dl_MAX_ERRNO)
extern inline int _dl_open (const char *addr, unsigned int flags);
extern inline int _dl_write (int fd, const char *buf, int len);
extern inline int _dl_read (int fd, char *buf, int len);
extern inline int _dl_mprotect (const void *addr, int size, int prot);
#ifdef USE_CACHE
extern inline int _dl_stat (const char *, struct stat *);
extern inline int _dl_munmap (const void *, unsigned int);
#endif

/* Here are the definitions for a bunch of syscalls that are required
   by the dynamic linker.  The idea is that we want to be able to call
   these before the errno symbol is dynamicly linked, so we use our
   own version here.  Note that we cannot assume any dynamic linking
   at all, so we cannot return any error codes.  We just punt if there
   is an error. */

extern inline void
_dl_exit (int status)
{
  register int __res __asm__ ("%d0");
  __asm__ volatile ("movel %2,%/d1\n"
		    "trap #0"
		    : "=d" (__res)
		    : "0" (__NR_exit), "d" (status)
		    : "%d0", "%d1");
  for (;;);
}

extern inline int
_dl_close (int fd)
{
  register int status __asm__ ("%d0");

  __asm__ volatile ("movel %2,%/d1\n"
		    "trap #0"
		    : "=d" (status)
		    : "0" (__NR_close), "d" (fd)
		    : "%d0", "%d1");
  return status;
}

extern inline int
_dl_mmap (void *addr, unsigned int size, unsigned int prot,
	  unsigned int flags, int fd, unsigned int f_offset)
{
  register int malloc_buffer __asm__ ("%d0");
  unsigned long buffer[6];

  buffer[0] = (unsigned long) addr;
  buffer[1] = (unsigned long) size;
  buffer[2] = (unsigned long) prot;
  buffer[3] = (unsigned long) flags;
  buffer[4] = (unsigned long) fd;
  buffer[5] = (unsigned long) f_offset;
  
  __asm__ volatile ("movel %2,%/d1\n\t"
		    "trap #0"
		    : "=d" (malloc_buffer)
		    : "0" (__NR_mmap), "g" (buffer)
		    : "%d0", "%d1");
  return malloc_buffer;
}


extern inline int
_dl_open (const char *addr, unsigned int flags)
{
  register int zfileno __asm__ ("%d0");
  __asm__ volatile ("movel %2,%/d1\n\t"
		    "movel %3,%/d2\n\t"
		    "trap #0"
		    : "=d" (zfileno)
		    : "0" (__NR_open), "g" (addr), "g" (flags)
		    : "%d0", "%d1", "%d2");
  return zfileno;
}

extern inline int
_dl_write (int fd, const char* buf, int len)
{
  register int status __asm__ ("%d0");
  __asm__ volatile ("movel %2,%/d1\n\t"
		    "movel %3,%/d2\n\t"
		    "movel %4,%/d3\n\t"
		    "trap #0"
		    : "=d" (status)
		    : "0" (__NR_write), "g" (fd), "g" (buf), "g" (len)
		    : "%d0", "%d1", "%d2", "%d3");
  return status;
}


extern inline int
_dl_read (int fd, char *buf, int len)
{
  register int status __asm__ ("%d0");
  __asm__ volatile ("movel %2,%/d1\n\t"
		    "movel %3,%/d2\n\t"
		    "movel %4,%/d3\n\t"
		    "trap #0"
		    : "=d" (status)
		    : "0" (__NR_read), "g" (fd), "g" (buf), "g" (len)
		    : "%d0", "%d1", "%d2", "%d3");
  return status;
}

extern inline int
_dl_mprotect (const void *addr, int size, int prot)
{
  register int status __asm__ ("%d0");
  __asm__ volatile ("movel %2,%/d1\n\t"
		    "movel %3,%/d2\n\t"
		    "movel %4,%/d3\n\t"
		    "trap #0"
		    : "=d" (status)
		    : "0" (__NR_mprotect), "g" (addr), "g" (size), "g" (prot)
		    : "%d0", "%d1", "%d2", "%d3");
  return status;
}

#ifdef USE_CACHE
extern inline int
_dl_stat (const char *name, struct stat *sb)
{
  register int status __asm__ ("%d0");
  __asm__ volatile ("movel %2,%/d1\n\t"
		    "movel %3,%/d2\n\t"
		    "trap #0"
		    : "=d" (status)
		    : "0" (__NR_stat), "g" (name), "g" (sb)
		    : "%d0", "%d1", "%d2");
  return status;
}

extern inline int
_dl_munmap (const void *addr, unsigned int size)
{
  register int status __asm__ ("%d0");
  __asm__ volatile ("movel %2,%/d1\n\t"
		    "movel %3,%/d2\n\t"
		    "trap #0"
		    : "=d" (status)
		    : "0" (__NR_munmap), "g" (addr), "g" (size)
		    : "%d0", "%d1");
  return status;
}
#endif

/* Not an actual syscall, but we need something in assembly to say
   whether this is OK or not.  */

extern inline int
_dl_suid_ok (void)
{
  unsigned int uid, euid, gid, egid;

  __asm__ volatile ("movel %1,%%d0; trap #0; movel %%d0,%0"
		    : "=g" (uid) : "i" (__NR_getuid) : "%d0");
  __asm__ volatile ("movel %1,%%d0; trap #0; movel %%d0,%0"
		    : "=g" (euid) : "i" (__NR_geteuid) : "%d0");
  __asm__ volatile ("movel %1,%%d0; trap #0; movel %%d0,%0"
		    : "=g" (gid) : "i" (__NR_getgid) : "%d0");
  __asm__ volatile ("movel %1,%%d0; trap #0; movel %%d0,%0"
		    : "=g" (egid) : "i" (__NR_getegid) : "%d0");

  return (uid == euid && gid == egid);
}
