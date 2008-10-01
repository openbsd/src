#	$OpenBSD: Makefile,v 1.1 2008/10/01 23:04:36 millert Exp $

PROG=	globtest

run-regress-${PROG}:
	mkdir -p `sed 's@/[^/]*$$@@' ${.CURDIR}/files | sort -u`
	touch `cat files`
	./${PROG} ${.CURDIR}/${PROG}.in

clean:
	rm -rf fake ${PROG} ${OBJS} *.core

.include <bsd.regress.mk>
