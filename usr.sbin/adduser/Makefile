#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:43:10 deraadt Exp $

SCRIPTS= adduser.perl rmuser.perl rmgroup.sh
MAN=	 adduser.8 adduser_proc.8 rmuser.8 rmgroup.8

beforeinstall:
.for script in ${SCRIPTS}
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor

.include <bsd.prog.mk>
