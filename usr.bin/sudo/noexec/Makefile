#	$OpenBSD: Makefile,v 1.1 2004/09/28 15:10:51 millert Exp $

.PATH:		${.CURDIR}/..

LIB=	noexec
NOMAN=	yes
SRCS=	sudo_noexec.c
CPPFLAGS+=	-I.
LIBDIR=	/usr/libexec

DEBUGLIBS= no
NOPROFILE= yes

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    lib${LIB}.so.${SHLIB_MAJOR}.${SHLIB_MINOR} \
	    ${DESTDIR}${LIBDIR}/sudo_noexec.so

.include <bsd.lib.mk>
