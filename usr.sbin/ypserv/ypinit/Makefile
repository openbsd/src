#	$OpenBSD: Makefile,v 1.12 1997/04/24 18:30:30 deraadt Exp $

MAN=	ypinit.8

DISTFILES= Makefile.main Makefile.yp
SHFILES= ypinit.sh

install:
.for script in ${SHFILES}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor
.for script in ${DISTFILES}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NONBINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}/var/yp/${script}.dist
.endfor
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/README ${DESTDIR}/var/yp/README

distribution:
.for script in ${DISTFILES}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NONBINMODE} \
	    ${.CURDIR}/${script} ${DESTDIR}/var/yp/${script}
.endfor

.include <bsd.prog.mk>
