#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90
#	$Id: Makefile,v 1.2 1995/11/01 17:40:33 deraadt Exp $

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
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
CFLAGS+=-I${.CURDIR}/../../usr.sbin/ypserv -I${.CURDIR}/../../usr.sbin/vipw \
	-I${.CURDIR}/../../usr.bin/chpass

.include <bsd.prog.mk>
