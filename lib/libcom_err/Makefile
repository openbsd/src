#	$OpenBSD: Makefile,v 1.4 2000/02/19 12:10:27 hin Exp $

LIB=	com_err
SRCS=	com_err.c error.c
CFLAGS+=-I${.CURDIR}
MAN=	com_err.3

includes:
	-cd ${.CURDIR}; cmp -s com_err.h ${DESTDIR}/usr/include/com_err.h > \
	    /dev/null 2>&1 || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 com_err.h \
	    ${DESTDIR}/usr/include/com_err.h

.include <bsd.lib.mk>
