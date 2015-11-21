#	$OpenBSD: Makefile,v 1.10 2015/11/21 19:41:59 jmc Exp $
#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90

PROG=	rpc.yppasswdd
SRCS=	rpc.yppasswdd.c yppasswdd_proc.c yppasswdd_mkpw.c
MAN=	rpc.yppasswdd.8
BINDIR=/usr/sbin
DPADD+= ${LIBUTIL} ${LIBRPCSVC}
LDADD+= -lutil -lrpcsvc

.include <bsd.prog.mk>
