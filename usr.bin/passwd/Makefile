#	$OpenBSD: Makefile,v 1.11 1997/01/02 16:34:51 etheisen Exp $

.include <bsd.own.mk>

PROG=	passwd
SRCS=	local_passwd.c yp_passwd.c passwd.c getpwent.c
.PATH:  ${.CURDIR}/../../lib/libc/gen
DPADD+= ${LIBRPCSVC} ${LIBCRYPT} ${LIBUTIL}
LDADD+= -lrpcsvc -lutil
CFLAGS+=-I${.CURDIR}

.if defined(YP)
CFLAGS+=-DYP -I${.CURDIR}/../../lib/libc/yp
.endif

.include <bsd.own.mk>	# For Kerberos

.if defined(KERBEROS5)
#SRCS+= krb5_passwd.c XXX fix the /tmp/tkt_cpw_%d race first
CFLAGS+= -DKERBEROS5
DPADD+=	${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lcrypto
.elif defined(KERBEROS)
.PATH:  ${.CURDIR}/../rlogin
SRCS+= krb_passwd.c des_rw.c
CFLAGS+= -DKERBEROS
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkrb -ldes
.endif

BINMODE=4555
BINOWN=	root

.include <bsd.prog.mk>

getpwent.o: getpwent.c
	${COMPILE.c} -UYP ${.IMPSRC}
