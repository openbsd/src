#	from: @(#)Makefile.inc	5.6 (Berkeley) 6/4/91

.PATH: ${.CURDIR}/pthreads

SRCS+= cleanup.c cond.c fd.c fd_kern.c fd_pipe.c file.c globals.c malloc.c \
       mutex.c pthread.c pthread_attr.c queue.c signal.c machdep.c syscall.S \
       pthread_join.c pthread_detach.c pthread_once.c sleep.c specific.c \
       process.c wait.c errno.c schedparam.c _exit.c prio_queue.c \
       pthread_init.c init.cc sig.c info.c mutexattr.c select.c wrapper.c \
       dump_state.c pthread_kill.c condattr.c pthread_cancel.c panic.c

# glue to provide compatibility between GCC 1.X and 2.X and for compat
# with old syscall interfaces.
#SRCS+=	ftruncate.c lseek.c mmap.c semctl.c truncate.c \
#	timer_create.c timer_delete.c timer_getoverrun.c timer_gettime.c \
#	timer_settime.c

# modules with default implementations on all architectures:
ASM=	accept.o access.o acct.o adjtime.o bind.o chdir.o chflags.o chmod.o \
	chown.o chroot.o clock_gettime.o clock_settime.o clock_getres.o \
	close.o connect.o dup.o dup2.o execve.o fchdir.o \
	fchflags.o fchmod.o fchown.o fcntl.o flock.o fpathconf.o fstat.o \
	fstatfs.o fsync.o futimes.o getdirentries.o getegid.o geteuid.o \
	getfh.o getfsstat.o getgid.o getgroups.o getitimer.o getpeername.o \
	getpgid.o \
	getpgrp.o getpid.o getppid.o getpriority.o getrlimit.o getrusage.o \
	getsid.o \
	getsockname.o getsockopt.o gettimeofday.o getuid.o issetugid.o \
	ioctl.o kill.o \
	ktrace.o lchown.o lfs_bmapv.o lfs_markv.o lfs_segclean.o lfs_segwait.o \
	link.o listen.o lstat.o madvise.o mincore.o minherit.o mkdir.o \
	mkfifo.o mknod.o mlock.o \
	mount.o mprotect.o msgctl.o msgget.o msgrcv.o msgsnd.o msync.o \
	munlock.o munmap.o nanosleep.o nfssvc.o \
	ntp_gettime.o ntp_adjtime.o \
	open.o pathconf.o poll.o profil.o quotactl.o \
	read.o readlink.o readv.o reboot.o recvfrom.o recvmsg.o rename.o \
	revoke.o rfork.o \
	rmdir.o select.o semconfig.o semget.o semop.o sendmsg.o sendto.o \
	setegid.o seteuid.o setgid.o setgroups.o setitimer.o setpgid.o \
	setpriority.o setrlimit.o setsid.o setsockopt.o settimeofday.o \
	setuid.o shmat.o shmctl.o shmdt.o shmget.o shutdown.o sigaction.o \
	sigaltstack.o socket.o socketpair.o stat.o statfs.o swapon.o \
	symlink.o sync.o sysarch.o umask.o undelete.o unlink.o unmount.o \
	utimes.o vadvise.o wait4.o write.o writev.o __semctl.o \
	__sysctl.o

PASM=	${ASM:.o=.po}
SASM=	${ASM:.o=.so}

PSEUDO=	_exit.o _getlogin.o
PPSEUDO=${PSEUDO:.o=.po}
SPSEUDO=${PSEUDO:.o=.so}

OBJS+=	${ASM} ${PSEUDO}

${PASM}: ${.CURDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} -DPROF ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.if (${MACHINE_ARCH} != "mips")
${SASM}: ${.CURDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -k -o ${.TARGET}
.else
${SASM}: ${.CURDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}
.endif

${ASM}: ${.CURDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nRSYSCALL(${.PREFIX})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}.o
	@${LD} -x -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

${PPSEUDO}: ${.CURDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} -DPROF ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o

.if (${MACHINE_ARCH} != "mips")
${SPSEUDO}: ${.CURDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -k -o ${.TARGET}
.else
${SPSEUDO}: ${.CURDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} -DPIC ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}
.endif

${PSEUDO}: ${.CURDIR}/arch/${MACHINE_ARCH}/SYS.h /usr/include/sys/syscall.h
	@echo creating ${.TARGET}
	@printf '#include "SYS.h"\nPSEUDO(${.PREFIX},${.PREFIX:S/_//})\n' | \
	    ${CPP} ${CFLAGS:M-[ID]*} ${AINC} | ${AS} -o ${.TARGET}.o
	@${LD} -X -r ${.TARGET}.o -o ${.TARGET}
	@rm -f ${.TARGET}.o
