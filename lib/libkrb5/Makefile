# $OpenBSD: Makefile,v 1.9 2003/06/03 09:18:00 hin Exp $

KRB5DIR=	${.CURDIR}/../../kerberosV
GENDIR=		${.CURDIR}/generated
.include "Makefile.common.inc"

LIB	= krb5

.PATH:	${KRB5SRC}/lib
.PATH:	${GENDIR}

.include "Makefile.asn1.inc"
.include "Makefile.hdb.inc"
.include "Makefile.kafs.inc"
.include "Makefile.roken.inc"
.include "Makefile.krb5.inc"
.include "Makefile.com_err.inc"

includes:
	@test -d ${DESTDIR}/usr/include/kerberosV || \
		mkdir ${DESTDIR}/usr/include/kerberosV
	@echo -n ${DIR_KRB5INC}:; \
	cd ${DIR_KRB5INC}; for i in ${HDR_KRB5INC}; do \
		echo -n " $$i"; \
		cmp -s $$i ${DESTDIR}/usr/include/kerberosV/$$i || \
			${INSTALL} ${INSTALL_COPY} -m 444 \
			-o ${BINOWN} -g $(BINGRP) $$i \
			${DESTDIR}/usr/include/kerberosV; done; \
		echo
	@echo -n ${DIR_KRB5LIB}:; \
	cd ${DIR_KRB5LIB}; for i in ${HDR_KRB5LIB}; do \
		echo -n " $$i"; \
		cmp -s $$i ${DESTDIR}/usr/include/kerberosV/$$i || \
			${INSTALL} ${INSTALL_COPY} -m 444 \
			-o ${BINOWN} -g $(BINGRP) $$i \
			${DESTDIR}/usr/include/kerberosV; done; \
		echo
	@echo -n ${DIR_KAFS}:; \
	cd ${DIR_KAFS}; for i in ${HDR_KAFS}; do \
		echo -n " $$i"; \
		cmp -s $$i ${DESTDIR}/usr/include/kerberosV/$$i || \
			${INSTALL} ${INSTALL_COPY} -m 444 \
			-o ${BINOWN} -g $(BINGRP) $$i \
			${DESTDIR}/usr/include/kerberosV; done; \
		echo
	@echo -n ${DIR_HDB}:; \
	cd ${DIR_HDB}; for i in ${HDR_HDB}; do \
		echo -n " $$i"; \
		cmp -s $$i ${DESTDIR}/usr/include/kerberosV/$$i || \
			${INSTALL} ${INSTALL_COPY} -m 444 \
			-o ${BINOWN} -g $(BINGRP) $$i \
			${DESTDIR}/usr/include/kerberosV; done; \
		echo
	@echo -n ${DIR_ASN1}:; \
	cd ${DIR_ASN1}; for i in ${HDR_ASN1}; do \
		echo -n " $$i"; \
		cmp -s $$i ${DESTDIR}/usr/include/kerberosV/$$i || \
			${INSTALL} ${INSTALL_COPY} -m 444 \
			-o ${BINOWN} -g $(BINGRP) $$i \
			${DESTDIR}/usr/include/kerberosV; done; \
		echo
	@echo -n ${GENDIR}:; \
	cd ${GENDIR}; for i in ${HDR_GEN}; do \
		echo -n " $$i"; \
		cmp -s $$i ${DESTDIR}/usr/include/kerberosV/$$i || \
			${INSTALL} ${INSTALL_COPY} -m 444 \
			-o ${BINOWN} -g $(BINGRP) $$i \
			${DESTDIR}/usr/include/kerberosV; done; \
		echo


SHLIB_LINKS	= kafs asn1
ALIB_LINKS	= $(SHLIB_LINKS) hdb com_err

afterinstall:
	for f in $(SHLIB_LINKS); do \
		ln -f $(DESTDIR)/$(LIBDIR)/lib$(LIB).so.$(SHLIB_MAJOR).$(SHLIB_MINOR) \
		$(DESTDIR)$(LIBDIR)/lib$$f.so.$(SHLIB_MAJOR).$(SHLIB_MINOR) ;\
	done
	for f in $(ALIB_LINKS); do \
		ln -f $(DESTDIR)/$(LIBDIR)/lib$(LIB).a $(DESTDIR)$(LIBDIR)/lib$$f.a ; \
	done

.include <bsd.lib.mk>
