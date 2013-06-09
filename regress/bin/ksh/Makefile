# $OpenBSD: Makefile,v 1.7 2013/06/09 13:27:27 millert Exp $

REGRESS_TARGETS=shcrash.sh seterror.sh varfunction.sh eval.sh eval2.sh eval3.sh eval4.sh

shcrash.sh:
	ulimit -c 0 && sh ${.CURDIR}/shcrash.sh

seterror.sh:
	sh ${.CURDIR}/seterror.sh

varfunction.sh:
	sh ${.CURDIR}/varfunction.sh

eval.sh:
	test `sh ${.CURDIR}/eval.sh | wc -l` == 4

eval2.sh:
	# this should not print
	test `sh ${.CURDIR}/eval2.sh|wc -c` == 0
	# and it should return an error
	if ! sh ${.CURDIR}/eval2.sh >/dev/null; then true; else false; fi

eval3.sh:
	# this should not print
	test `sh ${.CURDIR}/eval3.sh|wc -c` == 0
	# and it should return an error
	if ! sh ${.CURDIR}/eval3.sh >/dev/null; then true; else false; fi

eval4.sh:
	# this should not print
	test `sh ${.CURDIR}/eval4.sh|wc -c` == 0
	# and it should return an error
	if ! sh ${.CURDIR}/eval4.sh >/dev/null; then true; else false; fi

.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
