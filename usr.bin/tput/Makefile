#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:41:46 deraadt Exp $
#	$NetBSD: Makefile,v 1.3 1994/12/07 08:49:07 jtc Exp $

PROG=	tput
DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap
MLINKS=	tput.1 clear.1

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/clear.sh ${DESTDIR}/usr/bin/clear

.include <bsd.prog.mk>
