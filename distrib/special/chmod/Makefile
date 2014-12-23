#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:01 deraadt Exp $

PROG=	chmod
CFLAGS+=-DSUPPORT_DOT
MAN=	chmod.1 chgrp.1 chown.8 chflags.1
LINKS=	${BINDIR}/chmod ${BINDIR}/chgrp \
	${BINDIR}/chmod /sbin/chown

# XXX compatibility
afterinstall:
	(cd ${DESTDIR}/usr/sbin; \
	    ln -sf ../../sbin/chown .; \
	    ln -sf ../../bin/chgrp .)
	(cd ${DESTDIR}/usr/bin; \
	    ln -sf ../../bin/chmod chflags)


.PATH:  ${.CURDIR}/../../../bin/chmod
.include <bsd.prog.mk>
