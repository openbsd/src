#	$OpenBSD: Makefile,v 1.11 2004/01/20 21:06:02 matthieu Exp $

PROG=	ypserv
MAN=	ypserv.acl.5 securenet.5 ypserv.8

XSRCS=	ypv1.x
TSRCS=	${XSRCS:R:S/$/.c/g}
HDRS=	${XSRCS:R:S/$/.h/g}
SRCS+=	${TSRCS}

SRCS+=	ypserv.c ypserv_proc.c ypserv_db.c acl.c yplog.c ypdb.c

CLEANFILES+=	${TSRCS} ${HDRS}

LDADD+=	-lutil -lrpcsvc
DPADD+=	${LIBUTIL} ${LIBRPCSVC}
.PATH:	${.CURDIR}/../common
CFLAGS+=-DDAEMON -DOPTDB -I${.CURDIR}/../common -I.
#CFLAGS=-DDAEMON -I${.CURDIR}/../common  -DDEBUG -g

all:	ypserv
beforedepend:	${HDRS}
ypv1.c: ypv1.h


ypv1.c: ypv1.x
	rpcgen -C -c ${.CURDIR}/ypv1.x -o ypv1.c

ypv1.h: ypv1.x
	rpcgen -C -h ${.CURDIR}/ypv1.x -o ypv1.h

.include <bsd.prog.mk>
