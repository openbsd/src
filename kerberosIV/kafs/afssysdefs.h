/*	$Id: afssysdefs.h,v 1.1.1.1 1995/12/14 06:52:46 tholo Exp $	*/

/*
 * This section is for machines using single entry point AFS syscalls!
 * or
 * This section is for machines using multiple entry point AFS syscalls!
 */

#if defined(sun) && !defined(__svr4__)
#define AFS_SYSCALL	31
#endif

#if defined(sun) && defined(__svr4__)
#define AFS_SYSCALL	105
#endif

#if defined(hpux)
#define AFS_SYSCALL	50
#define AFS_SYSCALL2	49
#endif

#if defined(_AIX)
/* _AIX is too weird */
#endif

#if defined(sgi)
#define AFS_PIOCTL      (64+1000)
#define AFS_SETPAG      (65+1000)
#endif

#if defined(__osf__)
#define AFS_SYSCALL	232
#define AFS_SYSCALL2	258
#endif
