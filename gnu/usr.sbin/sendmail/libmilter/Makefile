#	$OpenBSD: Makefile,v 1.6 2004/06/29 04:07:49 millert Exp $

LIB=		milter
SRCS=		main.c engine.c listener.c handler.c comm.c smfi.c signal.c \
		sm_gethost.c errstring.c strl.c
CPPFLAGS+=	-D_FFR_MILTER_ROOT_UNSAFE -pthread -Dsm_snprintf=snprintf
.PATH:		${.CURDIR}/../libsm

# Only build the .a version for now.
NOPIC=1

includes:
	cmp -s ${.CURDIR}/libmilter.h ${DESTDIR}/usr/include/libmilter.h || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/libmilter.h ${DESTDIR}/usr/include

.include <bsd.lib.mk>
