#	$NetBSD: Makefile,v 1.8 1995/04/24 07:39:04 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

MAN=	lorder.1

.if (${MACHINE_ARCH} != "alpha")
SCRIPT=	lorder.sh
.else
# alpha uses special version which works with GNU nm.
SCRIPT=	lorder.sh.gnm
.endif

realinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/lorder

.include <bsd.prog.mk>
