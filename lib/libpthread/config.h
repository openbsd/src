/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

/* Does the OS already support struct timespec */
#define _OS_HAS_TIMESPEC 1

/* For networking code: an integral type the size of an IP address (4
   octets).  Determined by examining return values from certain
   functions.  */
#define pthread_ipaddr_type unsigned long

/* For networking code: an integral type the size of an IP port number
   (2 octets).  Determined by examining return values from certain
   functions.  */
#define pthread_ipport_type unsigned short

/* type of clock_t, from system header files */
#define pthread_clock_t long

/* Specially named so grep processing will find it and put it into the
   generated ac-types.h.  */
/* #undef pthread_have_va_list_h */

/* type of size_t, from system header files */
#define pthread_size_t unsigned int

/* type of ssize_t, from system header files */
#define pthread_ssize_t int

/* type of time_t, from system header files */
#define pthread_time_t long

/* type of fpos_t, from system header files */
#define pthread_fpos_t long

/* type of off_t, from system header files */
#define pthread_off_t long

/* type of va_list, from system header files */
#define pthread_va_list void *

/* I don't know why the native compiler definitions aren't sufficient
   for this.  */
/* #undef sunos4 */

/* define if the linker hauls in certain static data from libc even when
   you don't want it to.  yes, this description is bogus, but chris added
   the need for this, without describing the problem.  */
#define LD_LINKS_STATIC_DATA 1

/* define if the system reissues the SIGCHLD if the handler reinstalls
 * itself before calling wait()
 */
#define BROKEN_SIGNALS 1

/* where are terminal devices to be found? */
#define _PATH_PTY "/devices/pseudo/"

/* what directory holds the time zone info on this system? */
#define _PATH_TZDIR "/usr/share/lib/zoneinfo"

/* what file indicates the local time zone? */
#define _PATH_TZFILE "/usr/share/lib/zoneinfo/localtime"

/* Paths for various networking support files.  */
#define _PATH_RESCONF "/etc/resolv.conf"
#define _PATH_HOSTS "/etc/hosts"
#define _PATH_NETWORKS "/etc/networks"
#define _PATH_PROTOCOLS "/etc/protocols"
#define _PATH_SERVICES "/etc/services"

/* Path for Bourne shell.  */
#define _PATH_BSHELL "/bin/sh"

/* Define if you have the syscall_accept function.  */
/* #undef HAVE_SYSCALL_ACCEPT */

/* Define if you have the syscall_bind function.  */
/* #undef HAVE_SYSCALL_BIND */

/* Define if you have the syscall_chdir function.  */
#define HAVE_SYSCALL_CHDIR 1

/* Define if you have the syscall_chmod function.  */
#define HAVE_SYSCALL_CHMOD 1

/* Define if you have the syscall_chown function.  */
#define HAVE_SYSCALL_CHOWN 1

/* Define if you have the syscall_close function.  */
#define HAVE_SYSCALL_CLOSE 1

/* Define if you have the syscall_connect function.  */
/* #undef HAVE_SYSCALL_CONNECT */

/* Define if you have the syscall_creat function.  */
#define HAVE_SYSCALL_CREAT 1

/* Define if you have the syscall_dup function.  */
#define HAVE_SYSCALL_DUP 1

/* Define if you have the syscall_dup2 function.  */
/* #undef HAVE_SYSCALL_DUP2 */

/* Define if you have the syscall_execve function.  */
#define HAVE_SYSCALL_EXECVE 1

/* Define if you have the syscall_exit function.  */
#define HAVE_SYSCALL_EXIT 1

/* Define if you have the syscall_fchmod function.  */
#define HAVE_SYSCALL_FCHMOD 1

/* Define if you have the syscall_fchown function.  */
#define HAVE_SYSCALL_FCHOWN 1

/* Define if you have the syscall_fcntl function.  */
#define HAVE_SYSCALL_FCNTL 1

/* Define if you have the syscall_flock function.  */
/* #undef HAVE_SYSCALL_FLOCK */

/* Define if you have the syscall_fork function.  */
#define HAVE_SYSCALL_FORK 1

/* Define if you have the syscall_fstat function.  */
#define HAVE_SYSCALL_FSTAT 1

/* Define if you have the syscall_fstatfs function.  */
#define HAVE_SYSCALL_FSTATFS 1

/* Define if you have the syscall_ftruncate function.  */
/* #undef HAVE_SYSCALL_FTRUNCATE */

/* Define if you have the syscall_getdents function.  */
#define HAVE_SYSCALL_GETDENTS 1

/* Define if you have the syscall_getdirentries function.  */
/* #undef HAVE_SYSCALL_GETDIRENTRIES */

/* Define if you have the syscall_getdtablesize function.  */
/* #undef HAVE_SYSCALL_GETDTABLESIZE */

/* Define if you have the syscall_getmsg function.  */
#define HAVE_SYSCALL_GETMSG 1

/* Define if you have the syscall_getpeername function.  */
/* #undef HAVE_SYSCALL_GETPEERNAME */

/* Define if you have the syscall_getpgrp function.  */
/* #undef HAVE_SYSCALL_GETPGRP */

/* Define if you have the syscall_getsockname function.  */
/* #undef HAVE_SYSCALL_GETSOCKNAME */

/* Define if you have the syscall_getsockopt function.  */
/* #undef HAVE_SYSCALL_GETSOCKOPT */

/* Define if you have the syscall_ioctl function.  */
#define HAVE_SYSCALL_IOCTL 1

/* Define if you have the syscall_ksigaction function.  */
/* #undef HAVE_SYSCALL_KSIGACTION */

/* Define if you have the syscall_link function.  */
#define HAVE_SYSCALL_LINK 1

/* Define if you have the syscall_listen function.  */
/* #undef HAVE_SYSCALL_LISTEN */

/* Define if you have the syscall_lseek function.  */
#define HAVE_SYSCALL_LSEEK 1

/* Define if you have the syscall_lstat function.  */
#define HAVE_SYSCALL_LSTAT 1

/* Define if you have the syscall_open function.  */
#define HAVE_SYSCALL_OPEN 1

/* Define if you have the syscall_pgrpsys function.  */
#define HAVE_SYSCALL_PGRPSYS 1

/* Define if you have the syscall_pipe function.  */
#define HAVE_SYSCALL_PIPE 1

/* Define if you have the syscall_poll function.  */
#define HAVE_SYSCALL_POLL 1

/* Define if you have the syscall_putmsg function.  */
#define HAVE_SYSCALL_PUTMSG 1

/* Define if you have the syscall_read function.  */
#define HAVE_SYSCALL_READ 1

/* Define if you have the syscall_readdir function.  */
/* #undef HAVE_SYSCALL_READDIR */

/* Define if you have the syscall_readv function.  */
#define HAVE_SYSCALL_READV 1

/* Define if you have the syscall_recv function.  */
/* #undef HAVE_SYSCALL_RECV */

/* Define if you have the syscall_recvfrom function.  */
/* #undef HAVE_SYSCALL_RECVFROM */

/* Define if you have the syscall_recvmsg function.  */
/* #undef HAVE_SYSCALL_RECVMSG */

/* Define if you have the syscall_rename function.  */
#define HAVE_SYSCALL_RENAME 1

/* Define if you have the syscall_select function.  */
/* #undef HAVE_SYSCALL_SELECT */

/* Define if you have the syscall_send function.  */
/* #undef HAVE_SYSCALL_SEND */

/* Define if you have the syscall_sendmsg function.  */
/* #undef HAVE_SYSCALL_SENDMSG */

/* Define if you have the syscall_sendto function.  */
/* #undef HAVE_SYSCALL_SENDTO */

/* Define if you have the syscall_setsockopt function.  */
/* #undef HAVE_SYSCALL_SETSOCKOPT */

/* Define if you have the syscall_shutdown function.  */
/* #undef HAVE_SYSCALL_SHUTDOWN */

/* Define if you have the syscall_sigaction function.  */
#define HAVE_SYSCALL_SIGACTION 1

/* Define if you have the syscall_sigpause function.  */
/* #undef HAVE_SYSCALL_SIGPAUSE */

/* Define if you have the syscall_sigprocmask function.  */
#define HAVE_SYSCALL_SIGPROCMASK 1

/* Define if you have the syscall_sigsuspend function.  */
#define HAVE_SYSCALL_SIGSUSPEND 1

/* Define if you have the syscall_socket function.  */
/* #undef HAVE_SYSCALL_SOCKET */

/* Define if you have the syscall_socketcall function.  */
/* #undef HAVE_SYSCALL_SOCKETCALL */

/* Define if you have the syscall_socketpair function.  */
/* #undef HAVE_SYSCALL_SOCKETPAIR */

/* Define if you have the syscall_stat function.  */
#define HAVE_SYSCALL_STAT 1

/* Define if you have the syscall_unlink function.  */
#define HAVE_SYSCALL_UNLINK 1

/* Define if you have the syscall_wait3 function.  */
/* #undef HAVE_SYSCALL_WAIT3 */

/* Define if you have the syscall_wait4 function.  */
/* #undef HAVE_SYSCALL_WAIT4 */

/* Define if you have the syscall_waitpid function.  */
/* #undef HAVE_SYSCALL_WAITPID */

/* Define if you have the syscall_waitsys function.  */
#define HAVE_SYSCALL_WAITSYS 1

/* Define if you have the syscall_write function.  */
#define HAVE_SYSCALL_WRITE 1

/* Define if you have the syscall_writev function.  */
#define HAVE_SYSCALL_WRITEV 1

/* Define if you have the vfork function.  */
#define HAVE_VFORK 1

/* Define if you have the <alloc.h> header file.  */
/* #undef HAVE_ALLOC_H */

/* Define if you have the <sys/filio.h> header file.  */
#define HAVE_SYS_FILIO_H 1

/* Define if you have the <sys/syscall.h> header file.  */
#define HAVE_SYS_SYSCALL_H 1

/* Define if you have the <sys/termio.h> header file.  */
#define HAVE_SYS_TERMIO_H 1

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <syscall.h> header file.  */
/* #undef HAVE_SYSCALL_H */

/* Define if you have the <termio.h> header file.  */
#define HAVE_TERMIO_H 1

/* Define if you have the <termios.h> header file.  */
#define HAVE_TERMIOS_H 1
