# $OpenBSD: Makefile.inc,v 1.3 1996/10/04 23:31:23 niklas Exp $
# $NetBSD: Makefile.inc,v 1.1.2.3 1996/06/25 01:13:36 jtc Exp $

BINDIR=	/usr/share/keymaps/amiga
NOMAN=
CC=	${HOSTCC}

all:	${MAP}

${MAP}: ${PROG}
	./${PROG} >${MAP}

realinstall:
.if !exists(${DESTDIR}${BINDIR})
	mkdir -p ${DESTDIR}${BINDIR}
.endif
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m 555 ${MAP} \
	    ${DESTDIR}${BINDIR}
