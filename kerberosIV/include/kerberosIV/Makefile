#	$Id: Makefile,v 1.6 1997/12/12 05:30:09 art Exp $

FILES=	acl.h com_err.h kadm.h kafs.h kparse.h krb.h krb_db.h site.h krb-protos.h
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
