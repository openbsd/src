#	$Id: Makefile,v 1.2 1996/02/10 21:04:43 tholo Exp $

FILES=	acl.h com_err.h des.h kadm.h kafs.h kparse.h krb.h krb_db.h \
	site.h
NOOBJ=	noobj

all include clean cleandir depend lint tags:

realinstall:
	@echo installing ${FILES}
	@-for i in ${FILES}; do \
		cmp -s $$i ${DESTDIR}/usr/include/kerberosIV/$$i || \
		    install -c -m 444 $$i ${DESTDIR}/usr/include/kerberosIV/$$i; \
	done

beforeinstall:
	install -d -o ${BINOWN} -g ${BINGRP} -m 755 ${DESTDIR}/usr/include/kerberosIV

.include <bsd.prog.mk>
