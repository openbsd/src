#	$Id: Makefile,v 1.3 1998/03/12 17:46:26 art Exp $

LIB=	kafs
CFLAGS+=-I${.CURDIR}
SRCS=	afskrb.c afssys.c common.c

.include <bsd.own.mk> # for AFS

.if (${AFS} != "yes")
CFLAGS+=-DNO_AFS
.endif

.include <bsd.lib.mk>
