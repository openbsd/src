#	$OpenBSD: Makefile,v 1.3 1996/08/26 07:00:30 deraadt Exp $
PROG= pkg_add

CFLAGS+=	${DEBUG} -I${.CURDIR}/../lib

.include <bsd.obj.mk>

.if exists(${.CURDIR}/../lib/${__objdir})
LDADD+=		-L${.CURDIR}/../lib/${__objdir} -linstall
DPADD+=		${.CURDIR}/../lib/${__objdir}/libinstall.a
.else
LDADD+=		-L${.CURDIR}/../lib -linstall
DPADD+=		${.CURDIR}/../lib/libinstall.a
.endif


SRCS= main.c perform.c futil.c extract.c

.include <bsd.prog.mk>
