#	$OpenBSD: Makefile,v 1.2 1996/12/08 13:57:06 downsj Exp $
#	$From: Makefile,v 1.9 1996/11/17 03:51:26 wosch Exp $

SCRIPTS= adduser.perl rmuser.perl rmgroup.sh
MAN=	 adduser.8 adduser_proc.8 rmuser.8 rmgroup.8

beforeinstall:
.for script in ${SCRIPTS}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor

.include <bsd.prog.mk>
