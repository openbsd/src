#	$OpenBSD: Makefile,v 1.2 2009/02/18 15:17:55 millert Exp $

PROG=	globtest

run-regress-${PROG}:
	mkdir -p `sed 's@/[^/]*$$@@' ${.CURDIR}/files | sort -u`
	touch `cat ${.CURDIR}/files`
	./${PROG} ${.CURDIR}/${PROG}.in

clean:
	rm -rf fake ${PROG} ${OBJS} *.core

.include <bsd.regress.mk>
