# $OpenBSD: Makefile.inc,v 1.1 1996/08/23 23:38:04 niklas Exp $
# $NetBSD: Makefile.inc,v 1.1.2.3 1996/06/25 01:13:36 jtc Exp $

BINDIR=$(DESTDIR)/usr/share/keymaps/amiga
NOMAN=

realinstall: ${PROG}
.if !.exists(${BINDIR}(
	mkdir -p ${BINDIR}
.endif
	./${PROG} > ${BINDIR}/${MAP}
	chmod 555 ${BINDIR}/${MAP}
	chown ${BINOWN}.${BINGRP} ${BINDIR}/${MAP}
