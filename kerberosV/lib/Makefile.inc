# $OpenBSD: Makefile.inc,v 1.2 2001/05/25 22:45:56 hin Exp $

.if defined(INCLUDE_FILES)
INCLUDE_DIR?=	${DESTDIR}/usr/include/kerberosV
includes:	$(INCLUDE_FILES)
	mkdir -p ${INCLUDE_DIR}
	-for i in ${INCLUDE_FILES}; do \
		echo "installing `basename $$i` in ${INCLUDE_DIR}" ;\
		cmp -s $$i ${INCLUDE_DIR}/`basename $$i` || \
		${INSTALL} ${INSTALL_COPY} -m 444 $$i ${INCLUDE_DIR}/`basename $$i` ;\
	done
.endif

