#	$OpenBSD: Makefile,v 1.4 2003/03/02 23:55:11 cloder Exp $

PROG=	spamd
SRCS=	spamd.c sdl.c
MAN=	spamd.8 spamd-setup.8

CFLAGS+= -Wall -Werror -Wstrict-prototypes -ansi

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/spamd-setup.pl ${DESTDIR}${BINDIR}/spamd-setup

.include <bsd.prog.mk>
