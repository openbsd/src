#	$OpenBSD: Makefile.inc,v 1.140 2016/03/30 07:52:47 guenther Exp $
#	$NetBSD: Makefile.inc,v 1.35 1995/10/16 23:49:07 jtc Exp $
#	@(#)Makefile.inc	8.1 (Berkeley) 6/17/93

# sys sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_CPU}/sys ${LIBCSRCDIR}/sys

# modules with non-default implementations on at least one architecture:
SRCS+=	Ovfork.S brk.S cerror.S fork.S \
	sbrk.S sigpending.S sigprocmask.S sigreturn.S \
	sigsuspend.S syscall.S tfork_thread.S

# glue to offer userland wrappers for some syscalls
SRCS+=	posix_madvise.c pthread_sigmask.c \
	w_fork.c w_sigaction.c w_sigprocmask.c

# glue to provide compatibility between GCC 1.X and 2.X and for compat
# with old syscall interfaces.
SRCS+=	ftruncate.c lseek.c mquery.c mmap.c ptrace.c semctl.c truncate.c \
	timer_create.c timer_delete.c timer_getoverrun.c timer_gettime.c \
	timer_settime.c pread.c preadv.c pwrite.c pwritev.c

# stack protector helper functions
SRCS+=	stack_protector.c

# modules with default implementations on all architectures, unless overridden
# below:
ASM=	__get_tcb.o __getcwd.o __semctl.o __set_tcb.o __syscall.o \
	__threxit.o __thrsigdivert.o __thrsleep.o \
	__thrwakeup.o accept.o accept4.o access.o acct.o adjfreq.o adjtime.o \
	bind.o chdir.o chflags.o chflagsat.o chmod.o chown.o chroot.o \
	clock_getres.o clock_gettime.o clock_settime.o close.o \
	closefrom.o connect.o dup.o dup2.o dup3.o execve.o faccessat.o \
	fchdir.o fchflags.o fchmod.o fchmodat.o fchown.o \
	fchownat.o fcntl.o fhopen.o fhstat.o fhstatfs.o \
	flock.o fpathconf.o fstat.o fstatat.o fstatfs.o \
	fsync.o futimens.o futimes.o getentropy.o getdents.o getdtablecount.o \
	getegid.o geteuid.o getfh.o getfsstat.o getgid.o \
	getgroups.o getitimer.o getpeername.o getpgid.o getpgrp.o \
	getpid.o getppid.o getpriority.o getresgid.o getresuid.o \
	getrlimit.o getrtable.o getrusage.o getsid.o getsockname.o \
	getsockopt.o getthrid.o gettimeofday.o getuid.o ioctl.o \
	issetugid.o kevent.o kill.o kqueue.o ktrace.o lchown.o \
	link.o linkat.o listen.o lstat.o madvise.o mincore.o \
	minherit.o mkdir.o mkdirat.o mkfifo.o mkfifoat.o \
	mknod.o mknodat.o mlock.o mlockall.o mount.o mprotect.o \
	msgctl.o msgget.o msgrcv.o msgsnd.o msync.o munlock.o \
	munlockall.o munmap.o nanosleep.o nfssvc.o \
	open.o openat.o pathconf.o pipe.o pipe2.o pledge.o \
	poll.o ppoll.o profil.o pselect.o \
	quotactl.o read.o readlink.o readlinkat.o readv.o reboot.o \
	recvfrom.o recvmsg.o rename.o renameat.o revoke.o rmdir.o \
	sched_yield.o select.o semget.o semop.o sendmsg.o sendsyslog.o \
	sendto.o \
	setegid.o seteuid.o setgid.o setgroups.o setitimer.o setlogin.o \
	setpgid.o setpriority.o setregid.o setresgid.o setresuid.o \
	setreuid.o setrlimit.o setrtable.o setsid.o setsockopt.o \
	settimeofday.o setuid.o shmat.o shmctl.o shmdt.o \
	shmget.o shutdown.o sigaltstack.o socket.o \
	socketpair.o stat.o statfs.o swapctl.o symlink.o symlinkat.o \
	sync.o sysarch.o sysctl.o thrkill.o umask.o unlink.o unlinkat.o \
	unmount.o utimensat.o utimes.o utrace.o wait4.o write.o writev.o

SRCS+=	${SRCS_${MACHINE_CPU}}
.for i in ${SRCS_${MACHINE_CPU}}
ASM:=	${ASM:N${i:R}.o}
.endfor

GASM=	${ASM:.o=.go}
PASM=	${ASM:.o=.po}
SASM=	${ASM:.o=.so}
DASM=	${ASM:.o=.do}

PSEUDO=	_ptrace.o
GPSEUDO=${PSEUDO:.o=.go}
PPSEUDO=${PSEUDO:.o=.po}
SPSEUDO=${PSEUDO:.o=.so}
DPSEUDO=${PSEUDO:.o=.do}

PSEUDO_NOERR=	_exit.o getlogin_r.o
GPSEUDO_NOERR=${PSEUDO_NOERR:.o=.go}
PPSEUDO_NOERR=${PSEUDO_NOERR:.o=.po}
SPSEUDO_NOERR=${PSEUDO_NOERR:.o=.so}
DPSEUDO_NOERR=${PSEUDO_NOERR:.o=.do}

HIDDEN= sigaction.o
GHIDDEN=${HIDDEN:.o=.go}
PHIDDEN=${HIDDEN:.o=.po}
SHIDDEN=${HIDDEN:.o=.so}
DHIDDEN=${HIDDEN:.o=.do}

OBJS+=	${ASM} ${PSEUDO} ${PSEUDO_NOERR} ${HIDDEN}

SYS_DEP =  ${LIBCSRCDIR}/arch/${MACHINE_CPU}/SYS.h /usr/include/sys/syscall.h

# XXX only difference is -x removes FILE symbols?
TRIM=${LD} -x -r ${.TARGET}.o -o ${.TARGET} && rm -f ${.TARGET}.o
TRIM.debug=${LD} -X -r ${.TARGET}.o -o ${.TARGET} && rm -f ${.TARGET}.o


# Pick one of these to set what ASM is generated...
GENERATE.rsyscall=\
	echo creating ${.TARGET} && \
	printf '\#include "SYS.h"\nRSYSCALL(${.PREFIX})\n'
GENERATE.pseudo=\
	echo creating ${.TARGET} && \
	printf '\#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/^_//})\n'
GENERATE.pseudo_noerr=\
	echo creating ${.TARGET} && \
	printf '\#include "SYS.h"\nPSEUDO_NOERROR(${.PREFIX},${.PREFIX:S/^_//})\n'
GENERATE.rsyscall_hidden=\
	echo creating ${.TARGET} && \
	printf '\#include "SYS.h"\nRSYSCALL_HIDDEN(${.PREFIX:S/^_//})\n'

# ...and one of these to control how it's compiled
FINISH=	${COMPILE.S} ${CFLAGS:M-[ID]*} ${AINC} -x assembler-with-cpp -
FINISH.go=${FINISH} -o ${.TARGET}.o			&& ${TRIM.debug}
FINISH.po=${FINISH} -o ${.TARGET}.o -DPROF		&& ${TRIM.debug}
FINISH.so=${FINISH} -o ${.TARGET}   ${PICFLAG}
FINISH.do=${FINISH} -o ${.TARGET}.o ${DIST_CFLAGS}	&& ${TRIM}
FINISH.o =${FINISH} -o ${.TARGET}.o			&& ${TRIM}

# Use those to generate the desired syscall stubs
${GASM}: ${SYS_DEP} ; @${GENERATE.rsyscall} | ${FINISH.go}
${PASM}: ${SYS_DEP} ; @${GENERATE.rsyscall} | ${FINISH.po}
${SASM}: ${SYS_DEP} ; @${GENERATE.rsyscall} | ${FINISH.so}
${DASM}: ${SYS_DEP} ; @${GENERATE.rsyscall} | ${FINISH.do}
${ASM}:  ${SYS_DEP} ; @${GENERATE.rsyscall} | ${FINISH.o}

${GPSEUDO}: ${SYS_DEP} ; @${GENERATE.pseudo} | ${FINISH.go}
${PPSEUDO}: ${SYS_DEP} ; @${GENERATE.pseudo} | ${FINISH.po}
${SPSEUDO}: ${SYS_DEP} ; @${GENERATE.pseudo} | ${FINISH.so}
${DPSEUDO}: ${SYS_DEP} ; @${GENERATE.pseudo} | ${FINISH.do}
${PSEUDO}:  ${SYS_DEP} ; @${GENERATE.pseudo} | ${FINISH.o}

${GPSEUDO_NOERR}: ${SYS_DEP} ; @${GENERATE.pseudo_noerr} | ${FINISH.go}
${PPSEUDO_NOERR}: ${SYS_DEP} ; @${GENERATE.pseudo_noerr} | ${FINISH.po}
${SPSEUDO_NOERR}: ${SYS_DEP} ; @${GENERATE.pseudo_noerr} | ${FINISH.so}
${DPSEUDO_NOERR}: ${SYS_DEP} ; @${GENERATE.pseudo_noerr} | ${FINISH.do}
${PSEUDO_NOERR}:  ${SYS_DEP} ; @${GENERATE.pseudo_noerr} | ${FINISH.o}

${GHIDDEN}: ${SYS_DEP} ; @${GENERATE.rsyscall_hidden} | ${FINISH.go}
${PHIDDEN}: ${SYS_DEP} ; @${GENERATE.rsyscall_hidden} | ${FINISH.po}
${SHIDDEN}: ${SYS_DEP} ; @${GENERATE.rsyscall_hidden} | ${FINISH.so}
${DHIDDEN}: ${SYS_DEP} ; @${GENERATE.rsyscall_hidden} | ${FINISH.do}
${HIDDEN}:  ${SYS_DEP} ; @${GENERATE.rsyscall_hidden} | ${FINISH.o}


MAN+=	__get_tcb.2 __thrsigdivert.2 __thrsleep.2 _exit.2 accept.2 \
	access.2 acct.2 adjfreq.2 adjtime.2 bind.2 brk.2 chdir.2 \
	chflags.2 chmod.2 chown.2 chroot.2 clock_gettime.2 close.2 \
	closefrom.2 connect.2 dup.2 execve.2 fcntl.2 fhopen.2 flock.2 \
	fork.2 fsync.2 getentropy.2 getdents.2 getdtablecount.2 \
	getfh.2 getfsstat.2 getgid.2 getgroups.2 getitimer.2 getlogin.2 \
	getpeername.2 getpgrp.2 getpid.2 getpriority.2 getrlimit.2 \
	getrtable.2 getrusage.2 getsid.2 getsockname.2 getsockopt.2 \
	getthrid.2 gettimeofday.2 getuid.2 intro.2 ioctl.2 issetugid.2 \
	kbind.2 kill.2 kqueue.2 ktrace.2 link.2 listen.2 lseek.2 madvise.2 \
	mincore.2 minherit.2 mkdir.2 mkfifo.2 mknod.2 mlock.2 \
	mlockall.2 mmap.2 mount.2 mprotect.2 mquery.2 msgctl.2 \
	msgget.2 msgrcv.2 msgsnd.2 msync.2 munmap.2 nanosleep.2 \
	nfssvc.2 open.2 pathconf.2 pipe.2 pledge.2 poll.2 profil.2 \
	ptrace.2 quotactl.2 read.2 readlink.2 reboot.2 recv.2 \
	rename.2 revoke.2 rmdir.2 sched_yield.2 select.2 semctl.2 semget.2 \
	semop.2 send.2 setgroups.2 setpgid.2 setregid.2 \
	setresuid.2 setreuid.2 setsid.2 sendsyslog.2 setuid.2 shmat.2 \
	shmctl.2 shmget.2 shutdown.2 sigaction.2 sigaltstack.2 sigpending.2 \
	sigprocmask.2 sigreturn.2 sigsuspend.2 socket.2 \
	socketpair.2 stat.2 statfs.2 swapctl.2 symlink.2 \
	sync.2 sysarch.2 syscall.2 thrkill.2 truncate.2 umask.2 unlink.2 \
	utimes.2 utrace.2 vfork.2 wait.2 write.2
