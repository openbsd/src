.PATH:	${.CURDIR}/../../usr.sbin/vipw ${.CURDIR}/../../usr.bin/chpass \
	${.CURDIR}/../../usr.sbin/ypserv

PROG=	rpc.yppasswdd
SRCS=	rpc.yppasswdd.c yppasswdd_xdr.c yppasswdd_proc.c yppasswdd_mkpw.c \
	pw_util.c pw_copy.c yplog.c
MAN=	rpc.yppasswdd.8
BINOWN= root
BINMODE=4555

MLINKS= rpc.yppasswdd.8 yppasswdd.8

DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt

#CFLAGS+=-DDAEMON
CFLAGS+=-I${.CURDIR}/../../usr.sbin/ypserv -DDEBUG

.include <bsd.prog.mk>
