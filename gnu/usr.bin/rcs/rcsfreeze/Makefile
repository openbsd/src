#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:41:04 deraadt Exp $

MAN=	rcsfreeze.1

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/rcsfreeze.sh ${DESTDIR}${BINDIR}/rcsfreeze

.include "../../Makefile.inc"
.include <bsd.prog.mk>
