#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.5 1996/06/26 22:11:24 maja Exp $

PROG=	rpc.yppasswdd
SRCS=	rpc.yppasswdd.c yppasswdd_xdr.c yppasswdd_proc.c yppasswdd_mkpw.c
MAN=	rpc.yppasswdd.8
BINOWN= root
BINMODE=4555
BINDIR=/usr/sbin
MLINKS= rpc.yppasswdd.8 yppasswdd.8
DPADD+= ${LIBUTIL}
LDADD+= -lutil

.include <bsd.prog.mk>
