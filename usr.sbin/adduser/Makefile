#	$OpenBSD: Makefile,v 1.7 1999/07/09 13:28:23 aaron Exp $

SCRIPTS= adduser.perl rmuser.perl rmgroup.sh
MAN=	 adduser.8 adduser_proc.8 rmgroup.8
MLINKS+= adduser.8 rmuser.8

beforeinstall:
.for script in ${SCRIPTS}
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor

.include <bsd.prog.mk>
