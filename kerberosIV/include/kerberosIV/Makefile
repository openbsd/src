#	$Id: Makefile,v 1.3 1996/06/02 07:21:31 tholo Exp $

FILES=	acl.h com_err.h des.h kadm.h kafs.h kparse.h krb.h krb_db.h \
	site.h
NOOBJ=	noobj

all include clean cleandir depend lint tags:

includes:
	install -d -o ${BINOWN} -g ${BINGRP} -m 755 ${DESTDIR}/usr/include/kerberosIV
	@echo installing ${FILES}
	@-for i in ${FILES}; do \
		cmp -s $$i ${DESTDIR}/usr/include/kerberosIV/$$i || \
		    install -c -m 444 $$i ${DESTDIR}/usr/include/kerberosIV/$$i; \
	done

.include <bsd.prog.mk>
