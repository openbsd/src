# $OpenBSD: Makefile,v 1.11 2004/06/04 23:45:30 biorn Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ktutil
MAN=	ktutil.8

SRCS=	add.c \
	change.c \
	copy.c \
	get.c \
	ktutil.c \
	list.c \
	purge.c \
	remove.c \
	rename.c \
	sl.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/admin
.PATH:	${KRB5SRC}/lib/sl

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5

DPADD+=	${LIBREADLINE} ${LIBCURSES} ${LIBKADM5CLNT} ${LIBKRB5} \
	${LIBCRYPTO} ${LIBDES}
LDADD+=	-lreadline -lcurses -lkadm5clnt -lkrb5 \
	-lcrypto -ldes

.include <bsd.prog.mk>
