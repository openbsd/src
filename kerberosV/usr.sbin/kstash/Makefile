# $OpenBSD: Makefile,v 1.3 2002/05/11 00:20:20 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kstash
NOMAN=

SRCS=	kstash.c

.PATH:	${KRB5SRC}/kdc
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+= ${LIBREADLINE} ${LIBCURSES} ${LIBKRB} ${LIBDES} ${LIBHDB} ${LIBKRB5} \
	${LIBKADM5SRV} ${LIBKADM5CLNT} ${LIBKAFS}
LDADD+=	${LIBSL} -lreadline -lcurses -lkrb -ldes -lhdb \
	-lkrb5 -lkadm5srv -lkadm5clnt ${LIBROKEN} -lkafs

.include <bsd.prog.mk>
