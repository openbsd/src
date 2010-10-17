#	$OpenBSD: Makefile,v 1.19 2010/10/17 22:54:37 schwarze Exp $

.include <bsd.own.mk>

TARGET_MACHINE_ARCH?=	${MACHINE_ARCH}
TARGET_MACHINE_CPU?=	${MACHINE_CPU}

PROG=	gprof
SRCS=	gprof.c arcs.c dfn.c lookup.c ${TARGET_MACHINE_CPU}.c hertz.c \
	printgprof.c printlist.c
CFLAGS+= -I. -DMD_INCLUDE=\"${TARGET_MACHINE_CPU}.h\"

# XXX - need support for multiple archs.
.if (${ELF_TOOLCHAIN:L} == "yes")
SRCS+=elf.c
.else
SRCS+=aout.c
.endif

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/gprof.flat ${.CURDIR}/gprof.callg \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
