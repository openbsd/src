#	$Id: Makefile,v 1.1.1.1 1995/12/14 06:52:35 tholo Exp $

FILES=	ss.h
NOOBJ=	noobj

all include clean cleandir depend lint tags:

realinstall:
	@echo installing ${FILES}
	@-for i in ${FILES}; do \
		cmp -s $$i ${DESTDIR}/usr/include/ss/$$i || \
		    install -c -m 444 $$i ${DESTDIR}/usr/include/ss/$$i; \
	done

beforeinstall:
	install -d -o ${BINOWN} -g ${BINGRP} -m 755 ${DESTDIR}/usr/include/ss

.include <bsd.prog.mk>
