#	$OpenBSD: Makefile,v 1.8 2002/01/01 21:10:59 deraadt Exp $

PROG=	ypserv
SRCS=	ypserv.c ypserv_proc.c ypserv_db.c acl.c yplog.c ypdb.c \
	ypserv_xdr.c ypserv_xdr_v1.c
MAN=	ypserv.acl.5 securenet.5 ypserv.8
LDADD+=	-lutil -lrpcsvc
DPADD+=	${LIBUTIL} ${LIBRPCSVC}
.PATH:	${.CURDIR}/../common
CFLAGS+=-DDAEMON -DOPTDB -I${.CURDIR}/../common
#CFLAGS=-DDAEMON -I${.CURDIR}/../common  -DDEBUG -g

.include <bsd.prog.mk>
