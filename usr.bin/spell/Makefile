#	$OpenBSD: Makefile,v 1.2 2002/03/02 16:20:33 millert Exp $

PROG=	spellprog
MAN=	spell.1
SRCS=	spellprog.c look.c
BINDIR=	/usr/libexec

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/spell.ksh ${DESTDIR}/usr/bin/spell
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${NONBINMODE} \
		${.CURDIR}/stop ${DESTDIR}/usr/share/dict
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${NONBINMODE} \
		${.CURDIR}/american ${DESTDIR}/usr/share/dict
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${NONBINMODE} \
		${.CURDIR}/british ${DESTDIR}/usr/share/dict
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${NONBINMODE} \
		${.CURDIR}/special.4bsd  ${DESTDIR}/usr/share/dict/special/4bsd
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${NONBINMODE} \
		${.CURDIR}/special.math ${DESTDIR}/usr/share/dict/special/math

.include <bsd.prog.mk>
