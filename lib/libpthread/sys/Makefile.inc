#	$OpenBSD: Makefile.inc,v 1.10 2003/01/20 18:12:11 marc Exp $

.PATH:	 ${SRCDIR}/sys ${SRCDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c
SRCS+=	_atomic_lock.c slow_atomic_lock.c

.if exists(${SRCDIR}/arch/${MACHINE_ARCH}/uthread_machdep_asm.S)
SRCS+=	uthread_machdep_asm.S
.endif
.if exists(${SRCDIR}/arch/${MACHINE_ARCH}/uthread_machdep.c)
SRCS+=	uthread_machdep.c
.endif

.if (${LIB} == "c_r") && (${ELF_TOOLCHAIN} == "no")

#
# All syscalls are renamed as _thread_sys_{syscall}.
# This is a list of syscalls that are renamed as _thread_sys_{syscall}
# so that libc_r can provide replacement functions.
#
HIDDEN_SYSCALLS= accept.o bind.o close.o connect.o dup.o dup2.o \
	execve.o fchflags.o fchmod.o fchown.o fcntl.o \
	flock.o fpathconf.o fstat.o fstatfs.o fsync.o getdirentries.o \
	getlogin.o getpeername.o getsockname.o getsockopt.o ioctl.o kevent.o \
	listen.o msync.o nanosleep.o open.o pipe.o poll.o read.o readv.o \
	recvfrom.o recvmsg.o sched_yield.o select.o sendmsg.o sendto.o \
	setsockopt.o shutdown.o sigaction.o sigaltstack.o \
	sigprocmask.o sigsuspend.o \
	socket.o socketpair.o wait4.o write.o writev.o \
	_exit.o

SRCS+= _sys_aliases.c
CLEANFILES += _sys_aliases.c

_sys_aliases.c: ${SRCDIR}/Makefile ${LIBCSRCDIR}/sys/Makefile.inc
	echo '#include <sys/cdefs.h>' > ${.TARGET}
	for fn in ${ASM:R} ${PSEUDO:R} ""; do \
		case $$fn in ${HIDDEN_SYSCALLS:.o=|}"") : stays hidden ;; \
			*) echo "__indr_reference(_thread_sys_$$fn,$$fn);";; \
		esac; \
	done >> ${.TARGET}
.endif
