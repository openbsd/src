#	$Id: Makefile.inc,v 1.1 1998/08/27 09:00:48 d Exp $
#	$OpenBSD: Makefile.inc,v 1.1 1998/08/27 09:00:48 d Exp $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S _sys_aliases.S

_sys_aliases.S: ${.CURDIR}/Makefile ${LIBCSRCDIR}/sys/Makefile.inc
	(echo '#include "SYS.h"'; \
	 for fn in ${ASM:R} ${PSEUDO:R} ""; do \
	    case $$fn in ${HIDDEN_SYSCALLS:.o=|}"") : stays hidden ;; \
	    		 *) echo "PASSTHRU($$fn)";; \
	     esac; \
	 done ) > ${.TARGET}

CLEANFILES += _sys_aliases.S
