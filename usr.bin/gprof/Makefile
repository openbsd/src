#	$OpenBSD: Makefile,v 1.3 1996/10/04 19:16:54 deraadt Exp $
#	$NetBSD: Makefile,v 1.14 1995/04/19 07:24:08 cgd Exp $

.if (${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "mips")
NOPROG=
.else
PROG=	gprof
SRCS=	gprof.c arcs.c dfn.c lookup.c ${MACHINE_ARCH}.c hertz.c \
	printgprof.c printlist.c
CFLAGS+= -D${MACHINE_ARCH}
.endif

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/gprof.flat ${.CURDIR}/gprof.callg \
	    ${DESTDIR}/usr/share/misc

.if make(install)
SUBDIR+= PSD.doc
.endif

.include <bsd.prog.mk>
