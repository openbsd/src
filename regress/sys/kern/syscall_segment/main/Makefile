#	$OpenBSD: Makefile,v 1.2 2019/12/02 23:04:49 deraadt Exp $

PROG=syscall_main

run-regress-${PROG}: ${PROG}
	rm -f ./${PROG}.core
	if ./${PROG}; then false; else true; fi
	if [ ! -e ./${PROG}.core ]; then echo "No coredump"; false; fi

.include <bsd.regress.mk>
