# $OpenBSD: Makefile,v 1.2 2001/07/18 20:24:18 deraadt Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kstash
NOMAN=

SRCS=	kstash.c

.PATH:	${KRB5SRC}/kdc
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	${LIBSL} -lreadline -lcurses -lkrb -ldes -lhdb \
	-lkrb5 -lkadm5srv -lkadm5clnt ${LIBROKEN} -lkafs

.include <bsd.prog.mk>
