#	$OpenBSD: Makefile,v 1.13 2014/07/16 10:35:53 deraadt Exp $

.include <bsd.own.mk>

LIB=		milter
HDRS=		mfapi.h milter.h mfdef.h
SRCS=		main.c engine.c listener.c worker.c handler.c comm.c smfi.c \
		signal.c sm_gethost.c monitor.c errstring.c strl.c
CPPFLAGS+=	-D_FFR_MILTER_ROOT_UNSAFE -ULDAPMAP -pthread \
		-Dsm_snprintf=snprintf \
		-DSM_CONF_POLL
.PATH:		${.CURDIR}/../libsm

includes:
	@test -d ${DESTDIR}/usr/include/libmilter || \
	    mkdir -p ${DESTDIR}/usr/include/libmilter
	@cd ${.CURDIR}/../include/libmilter; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/libmilter/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include/libmilter"; \
	    echo $$j; \
	    eval "$$j"; \
	done

beforeinstall:
	cmp -s ${.CURDIR}/README ${DESTDIR}${SHAREDIR}/sendmail/README.milter || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/README ${DESTDIR}${SHAREDIR}/sendmail/README.milter

.include <bsd.lib.mk>
