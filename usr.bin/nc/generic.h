/* generic.h -- anything you don't #undef at the end remains in effect.
   The ONLY things that go in here are generic indicator flags; it's up
   to your programs to declare and call things based on those flags.

   You should only need to make changes via a minimal system-specific section
   at the end of this file.  To build a new section, rip through this and
   check everything it mentions on your platform, and #undef that which needs
   it.  If you generate a system-specific section you didn't find in here,
   please mail me a copy so I can update the "master".

   I realize I'm probably inventing another pseudo-standard here, but
   goddamnit, everybody ELSE has already, and I can't include all of their
   hairball schemes too.  HAVE_xx conforms to the gnu/autoconf usage and
   seems to be the most common format.  In fact, I dug a lot of these out
   of autoconf and tried to common them all together using "stupidh" to
   collect data from platforms.

   In disgust...  _H*  940910, 941115, 950511.  Pseudo-version: 1.3

   Updated 951104 with many patches from netcat feedback, and properly
   closed a lot of slop in open-ended comments: version 1.4
   960217 + nextstep: version 1.5
*/

#ifndef GENERIC_H		/* only run through this once */
#define GENERIC_H

/* =============================== */
/* System calls, lib routines, etc */
/* =============================== */

/* How does your system declare malloc, void or char?  Usually void, but go
   ask the SunOS people why they had to be different... */
#define VOID_MALLOC

/* notably from fwtk/firewall.h: posix locking? */
#define HAVE_FLOCK		/* otherwise it's lockf() */

/* if you don't have setsid(), you might have setpgrp(). */
#define HAVE_SETSID

/* random() is generally considered better than rand() */
#define HAVE_RANDOM

/* the srand48/lrand48/etc family is s'posedly even better */
#define HAVE_RAND48
/* bmc@telebase and others have suggested these macros if a box *does* have
   rand48.  Will consider for later if we're doing something that really
   requires stronger random numbers, but netcat and such certainly doesn't.
#define srandom(seed) srand48((long) seed)
#define random()      lrand48() */

/* if your machine doesn't have lstat(), it should have stat() [dos...] */
#define HAVE_LSTAT

/* different kinds of term ioctls.  How to recognize them, very roughly:
   sysv/POSIX_ME_HARDER:  termio[s].h, struct termio[s], tty.c_*[]
   bsd/old stuff:  sgtty.h, ioctl(TIOCSETP), sgttyb.sg_*, tchars.t_*  */
#define HAVE_TERMIOS

/* dbm vs ndbm */
#define HAVE_NDBM

/* extended utmp/wtmp stuff.  MOST machines still do NOT have this SV-ism */
#define UTMPX

/* some systems have nice() which takes *relative* values... [resource.h] */
#define HAVE_SETPRIORITY

/* a sysvism, I think, but ... */
#define HAVE_SYSINFO

/* ============= */
/* Include files */
/* ============= */

/* Presence of these can be determined via a script that sniffs them
   out if you aren't sure.  See "stupidh"... */

/* stdlib comes with most modern compilers, but ya never know */
#define HAVE_STDLIB_H

/* not on a DOS box! */
#define HAVE_UNISTD_H

/* stdarg is a weird one */
#define HAVE_STDARG_H

/* dir.h or maybe ndir.h otherwise. */
#define HAVE_DIRENT_H

/* string or strings */
#define HAVE_STRINGS_H

/* if you don't have lastlog.h, what you want might be in login.h */
#define HAVE_LASTLOG_H

/* predefines for _PATH_various */
#define HAVE_PATHS_H

/* some SV-flavors break select stuff out separately */
#define HAVE_SELECT_H

/* assorted others */
#define HAVE_PARAM_H		/* in sys/ */
#define HAVE_SYSMACROS_H	/* in sys/ */
#define HAVE_TTYENT_H		/* securetty et al */

/* ==================== */

/* Still maybe have to do something about the following, if it's even
   worth it.  I just grepped a lot of these out of various code, without
   looking them up yet:

#define HAVE_EINPROGRESS
#define HAVE_F_SETOWN
HAVE_FILIO_H ... fionbio, fiosetown, etc... will need for hairier
  select loops.
#define HAVE_SETENV ... now *there's* a hairy one; **environ is portable
#define BIG_ENDIAN/little_endian ... *please* try to avoid this stupidity
   and LSBFIRST/MSBFIRST
#define HAVE_GETUSERSHELL ... you could always pull it out of getpwent()
#define HAVE_SETE[UG]ID ... lib or syscall, it varies on diff platforms
#define HAVE_STRCHR ... should actually be handled by string/strings
#define HAVE_PSTAT
#define HAVE_ST_BLKSIZE ... a stat() thing?
#define HAVE_IP_TOS
#define HAVE_STRFTIME ... screw this, we'll just INCLUDE one for lame
   old boxes that don't have it [sunos 3.x, early 4.x?]
#define HAVE_VFPRINTF
#define HAVE_SHADOW_PASSWD  ... in its multitudinous schemes?? ... how
   about sumpin' like #define SHADOW_PASSWD_TYPE ... could get grody.
   ... looks like sysv /etc/shadow, getspent() family is common.
#define SIG*  ... what a swamp, punt for now; should all be in signal.h
#define HAVE_STRCSPN  ... see larry wall's comment in the fwtk regex code
#define ULTRIX_AUTH  ... bwahaha.
#define HAVE_YP  or NIS or whatever you wanna call it this week
randomness about VARARGS??
--- later stuff to be considered ---
#define UINT4 ... u-int on alpha/osf, i.e. __alpha/__osf__, ulong elsewhere?
   dont name it that, though, it'll conflict with extant .h files like md5
randomness about machine/endian.h, machine/inline.h -- bsdi, net/2
randomness about _PATH_WTMP vs WTMP_FILE and where they even live!!
#define HAVE_SYS_ERRLIST ... whether it's in stdio.h or not [bsd 4.4]
--- still more stuff
#define HAVE_SETENV
#define _PATH_UTMP vs UTMP_FILE, a la deslogind?!
#define HAVE_DAEMON
#define HAVE_INETADDR  [vixie bind?]
lseek: SEEK_SET vs L_SET and associated lossage [epi-notes, old 386Mach]
bsdi: ioctl_compat.h ?
--- takin' some ifdefs from CNS krb:
F_GETOWN/F_SETOWN
CRAY: long = 8 bytes, etc  [class with alpha?]
CGETENT
SIGINFO
SIGTSTP SIGTTOU SIGWINCH
SPX?
SYSV_TERMIO -- covered elsewhere, I hope
TIOCEXT TIOCFLUSH TIOC[GS]WINSIZ 
NEWINIT: something about init cleaning up dead login processes [telnet?]
PARENT_DOES_UTMP, too  [telnet]
VDISCARD
VEOL/VEOL2/VLNEXT VREPRINT -- termios stuff?, and related...
STREAMSPTY/STREAMSPTYEM
AF_INET/AF_UNSPEC, PF_*
ECHOCTL/ECHOKE
F_ULOCK [?!]
setpgrp/getpgrp() ONEARG business..
HAVE_ALLOCA
HAVE_GETUTENT
HAVE_SYS_SELECT_H  [irix!]
HAVE_DIRENT  [old 386mach has *direct.h*!]
HAVE_SIGSET
HAVE_VFORK_H and HAVE_VFORK
HAVE_VHANGUP
HAVE_VSPRINTF
HAVE_IPTOS_*
HAVE_STRCASECMP, STRNCASECMP
HAVE_SYS_FCNTL_H
HAVE_SYS_TIME_H
HAVE_UTIMES
NOTTYENT [?]
HAVE_FCHMOD
HAVE_GETUSERSHELL
HAVE_SIGCONTEXT  [stack hair, very machine-specific]
YYLINENO?
POSIX_SIGNALS
POSIX_TERMIOS
SETPROCTITLE -- breaks some places, like fbsd sendmail
SIG* -- actual signal names?  some are missing
SIOCGIFCONF
SO_BROADCAST
SHMEM  [krb tickets]
VARARGS, or HAVE_VARARGS
CBAUD
... and B300, B9600, etc etc
HAVE_BZERO  vs  memset/memcpy
HAVE_SETVBUF
HAVE_STRDUP
HAVE_GETENV
HAVE_STRSAVE
HAVE_STBLKSIZE  [stat?]
HAVE_STREAM_H -- in sys/, ref sendmail 8.7 for IP_SRCROUTE
FCHMOD
INITGROUPS -- most machines seem to *have*
SETREUID
SNPRINTF
SETPGRP semantics bsd vs. sys5 style

There's also the issue about WHERE various .h files live, sys/ or otherwise.
There's a BIG swamp lurking where network code of any sort lives.
*/

/* ======================== */
/* System-specific sections */
/* ======================== */

/* By turning OFF various bits of the above,  you can customize for
   a given platform.  Yes, we're ignoring the stock compiler predefines
   and using our own plugged in via the Makefile. */

/* DOS boxes, with MSC; you may need to adapt to a different compiler. */
/* looks like later ones *do* have dirent.h, for example */
#ifdef MSDOS
#undef HAVE_FLOCK
#undef HAVE_RANDOM
#undef HAVE_LSTAT
#undef HAVE_TERMIOS
#undef UTMPX
#undef HAVE_SYSINFO
#undef HAVE_UNISTD_H
#undef HAVE_DIRENT_H	/* unless you have the k00l little wrapper from L5!! */
#undef HAVE_STRINGS_H
#undef HAVE_LASTLOG_H
#undef HAVE_PATHS_H
#undef HAVE_PARAM_H
#undef HAVE_SYSMACROS_H
#undef HAVE_SELECT_H
#undef HAVE_TTYENT_H
#endif /* MSDOS */

/* buglix 4.x; dunno about 3.x on down.  should be bsd4.2 */
#ifdef ULTRIX
#undef UTMPX
#undef HAVE_PATHS_H
#undef HAVE_SYSMACROS_H
#undef HAVE_SELECT_H
#endif /* buglix */

/* some of this might still be broken on older sunoses */
#ifdef SUNOS
#undef VOID_MALLOC
#undef UTMPX
#undef HAVE_PATHS_H
#undef HAVE_SELECT_H
#endif /* sunos */

/* "contact your vendor for a fix" */
#ifdef SOLARIS
/* has UTMPX */
#undef HAVE_RANDOM
#undef HAVE_SETPRIORITY
#undef HAVE_STRINGS_H	/* this is genuinely the case, go figure */
#undef HAVE_PATHS_H
#undef HAVE_SELECT_H
#undef HAVE_TTYENT_H
#endif /* SOLARIS */

/* whatever aix variant MIT had at the time; 3.2.x?? */
#ifdef AIX
#undef UTMPX
#undef HAVE_LASTLOG_H
#define HAVE_LOGIN_H	/* "special", in the educational sense */
#endif /* aix */

/* linux, which is trying as desperately as the gnu folks can to be
   POSIXLY_CORRECT.  I think I'm gonna hurl... */
#ifdef LINUX
#undef UTMPX
#undef HAVE_SYSINFO
#undef HAVE_SELECT_H
#undef HAVE_TTYENT_H
#endif /* linux */

/* irix 5.x; may not be correct for earlier ones */
#ifdef IRIX
/* wow, does irix really have everything?! */
#endif /* irix */

/* osf on alphas */
#ifdef OSF
#undef UTMPX
#undef HAVE_SELECT_H
#endif /* osf */

/* they's some FUCKED UP paths in this one! */
#ifdef FREEBSD
#undef UTMPX
#undef HAVE_SYSINFO
#undef HAVE_LASTLOG_H
#undef HAVE_SYSMACROS_H
#undef HAVE_SELECT_H	/* actually a lie, but only for kernel */
#endif /* freebsd */

/* Originally from the sidewinder site, of all places, but subsequently
   checked further under a more normal bsdi 2.0 */
#ifdef BSDI
#undef UTMPX
#undef HAVE_LASTLOG_H
#undef HAVE_SYSMACROS_H
/* and their malloc.h was in sys/ ?! */
#undef HAVE_SELECT_H
#endif /* bsdi */

/* netbsd/44lite, jives with amiga-netbsd from cactus */
#ifdef NETBSD
#undef UTMPX
#undef HAVE_SYSINFO
#undef HAVE_LASTLOG_H
#undef HAVE_SELECT_H
#endif /* netbsd */

/* Hpux 9.0x, from BBN and various patches sent in */
#ifdef HPUX
#undef HAVE_RANDOM	/* but *does* have ?rand48 -- need to consider.. */
#undef HAVE_UTMPX
#undef HAVE_LASTLOG_H	/* has utmp/wtmp/btmp nonsense, and pututline() */
#undef HAVE_PATHS_H
#undef HAVE_SELECT_H
#undef HAVE_TTYENT_H
#endif /* hockeypux */

/* Unixware [a loose definition of "unix", to be sure], 1.1.2 [at least]
   from Brian Clapper.  He wasn't sure about 2.0... */
#ifdef UNIXWARE
/* has UTMPX */
#undef HAVE_SETPRIORITY
/* NOTE: UnixWare does provide the BSD stuff, in "/usr/ucbinclude" (headers)
   and "/usr/ucblib" (libraries).  However, I've run into problems linking
   stuff out of that version of the C library, when objects are also coming
   out of the "regular" C library.  My advice: Avoid the BSD compatibility
   stuff wherever possible.  Brian Clapper <bmc@telebase.com> */
#undef HAVE_STRINGS_H
#undef HAVE_PATHS_H
#undef HAVE_TTYENT_H
#endif /* UNIXWARE */

/* A/UX 3.1.x from darieb@sandia.gov */
#ifdef AUX
#undef HAVE_RANDOM
#undef HAVE_SELECT_H	/* xxx: untested */
#endif /* a/ux */

/* NeXTSTEP 3.2 motorola mudge@l0pht.com xxx should also work with
   white hardware and Sparc/HPPA. Should work with 3.3 too as it's
   4.3 / 4.4 bsd wrapped around mach */
#ifdef NEXT
#undef UTMPX
#undef HAVE_SELECT_X
#endif /* NeXTSTEP 3.2 motorola */

/* Make some "generic" assumptions if all else fails */
#ifdef GENERIC
#undef HAVE_FLOCK
#if defined(SYSV) && (SYSV < 4)  /* TW leftover: old SV doesnt have symlinks */
#undef HAVE_LSTAT
#endif /* old SYSV */
#undef HAVE_TERMIOS
#undef UTMPX
#undef HAVE_PATHS_H
#undef HAVE_SELECT_H
#endif /* generic */

/* ================ */
#endif /* GENERIC_H */

