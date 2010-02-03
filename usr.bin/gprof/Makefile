#	$OpenBSD: Makefile,v 1.18 2010/02/03 20:49:01 miod Exp $

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

.if make(install)
SUBDIR+= PSD.doc
.endif

.include <bsd.prog.mk>
