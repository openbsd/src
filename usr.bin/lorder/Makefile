#	$OpenBSD: Makefile,v 1.7 1997/09/21 11:49:46 deraadt Exp $

MAN=	lorder.1

.if (${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "mips" \
	|| ${MACHINE_ARCH} == "powerpc" )
# alpha, mips use special version which works with GNU (or BSD) nm.
SCRIPT=	lorder.sh.gnm
.else
SCRIPT=	lorder.sh
.endif

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/lorder

.include <bsd.prog.mk>
