#	$OpenBSD: Makefile,v 1.1 2015/10/09 06:44:13 semarie Exp $
PROG=	sigkill
NOMAN=	yes

run-regress-${PROG}: ${PROG}
	rm -f ./${PROG}.core
	if ./${PROG}; then false; else true; fi
	if [ -e ./${PROG}.core ]; then echo "Unexpected coredump"; false; fi
	./${PROG} | diff -I OpenBSD -u ${.CURDIR}/${PROG}.out -

.include <bsd.regress.mk>
