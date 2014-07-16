#	$OpenBSD: Makefile,v 1.5 2014/07/16 20:02:17 okan Exp $
#	$NetBSD: Makefile,v 1.5 1998/04/13 14:18:45 lukem Exp $

LIB=	ossaudio
MAN=	ossaudio.3

SRCS=	ossaudio.c

CPPFLAGS+= -I${.CURDIR}

includes:
	@cd ${.CURDIR}; cmp -s soundcard.h ${DESTDIR}/usr/include/soundcard.h || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) soundcard.h \
	  ${DESTDIR}/usr/include

.include <bsd.lib.mk>
