#	$OpenBSD: Makefile,v 1.1 2000/08/06 00:16:01 millert Exp $

PROG=	init
MAN=
COPTS+=-Os
CFLAGS+=-DDEFAULT_STATE=single_user
DPADD=	${LIBUTIL} 
LDADD=	-lutil 
.PATH:  ${.CURDIR}/../../../sbin/init

.include <bsd.prog.mk>
