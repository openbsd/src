# $OpenBSD: Makefile.inc,v 1.3 2002/04/18 22:12:00 espie Exp $

.if defined(INCLUDE_FILES)
includes:
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 \
	    ${DESTDIR}/usr/include/kerberosIV
	@-for i in ${INCLUDE_FILES}; do \
		echo "installing `basename $$i`..." ;\
		cmp -s $$i ${DESTDIR}/usr/include/kerberosIV/`basename $$i` || \
		${INSTALL} ${INSTALL_COPY} -m 444 $$i ${DESTDIR}/usr/include/kerberosIV/`basename $$i` ;\
	done
.endif

