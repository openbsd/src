#	$OpenBSD: Makefile,v 1.1 1996/11/11 04:55:02 downsj Exp $

LIB=	com_err
SRCS=	com_err.c error_message.c et_name.c init_et.c
CFLAGS+=-I${.CURDIR}
MAN=	com_err.3

includes:
	-cd ${.CURDIR}; cmp -s com_err.h ${DESTDIR}/usr/include/com_err.h > \
	    /dev/null 2>&1 || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 com_err.h \
	    ${DESTDIR}/usr/include/com_err.h

.include <bsd.lib.mk>
