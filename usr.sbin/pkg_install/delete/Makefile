#	$OpenBSD: Makefile,v 1.1 1996/06/04 07:56:07 niklas Exp $
PROG= pkg_delete
CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

.if exists(${.CURDIR}/../lib/obj)
LDADD+=		-L${.CURDIR}/../lib/obj -linstall
DPADD+=		${.CURDIR}/../lib/obj/libinstall.a
.else
LDADD+=		-L${.CURDIR}/../lib -linstall
DPADD+=		${.CURDIR}/../lib/libinstall.a
.endif

SRCS= main.c perform.c

.include <bsd.prog.mk>
