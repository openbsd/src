#	$OpenBSD: Makefile,v 1.3 2005/11/24 20:49:19 deraadt Exp $
#	$NetBSD: Makefile,v 1.5 1998/04/13 14:18:45 lukem Exp $

LIB=	ossaudio
WANTLINT=
MAN=	ossaudio.3

SRCS=	ossaudio.c

CPPFLAGS+= -I${.CURDIR}

includes:
	@cd ${.CURDIR}; cmp -s soundcard.h ${DESTDIR}/usr/include/soundcard.h || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) soundcard.h \
	  ${DESTDIR}/usr/include

.include <bsd.lib.mk>

