#	$OpenBSD: Makefile,v 1.2 1996/06/19 01:15:12 maja Exp $
PROG= pkg_create

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

.if exists(${.CURDIR}/../lib/${__objdir})
LDADD+=		-L${.CURDIR}/../lib/${__objdir} -linstall
DPADD+=		${.CURDIR}/../lib/${__objdir}/libinstall.a
.else
LDADD+=		-L${.CURDIR}/../lib -linstall
DPADD+=		${.CURDIR}/../lib/libinstall.a
.endif


SRCS= main.c perform.c pl.c

.include <bsd.prog.mk>
