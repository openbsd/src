#	$Id: Makefile,v 1.5 1997/06/29 10:32:13 provos Exp $

FILES=	acl.h com_err.h kadm.h kafs.h kparse.h krb.h krb_db.h site.h
NOOBJ=	noobj

all include clean cleandir depend lint tags:

includes:
	install -d -o ${BINOWN} -g ${BINGRP} -m 755 ${DESTDIR}/usr/include/kerberosIV
	@echo installing ${FILES}
	@-for i in ${FILES}; do \
		cmp -s $$i ${DESTDIR}/usr/include/kerberosIV/$$i || \
		    ${INSTALL} ${INSTALL_COPY} -m 444 $$i ${DESTDIR}/usr/include/kerberosIV/$$i; \
	done

.include <bsd.prog.mk>
