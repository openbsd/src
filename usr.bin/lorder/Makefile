#	$OpenBSD: Makefile,v 1.5 1997/01/02 20:48:29 rahnds Exp $
#	$NetBSD: Makefile,v 1.9 1995/12/18 07:59:12 jonathan Exp $

MAN=	lorder.1

.if (${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "mips" \
	|| ${MACHINE_ARCH} == "powerpc" )
# alpha, mips use special version which works with GNU (or BSD) nm.
SCRIPT=	lorder.sh.gnm
.else
SCRIPT=	lorder.sh
.endif

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/lorder

.include <bsd.prog.mk>
