#	$OpenBSD: Makefile,v 1.3 2003/03/02 19:21:59 beck Exp $

PROG=	spamd
SRCS=	spamd.c sdl.c
MAN=	spamd.8 spamd-setup.8

CFLAGS+= -Wall -Werror

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/spamd-setup.pl ${DESTDIR}${BINDIR}/spamd-setup

.include <bsd.prog.mk>
