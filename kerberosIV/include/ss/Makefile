#	$Id: Makefile,v 1.2 1996/06/02 07:21:31 tholo Exp $

FILES=	ss.h
NOOBJ=	noobj

all include clean cleandir depend lint tags:

includes:
	install -d -o ${BINOWN} -g ${BINGRP} -m 755 ${DESTDIR}/usr/include/ss
	@echo installing ${FILES}
	@-for i in ${FILES}; do \
		cmp -s $$i ${DESTDIR}/usr/include/ss/$$i || \
		    install -c -m 444 $$i ${DESTDIR}/usr/include/ss/$$i; \
	done

.include <bsd.prog.mk>
