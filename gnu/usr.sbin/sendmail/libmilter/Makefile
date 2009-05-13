#	$OpenBSD: Makefile,v 1.12 2009/05/13 18:24:15 jakob Exp $

.include <bsd.own.mk>

LIB=		milter
HDRS=		mfapi.h milter.h mfdef.h
DOCS=		api.html design.html figure1.jpg figure2.jpg index.html \
                installation.html other.html overview.html sample.html \
                smfi_addheader.html smfi_addrcpt.html smfi_chgheader.html \
                smfi_delrcpt.html smfi_getpriv.html smfi_getsymval.html \
                smfi_insheader.html smfi_main.html smfi_opensocket.html \
                smfi_progress.html smfi_quarantine.html smfi_register.html \
                smfi_replacebody.html smfi_setbacklog.html \
                smfi_setconn.html smfi_setdbg.html smfi_setmlreply.html \
                smfi_setpriv.html smfi_setreply.html smfi_settimeout.html \
                smfi_stop.html xxfi_abort.html xxfi_body.html \
                xxfi_close.html xxfi_connect.html xxfi_envfrom.html \
                xxfi_envrcpt.html xxfi_eoh.html xxfi_eom.html \
                xxfi_header.html xxfi_helo.html
MDOCDIR=	${DESTDIR}${DOCDIR}/html/milter
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
	@echo "installing ${DOCS} -> ${MDOCDIR}"
	@test -d ${MDOCDIR} || mkdir ${MDOCDIR}
	@-for i in ${DOCS}; do \
		cmp -s ${.CURDIR}/docs/$$i ${MDOCDIR}/$$i || \
		    ${INSTALL} ${INSTALL_COPY} -o ${DOCOWN} -g ${DOCGRP} \
		    -m ${DOCMODE} ${.CURDIR}/docs/$$i ${MDOCDIR} ; \
	done

.include <bsd.lib.mk>
