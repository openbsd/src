# $OpenBSD: Makefile,v 1.13 2002/09/03 18:59:55 markus Exp $

SUBDIR=crypto ssl man

distribution:
	${INSTALL} ${INSTALL_COPY} -g ${BINGRP} -m 444 \
	   ${.CURDIR}/openssl.cnf ${DESTDIR}/etc/ssl/openssl.cnf && \
	${INSTALL} ${INSTALL_COPY} -g ${BINGRP} -m 444 \
	   ${.CURDIR}/x509v3.cnf ${DESTDIR}/etc/ssl/x509v3.cnf

.include <bsd.subdir.mk>
