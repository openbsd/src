# $OpenBSD: Makefile,v 1.1 2001/05/25 08:21:12 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kstash
NOMAN=

SRCS=	kstash.c

.PATH:	${KRB5SRC}/kdc
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	${LIBSL} -lreadline -lcurses -lkrb -ldes -lhdb \
	-lkrb5 -lkadm5srv -lkadm5clnt ${LIBROKEN}

.include <bsd.prog.mk>
