#	$Id: Makefile.inc,v 1.2 1998/11/20 11:15:38 d Exp $
#	$OpenBSD: Makefile.inc,v 1.2 1998/11/20 11:15:38 d Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.c _sys_aliases.S slow_atomic_lock.c

_sys_aliases.S: ${.CURDIR}/Makefile ${LIBCSRCDIR}/sys/Makefile.inc
	(echo '#include "SYS.h"'; \
	 for fn in ${ASM:R} ${PSEUDO:R} ""; do \
	    case $$fn in ${HIDDEN_SYSCALLS:.o=|}"") : stays hidden ;; \
	    		 *) echo "PASSTHRU($$fn)";; \
	     esac; \
	 done ) > ${.TARGET}

CLEANFILES += _sys_aliases.S
