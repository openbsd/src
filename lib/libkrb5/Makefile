# $OpenBSD: Makefile,v 1.5 2003/05/17 02:03:19 hin Exp $

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
	@for i in $(INCLUDE_FILES); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/kerberosV/$$i || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		$$i ${DESTDIR}/usr/include/kerberosV"; \
	    echo $$j; \
	pwd; \
	    eval "$$j"; \
	done

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
