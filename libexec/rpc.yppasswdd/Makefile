#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.4 1996/05/22 11:36:00 deraadt Exp $

PROG=	rpc.yppasswdd
SRCS=	rpc.yppasswdd.c yppasswdd_xdr.c yppasswdd_proc.c yppasswdd_mkpw.c \
	pw_util.c pw_copy.c
MAN=	rpc.yppasswdd.8
BINOWN= root
BINMODE=4555
BINDIR=/usr/sbin
MLINKS= rpc.yppasswdd.8 yppasswdd.8
.PATH:	${.CURDIR}/../../usr.sbin/vipw ${.CURDIR}/../../usr.bin/chpass \
	${.CURDIR/../../ypserv
DPADD+= ${LIBUTIL}
LDADD+= -lutil
CFLAGS+=-I${.CURDIR}/../../usr.sbin/vipw -I${.CURDIR}/../../usr.bin/chpass

.include <bsd.prog.mk>
