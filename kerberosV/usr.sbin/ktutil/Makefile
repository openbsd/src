# $OpenBSD: Makefile,v 1.2 2001/06/22 16:12:19 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ktutil
MAN=	ktutil.8

SRCS=	add.c change.c copy.c get.c ktutil.c list.c purge.c remove.c

.PATH:	${KRB5SRC}/admin
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	${LIBSL} -lreadline -lcurses -lkrb -ldes -lhdb \
	-lkadm5srv -lkadm5clnt -lhdb -lkrb5 ${LIBROKEN} -lasn1 -lcrypto

.include <bsd.prog.mk>
