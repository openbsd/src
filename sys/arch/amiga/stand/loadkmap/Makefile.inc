# $OpenBSD: Makefile.inc,v 1.4 1997/01/16 09:27:06 niklas Exp $
# $NetBSD: Makefile.inc,v 1.2 1996/06/21 21:55:53 is Exp $

BINDIR=	${DESTDIR}/usr/share/keymaps/amiga
NOMAN=
CC=	${HOSTCC}

# XXX making dirs should be handled by mtree
realinstall: ${PROG}
.if !exists(${BINDIR})
	mkdir -p ${BINDIR}
.endif
	./${PROG} > ${BINDIR}/${MAP}
	chmod 555 ${BINDIR}/${MAP}
	chown ${BINOWN}.${BINGRP} ${BINDIR}/${MAP}
