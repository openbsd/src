#	$OpenBSD: Makefile,v 1.7 2004/06/29 16:29:32 millert Exp $

LIB=		milter
HDRS=		mfapi.h milter.h mfdef.h
SRCS=		main.c engine.c listener.c handler.c comm.c smfi.c signal.c \
		sm_gethost.c errstring.c strl.c
CPPFLAGS+=	-D_FFR_MILTER_ROOT_UNSAFE -pthread -Dsm_snprintf=snprintf
.PATH:		${.CURDIR}/../libsm

# Only build the .a version for now.
NOPIC=1

includes:
	@test -d ${DESTDIR}/usr/include/libmilter || mkdir -p ${DESTDIR}/usr/include/libmilter
	@cd ${.CURDIR}/../include/libmilter; for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/libmilter/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i \
		${DESTDIR}/usr/include/libmilter"; \
	    echo $$j; \
	    eval "$$j"; \
	done

.include <bsd.lib.mk>
