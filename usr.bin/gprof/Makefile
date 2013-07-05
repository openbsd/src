#	$OpenBSD: Makefile,v 1.20 2013/07/05 21:29:51 miod Exp $

.include <bsd.own.mk>

TARGET_MACHINE_ARCH?=	${MACHINE_ARCH}
TARGET_MACHINE_CPU?=	${MACHINE_CPU}

PROG=	gprof
SRCS=	gprof.c arcs.c dfn.c lookup.c ${TARGET_MACHINE_CPU}.c hertz.c \
	printgprof.c printlist.c
CFLAGS+= -I. -DMD_INCLUDE=\"${TARGET_MACHINE_CPU}.h\"

SRCS+=elf.c

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/gprof.flat ${.CURDIR}/gprof.callg \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
