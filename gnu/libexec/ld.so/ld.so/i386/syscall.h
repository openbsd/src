
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

extern inline volatile void _dl_exit(int status)
{
  int __res;
#ifdef IBCS_COMPATIBLE
  __asm__ volatile ("pushl %0\n\tpushl $0\n\tmovl %1,%%eax\n\t" \
		    "lcall $7,$0" : : "r" (status), "a" (__IBCS_exit));
#else
  __asm__ volatile ("movl %%ecx,%%ebx\n"\
		    "int $0x80" \
		    :  "=a" (__res) : "0" (__NR_exit),"c" ((long)(status)));
#endif
}

extern inline volatile void _dl_close(int fd)
{
    int status;

#ifdef IBCS_COMPATIBLE
    __asm__ volatile ("pushl %1\n\t" \
	    "pushl $0\n\t" \
	    "movl %2,%%eax\n\t" \
	    "lcall $7,$0\n\t" \
	    "jnb .+4\n\t" \
	    "xor %%eax, %%eax\n\t" \
	    "addl $8,%%esp\n\t" \
	    : "=a" (status) : "r" (fd), "a" (__IBCS_close));
#else
  __asm__ volatile ("pushl %%ebx\n"\
		    "movl %%ecx,%%ebx\n"\
		    "int $0x80\n" \
		    "popl %%ebx\n"\
		    : "=a" (status) \
		    : "0" (__NR_close),"c" ((long)(fd)));
    
#endif
}

extern inline int _dl_mmap(void * addr, unsigned int size,
				    unsigned int prot,
				    unsigned int flags, int fd,
				    unsigned int f_offset)
{
  int malloc_buffer;
#ifdef IBCS_COMPATIBLE
  __asm__ volatile ("pushl %7\n\t" \
		    "pushl %6\n\t" \
		    "pushl %5\n\t" \
		    "pushl %4\n\t" \
		    "pushl %3\n\t" \
		    "pushl %2\n\t" \
		    "pushl $0\n\t" \
		    "movl %1,%%eax\n\t" \
		    "lcall $7,$0\n\t" \
		    "jnb .+4\n\t" \
		    "xor %%eax, %%eax\n\t" \
		    "addl $28,%%esp\n\t" \
		    : "=a" (malloc_buffer) : "a" (__IBCS_mmap),
		    "rm" (addr), "rm" (size), "rm" (prot), "rm" (flags), 
		    "rm" (fd), "rm" (f_offset));
#else
  __asm__ volatile ("pushl %%ebx\n\t" \
		    "pushl %7\n\t" \
		    "pushl %6\n\t" \
		    "pushl %5\n\t" \
		    "pushl %4\n\t" \
		    "pushl %3\n\t" \
		    "pushl %2\n\t" \
		    "movl %%esp,%%ebx\n\t" \
		    "int $0x80\n\t" \
		    "addl $24,%%esp\n\t" \
		    "popl %%ebx\n" \
		    : "=a" (malloc_buffer) : "a" (__NR_mmap),
		    "rm" (addr), "rm" (size), "rm" (prot), "rm" (flags), 
		    "rm" (fd), "rm" (f_offset));
#endif
  return malloc_buffer;
}


extern inline int _dl_open(char * addr, unsigned int flags)
{
  int zfileno;
#ifdef IBCS_COMPATIBLE
  __asm__ volatile ("pushl %3\n\t" \
	    "pushl %2\n\t" \
	    "pushl $0\n\t" \
	    "movl %1,%%eax\n\t" \
	    "lcall $7,$0\n\t" \
	    "jnb .+7\n\t" \
	    "movl $-1, %%eax\n\t" \
	    "addl $12,%%esp\n\t" \
	    :"=a" (zfileno) : "i" (__IBCS_open), "rm" (addr), "rm" (flags));
#else
  __asm__ volatile ("pushl %%ebx\n"\
		    "movl %%esi,%%ebx\n"\
		    "int $0x80\n" \
		    "popl %%ebx\n"\
		    : "=a" (zfileno) \
		    : "0" (__NR_open),"S" ((long)(addr)),"c" ((long)(flags)));
#endif

  return zfileno;
}

extern inline int _dl_write(int fd, const char * buf, int len)
{
  int status;
#ifdef IBCS_COMPATIBLE
  __asm__ volatile ("pushl %4\n\t" \
	  "pushl %3\n\t" \
	  "pushl %2\n\t" \
	  "pushl $0\n\t" \
	  "movl %1,%%eax\n\t" \
	  "lcall $7,$0\n\t" \
	  "jnb .+4\n\t" \
	  "xor %%eax, %%eax\n\t" \
	  "addl $12,%%esp\n\t" \
	  :"=a" (status) : "i" (__IBCS_write), "rm" (fd), "rm" (buf), "rm" (len));
#else
  __asm__ volatile ("pushl %%ebx\n"\
		    "movl %%esi,%%ebx\n"\
		    "int $0x80\n" \
		    "popl %%ebx\n"\
		    : "=a" (status) \
		    : "0" (__NR_write),"S" ((long)(fd)),"c" ((long)(buf)),"d" ((long)(len)));
#endif
}


extern inline int _dl_read(int fd, const char * buf, int len)
{
  int status;
#ifdef IBCS_COMPATIBLE
  __asm__ volatile ("pushl %4\n\t" \
	  "pushl %3\n\t" \
	  "pushl %2\n\t" \
	  "pushl $0\n\t" \
	  "movl %1,%%eax\n\t" \
	  "lcall $7,$0\n\t" \
	  "jnb .+4\n\t" \
	  "xor %%eax, %%eax\n\t" \
	  "addl $12,%%esp\n\t" \
	  : "=a" (status) : "i" (__IBCS_read), "rm" (fd), "rm" (buf), "rm" (len));
#else
  __asm__ volatile ("pushl %%ebx\n"\
		    "movl %%esi,%%ebx\n"\
		    "int $0x80\n" \
		    "popl %%ebx\n"\
		    : "=a" (status) \
		    : "0" (__NR_read),"S" ((long)(fd)),"c" ((long)(buf)),"d" ((long)(len)));
#endif
}

extern inline int _dl_mprotect(const char * addr, int size, int prot)
{
  int status;
#ifdef IBCS_COMPATIBLE
  __asm__ volatile ("pushl %4\n\t" \
	  "pushl %3\n\t" \
	  "pushl %2\n\t" \
	  "pushl $0\n\t" \
	  "movl %1,%%eax\n\t" \
	  "lcall $7,$0\n\t" \
	  "jnb .+7\n\t" \
	  "movl $-1, %%eax\n\t" \
	  "addl $16,%%esp\n\t" \
	  :"=a" (status) : "i" (__IBCS_mprotect), "rm" (addr), "rm" (size), "rm" (prot));
#else
  __asm__ volatile ("pushl %%ebx\n"\
		    "movl %%esi,%%ebx\n"\
		    "int $0x80\n" \
		    "popl %%ebx\n"\
		    : "=a" (status) \
		    : "0" (__NR_mprotect),"S" ((long)(addr)),"c" ((long)(size)),"d" ((long)(prot)));
#endif
  return status;
}

extern inline int _dl_stat(char * filename, struct stat *st)
{
  int ret;
#ifdef IBCS_COMPATIBLE
  __asm__ volatile ("pushl %3\n\t" \
            "pushl %2\n\t" \
            "pushl $0\n\t" \
            "movl %1,%%eax\n\t" \
            "lcall $7,$0\n\t" \
            "jnb .+7\n\t" \
            "movl $-1, %%eax\n\t" \
            "addl $12,%%esp\n\t" \
            :"=a" (ret) : "i" (__IBCS_stat), "rm" (filename), "rm" (st));
#else
  __asm__ volatile ("pushl %%ebx\n"\
                    "movl %%esi,%%ebx\n"\
                    "int $0x80\n" \
                    "popl %%ebx\n"\
                    : "=a" (ret) \
                    : "0" (__NR_stat),"S" (filename),"c" (st));
#endif

  return ret;
}

extern inline int _dl_munmap(char * addr, int size)
{
  int ret;
#ifdef IBCS_COMPATIBLE
  __asm__ volatile ("pushl %3\n\t" \
	    "pushl %2\n\t" \
	    "pushl $0\n\t" \
	    "movl %1,%%eax\n\t" \
	    "lcall $7,$0\n\t" \
	    "jnb .+7\n\t" \
	    "movl $-1, %%eax\n\t" \
	    "addl $12,%%esp\n\t" \
	    :"=a" (ret) : "i" (__IBCS_munmap), "rm" (addr), "rm" (size));
#else
  __asm__ volatile ("pushl %%ebx\n"\
		    "movl %%esi,%%ebx\n"\
		    "int $0x80\n" \
		    "popl %%ebx\n"\
		    : "=a" (ret) \
		    : "0" (__NR_munmap),"S" ((long)(addr)),"c" ((long)(size)));
#endif

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

#ifdef IBCS_COMPATIBLE
    __asm__("movl %2,%%eax\n\tlcall $7,$0" :"=a" (uid), "=d" (euid) :"a" (__IBCS_getuid));
    __asm__("movl %2,%%eax\n\tlcall $7,$0" :"=a" (gid), "=d" (egid) :"a" (__IBCS_getgid));
#else
    __asm__ volatile ("int $0x80" : "=a" (uid) : "0" (__NR_getuid));
    __asm__ volatile ("int $0x80" : "=a" (euid) : "0" (__NR_geteuid));
    __asm__ volatile ("int $0x80" : "=a" (gid) : "0" (__NR_getgid));
    __asm__ volatile ("int $0x80" : "=a" (egid) : "0" (__NR_getegid));
#endif

    if(uid == euid && gid == egid)
      return 1;
    else
      return 0;
}

