#ifdef USE_CACHE
#include <sys/stat.h>
#endif

#include <sys/syscall.h>

#ifndef _dl_MAX_ERRNO
#define _dl_MAX_ERRNO 4096
#endif
#define _dl_mmap_check_error(__res)	\
  ((int) __res < 0 && (int) __res >= -_dl_MAX_ERRNO)

/* Here are the definitions for a bunch of syscalls that are required
   by the dynamic linker.  The idea is that we want to be able to call
   these before the errno symbol is dynamicly linked, so we use our
   own version here.  Note that we cannot assume any dynamic linking
   at all, so we cannot return any error codes.  We just punt if there
   is an error. */

extern inline int
_dl_exit (int status)
{ 
  __asm__ volatile ("li    $2,%0\n\t"
		    "move  $4,%1\n\t"
                    "syscall"
                    : : "I" (SYS_exit), "r" (status)
                    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
		      "$10","$11","$12","$13","$14","$15","$24","$25");
  while (1)
    ;
} 

extern inline int
_dl_open (const char* addr, unsigned int flags)
{ 
  register int status;

  __asm__ volatile ("li    $2,%1\n\t"
		    "move  $4,%2\n\t"
                    "move  $5,%3\n\t"
                    "syscall\n\t"
		    "beq   $7,$0,1f\n\t"
		    "li    $2,-1\n\t"
		    "1:\n\t"
		    "move  %0,$2"
                    : "=r" (status)
                    : "I" (SYS_open), "r" (addr), "r" (flags)
                    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
		      "$10","$11","$12","$13","$14","$15","$24","$25");
  return status;
} 

extern inline int
_dl_close (int fd)
{ 
  register int status;

  __asm__ volatile ("li    $2,%1\n\t"
		    "move  $4,%2\n\t"
                    "syscall\n\t"
		    "beq   $7,$0,1f\n\t"
		    "li    $2,-1\n\t"
		    "1:\n\t"
		    "move  %0,$2"
                    : "=r" (status)
                    : "I" (SYS_close), "r" (fd)
                    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
		      "$10","$11","$12","$13","$14","$15","$24","$25");
  return status;
} 

extern inline int
_dl_write (int fd, const char* buf, int len)
{ 
  register int status;

  __asm__ volatile ("li    $2,%1\n\t"
		    "move  $4,%2\n\t"
                    "move  $5,%3\n\t"
                    "move  $6,%4\n\t"
                    "syscall\n\t"
		    "beq   $7,$0,1f\n\t"
		    "li    $2,-1\n\t"
		    "1:\n\t"
		    "move  %0,$2"
                    : "=r" (status)
                    : "I" (SYS_write), "r" (fd), "r" (buf), "r" (len)
                    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
		      "$10","$11","$12","$13","$14","$15","$24","$25");
  return status;
} 

extern inline int
_dl_read (int fd, const char* buf, int len)
{ 
  register int status;

  __asm__ volatile ("li    $2,%1\n\t"
		    "move  $4,%2\n\t"
                    "move  $5,%3\n\t"
                    "move  $6,%4\n\t"
                    "syscall\n\t"
		    "beq   $7,$0,1f\n\t"
		    "li    $2,-1\n\t"
		    "1:\n\t"
		    "move  %0,$2"
                    : "=r" (status)
                    : "I" (SYS_read), "r" (fd), "r" (buf), "r" (len)
                    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
		      "$10","$11","$12","$13","$14","$15","$24","$25");
  return status;
} 

extern inline int
_dl_mmap (void *addr, unsigned int size, unsigned int prot,
          unsigned int flags, int fd, unsigned int f_offset)
{ 
  register int malloc_buffer;

  __asm__ volatile ("li    $2,%1\n\t"
		    "addiu $29,-40\n\t"
                    "move  $6,%2\n\t"
                    "move  $7,%3\n\t"
		    "sw    %4,16($29)\n\t"
		    "sw    %5,20($29)\n\t"
#ifdef __MIPSEL__
		    "li    $4,%8\n\t"
                    "li    $5,0\n\t"
		    "sw    %6,24($29)\n\t"
		    "sw    $0,28($29)\n\t"
		    "sw    %7,32($29)\n\t"
		    "sw    $0,36($29)\n\t"
#else
#ifdef __MIPSEB__
                    "li    $4,0\n\t"
		    "li    $5,%8\n\t"
		    "sw    %6,24($29)\n\t"
		    "sw    $0,28($29)\n\t"
		    "sw    $0,32($29)\n\t"
		    "sw    %7,36($29)\n\t"
#else
#error "__MIPSEB__ or __MIPSEL__ not defined!"
#endif
#endif
                    "syscall\n\t"
		    "addiu $29,40\n\t"
		    "move  %0,$2"
                    : "=r" (malloc_buffer)
                    : "I" (SYS___syscall), "r" (addr), "r" (size), "r" (prot),
		      "r" (flags), "r" (fd), "r" (f_offset), "I" (SYS_mmap)
                    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
		      "$10","$11","$12","$13","$14","$15","$24","$25");
  return malloc_buffer;
} 

extern inline int
_dl_mprotect (const void *addr, int size, int prot)
{ 
  register int status;

  __asm__ volatile ("li    $2,%1\n\t"
		    "move  $4,%2\n\t"
                    "move  $5,%3\n\t"
                    "move  $6,%4\n\t"
                    "syscall\n\t"
		    "move  %0,$2"
                    : "=r" (status)
                    : "I" (SYS_mprotect), "r" (addr), "r" (size), "r" (prot)
                    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
		      "$10","$11","$12","$13","$14","$15","$24","$25");
  return status;
} 

#ifdef USE_CACHE
extern inline int
_dl_stat (const char *addr, struct stat *sb)
{ 
  register int status;

  __asm__ volatile ("li    $2,%1\n\t"
		    "move  $4,%2\n\t"
                    "move  $5,%3\n\t"
                    "syscall\n\t"
		    "move  %0,$2"
                    : "=r" (status)
                    : "I" (SYS_stat), "r" (addr), "r" (sb)
                    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
		      "$10","$11","$12","$13","$14","$15","$24","$25");
  return status;
} 

extern inline int
_dl_munmap (const void *addr, unsigned int size)
{ 
  register int status;

  __asm__ volatile ("li    $2,%1\n\t"
		    "move  $4,%2\n\t"
                    "move  $5,%3\n\t"
                    "syscall\n\t"
		    "move  %0,$2"
                    : "=r" (status)
                    : "I" (SYS_munmap), "r" (addr), "r" (size)
                    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
		      "$10","$11","$12","$13","$14","$15","$24","$25");
  return status;
} 
#endif

/* Not an actual syscall, but we need something in assembly to say
   whether this is OK or not.  */

extern inline int
_dl_suid_ok (void)
{
  unsigned int uid, euid, gid, egid;

  __asm__ volatile ("li $2,%1; syscall; move %0,$2"
                    : "=r" (uid) : "I" (SYS_getuid)
                    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
		     "$10","$11","$12","$13","$14","$15","$24","$25");
  __asm__ volatile ("li $2,%1; syscall; move %0,$2"
                    : "=r" (euid) : "I" (SYS_geteuid)
                    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
		     "$10","$11","$12","$13","$14","$15","$24","$25");
  __asm__ volatile ("li $2,%1; syscall; move %0,$2"
                    : "=r" (gid) : "I" (SYS_getgid)
                    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
		     "$10","$11","$12","$13","$14","$15","$24","$25");
  __asm__ volatile ("li $2,%1; syscall; move %0,$2"
                    : "=r" (egid) : "I" (SYS_getegid)
                    : "$2", "$3", "$4", "$5", "$6", "$7", "$8", "$9",
		     "$10","$11","$12","$13","$14","$15","$24","$25");

  	return (uid == euid && gid == egid);
}
