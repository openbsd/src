#	$OpenBSD: Makefile,v 1.1 1996/06/04 07:56:05 niklas Exp $
PROG= pkg_create

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

.if exists(${.CURDIR}/../lib/obj)
LDADD+=		-L${.CURDIR}/../lib/obj -linstall
DPADD+=		${.CURDIR}/../lib/obj/libinstall.a
.else
LDADD+=		-L${.CURDIR}/../lib -linstall
DPADD+=		${.CURDIR}/../lib/libinstall.a
.endif


SRCS= main.c perform.c pl.c

.include <bsd.prog.mk>
