# $OpenBSD: Makefile,v 1.6 2014/04/18 09:47:36 ajacoutot Exp $

SCRIPTS=krb5-config
MPAGES=	krb5-config.1
MSRC=	${KRB5DIR}/tools

CLEANFILES= krb5-config

krb5-config: krb5-config.in
	sed -e "s,@PACKAGE\@,heimdal,g" \
	    -e "s,@VERSION\@,1.5.3,g" \
	    -e "s,@prefix\@,/usr,g" \
	    -e "s,@exec_prefix\@,/usr,g" \
	    -e "s,@libdir\@,${LIBDIR},g" \
	    -e "s,@includedir\@,${INCLUDEDIR},g" \
	    -e "s,-lheimntlm,,g" \
	    -e "s,@LIB_crypt\@,,g" \
	    -e "s,@LIB_dbopen\@,,g" \
	    -e "s,@LIB_hcrypto_appl\@,-lcrypto,g" \
	    -e "s,@LIB_pkinit\@,,g" \
	    -e "s,@LIB_dlopen\@,,g" \
	    -e "s,@LIB_door_create\@,,g" \
	    -e "s,@PTHREAD_LIBADD\@,,g" \
	    -e "s,@LIBS\@,,g" \
	    -e "s,@INCLUDE_hcrypto@,,g" \
	    ${.ALLSRC} > ${.TARGET}

beforeinstall: ${SCRIPTS}
.for script in ${SCRIPTS}
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.OBJDIR}/${script} ${DESTDIR}${BINDIR}/${script:R}
.endfor

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/tools
