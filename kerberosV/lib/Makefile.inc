# $OpenBSD: Makefile.inc,v 1.3 2002/04/18 22:12:00 espie Exp $

.if defined(INCLUDE_FILES)
INCLUDE_DIR?=	${DESTDIR}/usr/include/kerberosV
includes:	$(INCLUDE_FILES)
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 755 ${INCLUDE_DIR}
	-for i in ${INCLUDE_FILES}; do \
		echo "installing `basename $$i` in ${INCLUDE_DIR}" ;\
		cmp -s $$i ${INCLUDE_DIR}/`basename $$i` || \
		${INSTALL} ${INSTALL_COPY} -m 444 $$i ${INCLUDE_DIR}/`basename $$i` ;\
	done
.endif

