# $OpenBSD: Makefile,v 1.3 2020/10/16 21:34:59 bluhm Exp $

PROG=	syscall_main

run-regress-${PROG}: ${PROG}
	rm -f ${PROG}.core
	! ./${PROG}
	if [ ! -e ${PROG}.core ]; then echo "No coredump"; false; fi

.include <bsd.regress.mk>
