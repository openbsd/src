#	$OpenBSD: Makefile,v 1.1 2002/12/21 01:41:54 deraadt Exp $

PROG=	spamd
SRCS=	spamd.c
MAN=	spamd.8

CFLAGS+= -Wall -Werror 

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/spamd-setup.sh ${DESTDIR}${BINDIR}/spamd-setup

.include <bsd.prog.mk>
