#	$OpenBSD: Makefile,v 1.8 1999/10/08 01:58:02 aaron Exp $

SCRIPTS= adduser.perl rmuser.perl rmgroup.sh
MAN=	 adduser.8 rmgroup.8
MLINKS+= adduser.8 rmuser.8

beforeinstall:
.for script in ${SCRIPTS}
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor

.include <bsd.prog.mk>
