# $OpenBSD: Makefile,v 1.4 2003/05/14 08:01:02 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kstash
NOMAN=

SRCS=	kstash.c \
	sl.c

.PATH:	${KRB5SRC}/kdc
.PATH:	${KRB5SRC}/lib/sl
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+= ${LIBREADLINE} ${LIBCURSES} ${LIBDES} ${LIBHDB} \
	${LIBKRB5} ${LIBCRYPTO} ${LIBKADM5SRV} ${LIBKADM5CLNT}
LDADD+=	-lreadline -lcurses -ldes -lhdb \
	-lkrb5 -lcrypto -lkadm5srv -lkadm5clnt ${LIBROKEN}

.include <bsd.prog.mk>
