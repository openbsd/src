#	$OpenBSD: Makefile,v 1.8 2001/11/07 19:07:06 deraadt Exp $
#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90

PROG=	rpc.yppasswdd
SRCS=	rpc.yppasswdd.c yppasswdd_proc.c yppasswdd_mkpw.c
MAN=	rpc.yppasswdd.8
BINOWN= root
BINMODE=555
BINDIR=/usr/sbin
MLINKS= rpc.yppasswdd.8 yppasswdd.8
DPADD+= ${LIBUTIL} ${LIBRPCSVC}
LDADD+= -lutil -lrpcsvc

.include <bsd.prog.mk>
