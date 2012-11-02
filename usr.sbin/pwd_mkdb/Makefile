#	$OpenBSD: Makefile,v 1.9 2012/11/02 21:53:16 miod Exp $

# static because it's used by the boot floppy through a chroot.

LDSTATIC=	${STATIC}
PROG=	pwd_mkdb
MAN=	pwd_mkdb.8
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
