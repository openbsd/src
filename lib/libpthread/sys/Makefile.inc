#	$Id: Makefile.inc,v 1.4 1999/11/25 07:01:29 d Exp $
#	$OpenBSD: Makefile.inc,v 1.4 1999/11/25 07:01:29 d Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.c slow_atomic_lock.c

SRCS+= _sys_aliases.c
CLEANFILES += _sys_aliases.c
_sys_aliases.c: ${.CURDIR}/Makefile ${LIBCSRCDIR}/sys/Makefile.inc
	echo '#include <sys/cdefs.h>' > ${.TARGET}
.if ${MACHINE} == "alpha"
	# This kludge will disappear when we shift to -lpthread
	echo '#define __indr_reference(old,new)' \
		'__asm__(".globl "#new";"#new": jmp $$31,"#old)' > ${.TARGET}

.endif
	for fn in ${ASM:R} ${PSEUDO:R} ""; do \
	    case $$fn in ${HIDDEN_SYSCALLS:.o=|}"") : stays hidden ;; \
	    		 *) echo "__indr_reference(_thread_sys_$$fn,$$fn);";; \
	     esac; \
	 done >> ${.TARGET}

