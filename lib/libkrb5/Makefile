# $OpenBSD: Makefile,v 1.6 2003/05/17 05:20:18 mho Exp $

KRB5DIR=	${.CURDIR}/../../kerberosV
.include "Makefile.common.inc"

LIB	= krb5

.PATH:	${KRB5SRC}/lib

.include "Makefile.asn1.inc"
.include "Makefile.hdb.inc"
.include "Makefile.kafs.inc"
.include "Makefile.roken.inc"
.include "Makefile.krb5.inc"

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
	@echo -n ${DIRKAFS}:; \
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
	@echo -n ${.OBJDIR}:; \
	cd ${.OBJDIR}; for i in ${HDR_OBJ}; do \
		echo -n " $$i"; \
		cmp -s $$i ${DESTDIR}/usr/include/kerberosV/$$i || \
			${INSTALL} ${INSTALL_COPY} -m 444 \
			-o ${BINOWN} -g $(BINGRP) $$i \
			${DESTDIR}/usr/include/kerberosV; done; \
		echo


SHLIB_LINKS	= kafs asn1
ALIB_LINKS	= $(SHLIB_LINKS) hdb

afterinstall:
	for f in $(SHLIB_LINKS); do \
		ln -f $(DESTDIR)/$(LIBDIR)/lib$(LIB).so.$(SHLIB_MAJOR).$(SHLIB_MINOR) \
		$(DESTDIR)$(LIBDIR)/lib$$f.so.$(SHLIB_MAJOR).$(SHLIB_MINOR) ;\
	done
	for f in $(ALIB_LINKS); do \
		ln -f $(DESTDIR)/$(LIBDIR)/lib$(LIB).a $(DESTDIR)$(LIBDIR)/lib$$f.a ; \
	done

.include <bsd.lib.mk>
