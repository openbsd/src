# $OpenBSD: Makefile.inc,v 1.2 2001/05/25 22:47:15 hin Exp $

.if defined(INCLUDE_FILES)
includes:
	@-for i in ${INCLUDE_FILES}; do \
		echo "installing `basename $$i`..." ;\
		cmp -s $$i ${DESTDIR}/usr/include/kerberosIV/`basename $$i` || \
		${INSTALL} ${INSTALL_COPY} -m 444 $$i ${DESTDIR}/usr/include/kerberosIV/`basename $$i` ;\
	done
.endif

