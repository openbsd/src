#	$OpenBSD: Makefile.inc,v 1.1 2004/01/15 21:32:03 millert Exp $

CPPFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Wall

.include <bsd.obj.mk>

.if exists(${.CURDIR}/../lib/${__objdir})
LDADD+=         -L${.CURDIR}/../lib/${__objdir} -lsudo
DPADD+=         ${.CURDIR}/../lib/${__objdir}/libsudo.a
.else
LDADD+=         -L${.CURDIR}/../lib -lsudo
DPADD+=         ${.CURDIR}/../lib/libsudo.a
.endif
