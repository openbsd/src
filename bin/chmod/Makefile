#	$OpenBSD: Makefile,v 1.6 2000/07/19 19:42:31 mickey Exp $

PROG=	chmod
CFLAGS+=-DSUPPORT_DOT
MAN=	chmod.1 chgrp.1 chown.8 chflags.1
LINKS=	${BINDIR}/chmod ${BINDIR}/chgrp \
	${BINDIR}/chmod /sbin/chown

# XXX compatibility
afterinstall:
	ln -fs /sbin/chown ${DESTDIR}/usr/sbin/chown
	ln -fs ${BINDIR}/chgrp ${DESTDIR}/usr/sbin/chgrp
	ln -fs ${BINDIR}/chmod ${DESTDIR}/usr/bin/chflags

.include <bsd.prog.mk>
