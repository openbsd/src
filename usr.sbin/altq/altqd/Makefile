#	$OpenBSD: Makefile,v 1.2 2001/08/03 17:57:14 jakob Exp $
#	$NetBSD: Makefile,v 1.3 2001/01/11 13:15:06 enami Exp $

.include <bsd.own.mk>

PROG=	altqd
MAN=	altqd.8 altq.conf.5

SRCS=	altqd.c libaltq2.c

CPPFLAGS+= -DALTQ -I${.CURDIR}/../libaltq

.include <bsd.obj.mk>

.if exists(${.CURDIR}/../libaltq/${__objdir})
LDADD+=         -L${.CURDIR}/../libaltq/${__objdir} -laltq
DPADD+=         ${.CURDIR}/../libaltq/${__objdir}/libaltq.a
.else
LDADD+=         -L${.CURDIR}/../libaltq -laltq
DPADD+=         ${.CURDIR}/../libaltq/libaltq.a
.endif

LDADD+=	-lm
DPADD+=	${LIBM}

.include <bsd.prog.mk>
