#	$OpenBSD: Makefile,v 1.24 2001/11/22 20:37:18 espie Exp $

.include <bsd.own.mk>

PROG=	passwd
SRCS=	local_passwd.c yp_passwd.c passwd.c pwd_gensalt.c getpwent.c \
	pwd_check.c
.PATH:  ${.CURDIR}/../../lib/libc/gen
DPADD+= ${LIBRPCSVC} ${LIBUTIL}
LDADD+= -lrpcsvc -lutil
CFLAGS+= -I${.CURDIR}

.if (${YP:L} == "yes")
CFLAGS+=-DYP -I${.CURDIR}/../../lib/libc/yp
.endif

.include <bsd.own.mk>	# For Kerberos

.if (${KERBEROS5:L} == "yes")
SRCS+= krb5_passwd.c
CFLAGS+= -DKRB5
DPADD+=	${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lcrypto -lasn1
.endif

.if (${KERBEROS:L} == "yes")
.PATH:  ${.CURDIR}/../rlogin
SRCS+= new_pwd.c krb_passwd.c des_rw.c
CFLAGS+= -DKERBEROS
DPADD+=	${LIBKRB} ${LIBDES}
LDADD+=	-lkadm -lkrb -ldes -lcom_err -lkafs
.endif

BINMODE=4555
BINOWN=root

getpwent.o: getpwent.c
	${COMPILE.c} -UYP ${.CURDIR}/../../lib/libc/gen/getpwent.c

.include <bsd.prog.mk>
