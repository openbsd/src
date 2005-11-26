#	$OpenBSD: Makefile,v 1.9 2005/11/26 04:06:59 deraadt Exp $
#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90

PROG=	rpc.yppasswdd
SRCS=	rpc.yppasswdd.c yppasswdd_proc.c yppasswdd_mkpw.c
MAN=	rpc.yppasswdd.8
BINDIR=/usr/sbin
MLINKS= rpc.yppasswdd.8 yppasswdd.8
DPADD+= ${LIBUTIL} ${LIBRPCSVC}
LDADD+= -lutil -lrpcsvc

.include <bsd.prog.mk>
