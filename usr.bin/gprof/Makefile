#	$OpenBSD: Makefile,v 1.8 2000/10/05 15:28:15 mickey Exp $

.if (${MACHINE_ARCH} == "alpha") || (${MACHINE_ARCH} == "mips") || \
	(${MACHINE_ARCH} == "powerpc") || (${MACHINE_ARCH} == "hppa")
NOPROG=
.else
PROG=	gprof
SRCS=	gprof.c arcs.c dfn.c lookup.c ${MACHINE_ARCH}.c hertz.c \
	printgprof.c printlist.c
CFLAGS+= -D${MACHINE_ARCH}
.endif

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/gprof.flat ${.CURDIR}/gprof.callg \
	    ${DESTDIR}/usr/share/misc

.if make(install)
SUBDIR+= PSD.doc
.endif

.include <bsd.prog.mk>
