#	$OpenBSD: Makefile.inc,v 1.14 2001/06/23 15:12:17 itojun Exp $

CFLAGS+=	-I${.CURDIR}/..

CFLAGS+=	-Wall
CFLAGS+=		-Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wno-uninitialized

.include <bsd.obj.mk>

.if exists(${.CURDIR}/../lib/${__objdir})
LDADD+=         -L${.CURDIR}/../lib/${__objdir} -lssh
DPADD+=         ${.CURDIR}/../lib/${__objdir}/libssh.a
.else
LDADD+=         -L${.CURDIR}/../lib -lssh
DPADD+=         ${.CURDIR}/../lib/libssh.a
.endif
