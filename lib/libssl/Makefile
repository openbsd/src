# $OpenBSD: Makefile,v 1.16 2010/12/28 20:07:47 jasper Exp $

.include <bsd.own.mk> # for KERBEROS5

SUBDIR=crypto ssl man
PC_FILES=openssl.pc libssl.pc libcrypto.pc

CLEANFILES=${PC_FILES}

distribution:
	${INSTALL} ${INSTALL_COPY} -g ${BINGRP} -m 444 \
	   ${.CURDIR}/openssl.cnf ${DESTDIR}/etc/ssl/openssl.cnf && \
	${INSTALL} ${INSTALL_COPY} -g ${BINGRP} -m 444 \
	   ${.CURDIR}/cert.pem ${DESTDIR}/etc/ssl/cert.pem && \
	${INSTALL} ${INSTALL_COPY} -g ${BINGRP} -m 444 \
	   ${.CURDIR}/x509v3.cnf ${DESTDIR}/etc/ssl/x509v3.cnf

beforeinstall:
.if (${KERBEROS5:L} == "yes")
	/bin/sh ${.CURDIR}/generate_pkgconfig.sh -c ${.CURDIR} -o ${.OBJDIR} -k
.else
	/bin/sh ${.CURDIR}/generate_pkgconfig.sh -c ${.CURDIR} -o ${.OBJDIR}
.endif
	[ -d ${DESTDIR}/usr/lib/pkgconfig/ ] || \
		${INSTALL} -o root -g ${SHAREGRP} -d ${DESTDIR}/usr/lib/pkgconfig/
.for p in ${PC_FILES}
	${INSTALL} ${INSTALL_COPY} -o root -g ${SHAREGRP} \
	    -m ${SHAREMODE} ${.OBJDIR}/$p ${DESTDIR}/usr/lib/pkgconfig/
.endfor

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
