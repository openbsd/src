#	$OpenBSD: Makefile,v 1.7 1997/08/19 07:00:50 niklas Exp $
#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90

PROG=	rpc.yppasswdd
SRCS=	rpc.yppasswdd.c yppasswdd_xdr.c yppasswdd_proc.c yppasswdd_mkpw.c
MAN=	rpc.yppasswdd.8
BINOWN= root
BINMODE=555
BINDIR=/usr/sbin
MLINKS= rpc.yppasswdd.8 yppasswdd.8
DPADD+= ${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>
