#	$OpenBSD: Makefile,v 1.13 2002/05/08 16:46:35 art Exp $

.include <bsd.own.mk>

TARGET_MACHINE_ARCH?=	${MACHINE_ARCH}

PROG=	gprof
SRCS=	gprof.c arcs.c dfn.c lookup.c machine.c hertz.c \
	printgprof.c printlist.c
CFLAGS+= -I${.CURDIR} -I.

# XXX - need support for multiple archs.
.if (${ELF_TOOLCHAIN} == "yes")
SRCS+=elf.c
.else
SRCS+=aout.c
.endif

.if defined(PROG)
beforedepend:
	ln -sf ${.CURDIR}/${TARGET_MACHINE_ARCH}.h machine.h
	ln -sf ${.CURDIR}/${TARGET_MACHINE_ARCH}.c machine.c
.endif # defined (PROG)

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/gprof.flat ${.CURDIR}/gprof.callg \
	    ${DESTDIR}/usr/share/misc

.if make(install)
SUBDIR+= PSD.doc
.endif

.include <bsd.prog.mk>
