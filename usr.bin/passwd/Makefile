#	$OpenBSD: Makefile,v 1.16 1998/01/20 15:32:19 art Exp $

.include <bsd.own.mk>

PROG=	passwd
SRCS=	local_passwd.c yp_passwd.c passwd.c pwd_gensalt.c new_pwd.c
.PATH:  ${.CURDIR}/../../lib/libc/gen
DPADD+= ${LIBRPCSVC} ${LIBUTIL}
LDADD+= -lrpcsvc -lutil
CFLAGS+= -I${.CURDIR}

.if (${YP} == "yes")
CFLAGS+=-DYP -I${.CURDIR}/../../lib/libc/yp
.endif

.include <bsd.own.mk>	# For Kerberos

.if (${KERBEROS5} == "yes")
#SRCS+= krb5_passwd.c XXX fix the /tmp/tkt_cpw_%d race first + others
CFLAGS+= -DKERBEROS5
DPADD+=	${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lcrypto
.elif (${KERBEROS} == "yes")
.PATH:  ${.CURDIR}/../rlogin
SRCS+= krb_passwd.c des_rw.c
CFLAGS+= -DKERBEROS
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkadm -lkrb -ldes -lcom_err
.endif

BINMODE=4555
BINOWN=root

.include <bsd.prog.mk>

