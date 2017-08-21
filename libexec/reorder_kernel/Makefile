#	$OpenBSD: Makefile,v 1.1 2017/08/21 21:24:11 rpe Exp $

SCRIPT=	reorder_kernel.sh

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/reorder_kernel

.include <bsd.prog.mk>
