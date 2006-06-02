#	$OpenBSD: Makefile,v 1.10 2006/06/02 19:19:10 beck Exp $

PROG=	chio
SRCS=	chio.c parse.y
DPADD=	${LIBUTIL}
LDADD=	-lutil

distribution:
	${INSTALL} ${INSTALL_COPY} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/chio.conf ${DESTDIR}/etc/chio.conf

.include <bsd.prog.mk>
