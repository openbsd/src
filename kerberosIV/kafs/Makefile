#	$Id: Makefile,v 1.4 1998/08/12 23:49:01 art Exp $

LIB=	kafs
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../roken
SRCS=	afskrb.c afssys.c common.c
.PATH: ${.CURDIR}/../roken
SRCS+=	resolve.c

.include <bsd.own.mk> # for AFS

.if (${AFS} != "yes")
CFLAGS+=-DNO_AFS
.endif

.include <bsd.lib.mk>
