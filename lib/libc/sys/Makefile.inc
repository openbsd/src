#	$OpenBSD: Makefile.inc,v 1.63 2002/12/02 15:54:14 millert Exp $
#	$NetBSD: Makefile.inc,v 1.35 1995/10/16 23:49:07 jtc Exp $
#	@(#)Makefile.inc	8.1 (Berkeley) 6/17/93

# sys sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/sys ${LIBCSRCDIR}/sys

# modules with non-default implementations on at least one architecture:
SRCS+=	Ovfork.S brk.S cerror.S exect.S fork.S rfork.S \
	sbrk.S sigpending.S sigprocmask.S sigreturn.S \
	sigsuspend.S syscall.S

LSRCS+=	Lint_Ovfork.c Lint_brk.c Lint_exect.c Lint_fork.c \
	Lint_sbrk.c \
	Lint_sigpending.c Lint_sigprocmask.c Lint_sigreturn.c \
	Lint_sigsuspend.c Lint_syscall.c
DPSRCS+= Lint_Ovfork.c Lint_brk.c Lint_exect.c Lint_fork.c \
	Lint_sbrk.c \
	Lint_sigpending.c Lint_sigprocmask.c Lint_sigreturn.c \
	Lint_sigsuspend.c Lint_syscall.c

# glue to provide compatibility between GCC 1.X and 2.X and for compat
# with old syscall interfaces.
SRCS+=	ftruncate.c lseek.c mmap.c ptrace.c semctl.c truncate.c \
	timer_create.c timer_delete.c timer_getoverrun.c timer_gettime.c \
	timer_settime.c pread.c preadv.c pwrite.c pwritev.c

# stack protector helper functions
SRCS+=	stack_protector.c

# modules with default implementations on all architectures:
ASM=	accept.o access.o acct.o adjtime.o bind.o chdir.o chflags.o chmod.o \
	chown.o chroot.o clock_gettime.o clock_settime.o clock_getres.o \
	close.o connect.o dup.o dup2.o execve.o fchdir.o \
	fchflags.o fchmod.o fchown.o fcntl.o fhopen.o fhstat.o fhstatfs.o \
	flock.o fpathconf.o fstat.o \
	fstatfs.o fsync.o futimes.o getdirentries.o getegid.o geteuid.o \
	getfh.o getfsstat.o getgid.o getgroups.o getitimer.o getpeereid.o \
	getpeername.o \
	getpgid.o \
	getpgrp.o getpid.o getppid.o getpriority.o getresgid.o getresuid.o \
	getrlimit.o getrusage.o getsid.o \
	getsockname.o getsockopt.o gettimeofday.o getuid.o issetugid.o \
	ioctl.o kill.o kevent.o kqueue.o \
	ktrace.o lchown.o lfs_bmapv.o lfs_markv.o lfs_segclean.o lfs_segwait.o \
	link.o listen.o lstat.o madvise.o mincore.o minherit.o mkdir.o \
	mkfifo.o mknod.o mlock.o mlockall.o \
	mount.o mprotect.o msgctl.o msgget.o msgrcv.o msgsnd.o msync.o \
	munlock.o munlockall.o munmap.o nanosleep.o nfssvc.o \
	open.o pathconf.o pipe.o poll.o profil.o quotactl.o \
	read.o readlink.o readv.o reboot.o recvfrom.o recvmsg.o rename.o \
	revoke.o rmdir.o select.o semget.o semop.o sendmsg.o sendto.o \
	setegid.o seteuid.o setgid.o setgroups.o setitimer.o setpgid.o \
	setpriority.o setresgid.o setresuid.o setrlimit.o setsid.o \
	setsockopt.o settimeofday.o \
	setuid.o shmat.o shmctl.o shmdt.o shmget.o shutdown.o sigaction.o \
	sigaltstack.o socket.o socketpair.o stat.o statfs.o swapon.o swapctl.o \
	symlink.o sync.o sysarch.o umask.o undelete.o unlink.o unmount.o \
	utimes.o vadvise.o wait4.o write.o writev.o xfspioctl.o __semctl.o \
	__syscall.o __sysctl.o

ASM+=	extattrctl.o extattr_set_file.o extattr_get_file.o \
	extattr_delete_file.o extattr_set_fd.o extattr_get_fd.o \
	extattr_delete_fd.o

GASM=	${ASM:.o=.go}
PASM=	${ASM:.o=.po}
SASM=	${ASM:.o=.so}

PSEUDO=	_getlogin.o _setlogin.o _ptrace.o
GPSEUDO=${PSEUDO:.o=.go}
PPSEUDO=${PSEUDO:.o=.po}
SPSEUDO=${PSEUDO:.o=.so}

PSEUDO_NOERR=	_exit.o
GPSEUDO_NOERR=${PSEUDO_NOERR:.o=.go}
PPSEUDO_NOERR=${PSEUDO_NOERR:.o=.po}
SPSEUDO_NOERR=${PSEUDO_NOERR:.o=.so}

OBJS+=	${ASM} ${PSEUDO} ${PSEUDO_NOERR}

${GASM}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

${PASM}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} -DPROF ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.if (${MACHINE_ARCH} != "mips")
${SASM}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | \
	    ${AS} ${ASPICFLAG} -o ${.TARGET}
.else
${SASM}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}
.endif

${ASM}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

${GPSEUDO}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

${PPSEUDO}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} -DPROF ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.if (${MACHINE_ARCH} != "mips")
${SPSEUDO}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | \
	    ${AS} ${ASPICFLAG} -o ${.TARGET}
.else
${SPSEUDO}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}
.endif

${PSEUDO}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

${GPSEUDO_NOERR}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO_NOERROR(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

${PPSEUDO_NOERR}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO_NOERROR(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} -DPROF ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.if (${MACHINE_ARCH} != "mips")
${SPSEUDO_NOERR}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO_NOERROR(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | \
	    ${AS} ${ASPICFLAG} -o ${.TARGET}
.else
${SPSEUDO_NOERR}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO_NOERROR(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}
.endif

${PSEUDO_NOERR}: ${LIBCSRCDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO_NOERROR(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

# Lint stubs for all automatically-generated assembly stubs (GENERATED)
#LSRCS+=	LintSysNormal.c LintSysNoerr.c LintSysPseudoNoerr.c
#DPSRCS+= LintSysNormal.c LintSysNoerr.c LintSysPseudoNoerr.c
LSRCS+=	LintSysNormal.c
DPSRCS+= LintSysNormal.c

CLEANFILES+= LintSysNormal.c LintSysNoerr.c LintSysPseudo.c

LintSysNormal.c: ${LIBCSRCDIR}/sys/makelintstub.sh \
    ${DESTDIR}/usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@sh ${LIBCSRCDIR}/sys/makelintstub.sh -o ${.TARGET} \
	    -s ${DESTDIR}/usr/include/sys/syscall.h ${ASM}

LintSysNoerr.c: ${LIBCSRCDIR}/sys/makelintstub.sh \
    ${DESTDIR}/usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@sh ${LIBCSRCDIR}/sys/makelintstub.sh -o ${.TARGET} -n \
	    -s ${DESTDIR}/usr/include/sys/syscall.h ${NOERR}

LintSysPseudoNoerr.c: ${LIBCSRCDIR}/sys/makelintstub.sh \
    ${DESTDIR}/usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@sh ${LIBCSRCDIR}/sys/makelintstub.sh -o ${.TARGET} -p \
	    -s ${DESTDIR}/usr/include/sys/syscall.h ${PSEUDONOERR}

MAN+=	accept.2 access.2 acct.2 adjtime.2 bind.2 brk.2 chdir.2 chflags.2 \
	chmod.2 chown.2 chroot.2 clock_gettime.2 close.2 connect.2 dup.2 \
	execve.2 _exit.2 fcntl.2 fhopen.2 \
	flock.2 fork.2 fsync.2 getdirentries.2 \
	getfh.2 getfsstat.2 getgid.2 getgroups.2 getitimer.2 getlogin.2 \
	getpeername.2 getpeereid.2 getpgrp.2 \
	getpid.2 getpriority.2 getrlimit.2 getrusage.2 getsid.2 getsockname.2 \
	getsockopt.2 gettimeofday.2 getuid.2 intro.2 issetugid.2 ioctl.2 \
	kill.2 kqueue.2 ktrace.2 link.2 \
	listen.2 lseek.2 mkdir.2 mkfifo.2 mknod.2 madvise.2 mincore.2 \
	minherit.2 mlock.2 mlockall.2 \
	mmap.2 mount.2 mprotect.2 msync.2 munmap.2 nanosleep.2 \
	nfssvc.2 open.2 pathconf.2 \
	pipe.2 profil.2 poll.2 ptrace.2 quotactl.2 read.2 readlink.2 reboot.2 \
	recv.2 rename.2 revoke.2 rfork.2 rmdir.2 select.2 send.2 setgroups.2 \
	setpgid.2 setresuid.2 setsid.2 setuid.2 shutdown.2 sigaction.2 \
	sigaltstack.2 sigpending.2 sigprocmask.2 sigreturn.2 sigstack.2 \
	sigsuspend.2 socket.2 socketpair.2 stat.2 statfs.2 swapctl.2 symlink.2 \
	sync.2 sysarch.2 syscall.2 truncate.2 umask.2 unlink.2 utimes.2 \
	vfork.2 wait.2 write.2

MAN+=	extattr_get_file.2
MLINKS+=extattr_get_file.2 extattr_set_file.2 \
	extattr_get_file.2 extattr_delete_file.2 \
	extattr_get_file.2 extattr_get_fd.2 \
	extattr_get_file.2 extattr_set_fd.2 \
	extattr_get_file.2 extattr_delete_fd.2 \
	extattr_get_file.2 extattr.2

MAN+=	msgctl.2 shmctl.2 shmat.2 semop.2 semget.2 semctl.2 msgsnd.2 msgrcv.2 \
	msgget.2 shmget.2

MLINKS+=brk.2 sbrk.2
MLINKS+=dup.2 dup2.2
MLINKS+=chdir.2 fchdir.2
MLINKS+=chflags.2 fchflags.2
MLINKS+=chmod.2 fchmod.2
MLINKS+=chown.2 fchown.2
MLINKS+=chown.2 lchown.2
MLINKS+=clock_gettime.2 clock_settime.2
MLINKS+=clock_gettime.2 clock_getres.2
MLINKS+=execve.2 exect.2
MLINKS+=fhopen.2 fhstat.2 fhopen.2 fhstatfs.2
MLINKS+=getgid.2 getegid.2
MLINKS+=getitimer.2 setitimer.2
MLINKS+=getitimer.2 timeradd.3
MLINKS+=getitimer.2 timerclear.3
MLINKS+=getitimer.2 timercmp.3
MLINKS+=getitimer.2 timerisset.3
MLINKS+=getitimer.2 timersub.3
MLINKS+=getlogin.2 getlogin_r.2 getlogin.2 setlogin.2
MLINKS+=getpgrp.2 getpgid.2
MLINKS+=getpid.2 getppid.2
MLINKS+=getpriority.2 setpriority.2
MLINKS+=getrlimit.2 setrlimit.2
MLINKS+=getsockopt.2 setsockopt.2
MLINKS+=gettimeofday.2 settimeofday.2
MLINKS+=getuid.2 geteuid.2
MLINKS+=kqueue.2 kevent.2
MLINKS+=intro.2 errno.2
MLINKS+=mlock.2 munlock.2
MLINKS+=mlockall.2 munlockall.2
MLINKS+=mount.2 unmount.2
MLINKS+=pathconf.2 fpathconf.2
MLINKS+=read.2 readv.2 read.2 pread.2 read.2 preadv.2
MLINKS+=recv.2 recvfrom.2 recv.2 recvmsg.2
MLINKS+=send.2 sendmsg.2 send.2 sendto.2
MLINKS+=setpgid.2 setpgrp.2
MLINKS+=setuid.2 setegid.2 setuid.2 seteuid.2 setuid.2 setgid.2
MLINKS+=setresuid.2 getresgid.2 setresuid.2 getresuid.2
MLINKS+=setresuid.2 setresgid.2
MLINKS+=shmat.2 shmdt.2
MLINKS+=stat.2 fstat.2 stat.2 lstat.2
MLINKS+=statfs.2 fstatfs.2
MLINKS+=syscall.2 __syscall.2
MLINKS+=truncate.2 ftruncate.2
MLINKS+=utimes.2 futimes.2
MLINKS+=wait.2 wait3.2 wait.2 wait4.2 wait.2 waitpid.2
MLINKS+=write.2 writev.2 write.2 pwrite.2 write.2 pwritev.2
MLINKS+=select.2 FD_SET.3 select.2 FD_CLR.3
MLINKS+=select.2 FD_ISSET.3 select.2 FD_ZERO.3
