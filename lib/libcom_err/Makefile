#	$OpenBSD: Makefile,v 1.2 1996/12/08 15:22:25 downsj Exp $

LIB=	com_err
SRCS=	com_err.c error_message.c et_name.c init_et.c
CFLAGS+=-I${.CURDIR}
MAN=	com_err.3

includes:
	-cd ${.CURDIR}; cmp -s com_err.h ${DESTDIR}/usr/include/com_err.h > \
	    /dev/null 2>&1 || \
	    ${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 com_err.h \
	    ${DESTDIR}/usr/include/com_err.h

.include <bsd.lib.mk>
