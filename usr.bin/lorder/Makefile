#	$OpenBSD: Makefile,v 1.4 1996/12/08 14:32:28 downsj Exp $
#	$NetBSD: Makefile,v 1.9 1995/12/18 07:59:12 jonathan Exp $

MAN=	lorder.1

.if (${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "mips")
# alpha, mips use special version which works with GNU (or BSD) nm.
SCRIPT=	lorder.sh.gnm
.else
SCRIPT=	lorder.sh
.endif

realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/lorder

.include <bsd.prog.mk>
