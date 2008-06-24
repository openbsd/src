#	$OpenBSD: Makefile,v 1.30 2008/06/24 14:27:26 deraadt Exp $

.include <bsd.own.mk>

PROG=	passwd
SRCS=	local_passwd.c yp_passwd.c passwd.c pwd_gensalt.c getpwent.c \
	pwd_check.c
.PATH:  ${.CURDIR}/../../lib/libc/gen
DPADD+= ${LIBRPCSVC} ${LIBUTIL}
LDADD+= -lrpcsvc -lutil
CFLAGS+= -I${.CURDIR}

.if (${YP:L} == "yes")
CFLAGS+=-DYP -I${.CURDIR}/../../lib/libc/yp -I${.CURDIR}/../../lib/libc/include
.endif

.include <bsd.own.mk>	# For Kerberos

.if (${KERBEROS5:L} == "yes")
SRCS+= krb5_passwd.c
CFLAGS+= -DKRB5
DPADD+=	${LIBKRB5} ${LIBASN1} ${LIBDES} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lasn1 -ldes -lcrypto
.endif

BINMODE=4555
BINOWN=root

getpwent.o: getpwent.c
	${COMPILE.c} -UYP ${.CURDIR}/../../lib/libc/gen/getpwent.c

.include <bsd.prog.mk>
