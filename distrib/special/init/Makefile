#	$OpenBSD: Makefile,v 1.2 2002/06/09 05:53:55 todd Exp $

PROG=	init
MAN=
COPTS+=-Os
CFLAGS+=-DDEFAULT_STATE=single_user
DPADD=	${LIBUTIL}
LDADD=	-lutil
.PATH:  ${.CURDIR}/../../../sbin/init

.include <bsd.prog.mk>
