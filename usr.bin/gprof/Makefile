#	$OpenBSD: Makefile,v 1.11 2001/09/10 23:03:20 jason Exp $

TARGET_MACHINE_ARCH?=	${MACHINE_ARCH}

.if (${TARGET_MACHINE_ARCH} == "alpha") || \
    (${TARGET_MACHINE_ARCH} == "powerpc") || \
    (${TARGET_MACHINE_ARCH} == "sparc64") || \
    (${TARGET_MACHINE_ARCH} == "hppa")
NOPROG=
.else
PROG=	gprof
SRCS=	gprof.c arcs.c dfn.c lookup.c machine.c hertz.c \
	printgprof.c printlist.c
CFLAGS+= -I${.CURDIR} -I.
.endif

beforedepend:
	ln -sf ${.CURDIR}/${TARGET_MACHINE_ARCH}.h machine.h
	ln -sf ${.CURDIR}/${TARGET_MACHINE_ARCH}.c machine.c

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/gprof.flat ${.CURDIR}/gprof.callg \
	    ${DESTDIR}/usr/share/misc

.if make(install)
SUBDIR+= PSD.doc
.endif

.include <bsd.prog.mk>
