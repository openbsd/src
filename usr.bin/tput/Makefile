#	$OpenBSD: Makefile,v 1.3 1996/12/08 14:32:35 downsj Exp $
#	$NetBSD: Makefile,v 1.3 1994/12/07 08:49:07 jtc Exp $

PROG=	tput
DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap
MLINKS=	tput.1 clear.1

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/clear.sh ${DESTDIR}/usr/bin/clear

.include <bsd.prog.mk>
