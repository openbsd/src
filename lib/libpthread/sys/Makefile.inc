#	$Id: Makefile.inc,v 1.3 1999/01/06 05:36:18 d Exp $
#	$OpenBSD: Makefile.inc,v 1.3 1999/01/06 05:36:18 d Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.c slow_atomic_lock.c 

SRCS+= _sys_aliases.c
CLEANFILES += _sys_aliases.c
_sys_aliases.c: ${.CURDIR}/Makefile ${LIBCSRCDIR}/sys/Makefile.inc
	(echo '#include <sys/cdefs.h>'; \
	 for fn in ${ASM:R} ${PSEUDO:R} ""; do \
	    case $$fn in ${HIDDEN_SYSCALLS:.o=|}"") : stays hidden ;; \
	    		 *) echo "__indr_reference(_thread_sys_$$fn,$$fn);";; \
	     esac; \
	 done ) > ${.TARGET}

