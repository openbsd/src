#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:43:19 deraadt Exp $

NOMAN=
NOPROG=
NOOBJ=

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		$(.CURDIR)/makewhatis.sh $(DESTDIR)$(BINDIR)/makewhatis

.include <bsd.prog.mk>
