#	$OpenBSD: Makefile,v 1.21 2013/10/17 10:51:57 deraadt Exp $

.include <bsd.own.mk>

TARGET_MACHINE_ARCH?=	${MACHINE_ARCH}
TARGET_MACHINE_CPU?=	${MACHINE_CPU}

PROG=	gprof
SRCS=	gprof.c arcs.c dfn.c elf.c lookup.c ${TARGET_MACHINE_CPU}.c \
	hertz.c printgprof.c printlist.c
CFLAGS+= -I. -DMD_INCLUDE=\"${TARGET_MACHINE_CPU}.h\"

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/gprof.flat ${.CURDIR}/gprof.callg \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
