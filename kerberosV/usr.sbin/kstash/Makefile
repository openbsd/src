# $OpenBSD: Makefile,v 1.5 2003/07/14 16:14:45 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kstash
NOMAN=

SRCS=	kstash.c \
	sl.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/kdc
.PATH:	${KRB5SRC}/lib/sl

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5

DPADD+= ${LIBREADLINE} ${LIBCURSES} ${LIBDES} ${LIBKRB5} \
	${LIBCRYPTO} ${LIBKADM5SRV} ${LIBKADM5CLNT}
LDADD+=	-lreadline -lcurses -ldes -lkrb5 \
	-lcrypto -lkadm5srv -lkadm5clnt

.include <bsd.prog.mk>
