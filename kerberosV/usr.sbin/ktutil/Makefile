# $OpenBSD: Makefile,v 1.4 2001/07/18 20:24:19 deraadt Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ktutil
MAN=	ktutil.8

SRCS=	add.c change.c copy.c get.c ktutil.c list.c purge.c remove.c

.PATH:	${KRB5SRC}/admin
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	${LIBSL} -lreadline -lcurses -lkrb -lhdb -lkadm5srv -lkadm5clnt \
	-lhdb -lkrb5 ${LIBROKEN} -lasn1 -lcrypto -lcom_err -lkafs

.include <bsd.prog.mk>
