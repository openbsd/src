#	from: @(#)Makefile	5.11 (Berkeley) 2/19/91
#	$Id: Makefile,v 1.5 1996/02/15 05:02:25 etheisen Exp $

PROG=	passwd
SRCS=	local_passwd.c yp_passwd.c passwd.c pw_copy.c pw_util.c getpwent.c
.PATH:  ${.CURDIR}/../../usr.bin/chpass ${.CURDIR}/../../usr.sbin/vipw \
	${.CURDIR}/../rlogin ${.CURDIR}/../../lib/libc/gen
#DPADD+= ${LIBRPCSVC} ${LIBCRYPT}
DPADD+= ${LIBRPCSVC}
#LDADD+= -lrpcsvc -lcrypt
LDADD+= -lrpcsvc
CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../usr.sbin/vipw -DYP

.if defined(KERBEROS5)
SRCS+= krb5_passwd.c
CFLAGS+= -DKERBEROS5
DPADD+=	${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lcrypto
.elif defined(KERBEROS)
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
