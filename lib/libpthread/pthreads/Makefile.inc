#	from: @(#)Makefile.inc	5.6 (Berkeley) 6/4/91

.PATH: ${.CURDIR}/pthreads

SRCS+= cleanup.c cond.c fd.c fd_kern.c fd_pipe.c file.c globals.c malloc.c \
       mutex.c pthread.c pthread_attr.c queue.c signal.c machdep.c syscall.S \
       pthread_join.c pthread_detach.c pthread_once.c sleep.c specific.c \
       process.c wait.c errno.c schedparam.c _exit.c prio_queue.c \
       pthread_init.c init.cc sig.c info.c mutexattr.c select.c wrapper.c \
       dump_state.c pthread_kill.c condattr.c pthread_cancel.c panic.c

OBJS+= syscalls.o

AVAILABLE_SYSCALLS =  open write read creat close fcntl lseek dup pipe fchmod fchown execve fstat lstat link unlink chdir chown chmod stat rename ioctl fstatfs sigsuspend sigaction sigprocmask getdents waitsys poll putmsg getmsg pgrpsys exit readv writev fork

SYSCALL_TEMPLATE = ${.CURDIR}/arch/${MACHINE}/syscall-template.S
SYSCALL_S = ${.CURDIR}/arch/${MACHINE}/syscall.S

.if !defined(NOPIC)
SOBJS+= syscalls.so
SYSCALL_PIC_COMPILE= $(CC) $(CFLAGS) -DSYSCALL_NAME=$$syscall -DPIC -c ${SYSCALL_TEMPLATE} -o ${.OBJDIR}/syscalls/S$$syscall.so
.else
SYSCALL_PIC_COMPILE= true
.endif

.if !defined(NOPROFILE)
POBJS+= syscalls.po
SYSCALL_PROF_COMPILE= $(CC) $(CFLAGS) -DSYSCALL_NAME=$$syscall -pg -c ${SYSCALL_TEMPLATE} -o ${.OBJDIR}/syscalls/S$$syscall.po
.else
SYSCALL_PROF_COMPILE= true
.endif

syscalls.o syscalls.so syscalls.po : ${SYSCALL_TEMPLATE}
	-rm -rf ${.OBJDIR}/syscalls
	mkdir ${.OBJDIR}/syscalls
	for syscall in $(AVAILABLE_SYSCALLS) ; do \
	  case " $(SYSCALL_EXCEPTIONS) " in \
	    *" "$$syscall" "*) ;; \
	    *)	echo $$syscall ; \
		$(CC) $(CFLAGS) -DSYSCALL_NAME=$$syscall -c ${SYSCALL_TEMPLATE} -o ${.OBJDIR}/syscalls/S$$syscall.o ; \
		$(SYSCALL_PIC_COMPILE) ; \
		$(SYSCALL_PROF_COMPILE) ;; \
	  esac ; \
	done
	x=`pwd` && cd ${.OBJDIR}/syscalls && ld -r -o ../syscalls.o *.o && cd $$x
.if !defined(NOPIC)
	x=`pwd` && cd ${.OBJDIR}/syscalls && ld -r -o ../syscalls.so *.so && cd $$x
.endif

.if !defined(NOPROFILE)
	x=`pwd` && cd ${.OBJDIR}/syscalls && ld -r -o ../syscalls.po *.po && cd $$x
.endif
	rm -r ${.OBJDIR}/syscalls

syscall.o: syscall.S
	cpp ${CPPFLAGS} ${SYSCALL_S} > syscall.i
	as syscall.i 
	rm syscall.i
	mv a.out syscall.o

syscall.po: syscall.S
	cpp ${CPPFLAGS} ${SYSCALL_S} > syscall.i
	as syscall.i 
	rm syscall.i
	mv a.out syscall.po

MAN2+=

MAN3+=

