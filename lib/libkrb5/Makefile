# $OpenBSD: Makefile,v 1.16 2005/11/24 20:49:18 deraadt Exp $

.include <bsd.own.mk>

KRB5DIR=	${.CURDIR}/../../kerberosV
GENDIR=		${.CURDIR}/generated
.include "Makefile.common.inc"

LIB	= krb5
WANTLINT=

.PATH:	${KRB5SRC}/lib
.PATH:	${GENDIR}

.include "Makefile.asn1.inc"
.include "Makefile.hdb.inc"
.include "Makefile.kafs.inc"
.include "Makefile.roken.inc"
.include "Makefile.krb5.inc"
.include "Makefile.com_err.inc"

includes:
	@echo "Removing old Kerberos include files (transitionary)"
	@/bin/rm -rf ${DESTDIR}/usr/include/kerberosV \
		${DESTDIR}/usr/include/kerberosIV \
		${DESTDIR}/usr/include/com_err.h
	@echo "Symlinking /usr/include/com_err.h (transitionary)"
	@/bin/ln -s /usr/include/kerberosV/com_err.h ${DESTDIR}/usr/include/
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
	@echo -n ${DIR_COM_ERR}:; \
	cd ${DIR_COM_ERR}; for i in ${HDR_COM_ERR}; do \
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


SHLIB_LINKS	= kafs asn1 com_err
ALIB_LINKS	= $(SHLIB_LINKS) hdb

afterinstall:
.if !defined(NOPIC)
	for f in $(SHLIB_LINKS); do \
		ln -f $(DESTDIR)$(LIBDIR)/lib$(LIB).so.$(SHLIB_MAJOR).$(SHLIB_MINOR) \
		    $(DESTDIR)$(LIBDIR)/lib$$f.so.$(SHLIB_MAJOR).$(SHLIB_MINOR) ;\
	done
	for f in $(SHLIB_LINKS); do \
		if [ -f $(DESTDIR)$(LIBDIR)/lib$(LIB)_pic.a ]; then \
			ln -f $(DESTDIR)$(LIBDIR)/lib$(LIB)_pic.a \
			    $(DESTDIR)$(LIBDIR)/lib$${f}_pic.a ;\
		fi \
	done
.endif
	for f in $(ALIB_LINKS); do \
		ln -f $(DESTDIR)$(LIBDIR)/lib$(LIB).a \
		    $(DESTDIR)$(LIBDIR)/lib$$f.a ; \
	done
.if !defined(NOPROFILE)
	for f in $(ALIB_LINKS); do \
		ln -f $(DESTDIR)$(LIBDIR)/lib$(LIB)_p.a \
		    $(DESTDIR)$(LIBDIR)/lib$${f}_p.a ; \
	done
.endif

.include <bsd.lib.mk>
