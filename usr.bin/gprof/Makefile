#	$OpenBSD: Makefile,v 1.7 1998/07/06 15:19:31 millert Exp $

.if (${MACHINE_ARCH} == "alpha") || (${MACHINE_ARCH} == "mips") || \
	(${MACHINE_ARCH} == "powerpc")
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
