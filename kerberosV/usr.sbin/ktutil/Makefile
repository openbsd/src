# $OpenBSD: Makefile,v 1.19 2013/06/19 19:37:00 robert Exp $

PROG=	ktutil
MAN=	ktutil.8

SRCS=	add.c \
	change.c \
	copy.c \
	destroy.c \
	get.c \
	ktutil.c \
	ktutil-commands.c \
	ktutil-commands.h \
	list.c \
	purge.c \
	remove.c \
	rename.c

CFLAGS+=-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/sl -I.
DPADD=	${LIBKADM5CLNT} ${LIBKRB5} ${LIBSL} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBEDIT}
LDADD=	-lkadm5clnt -lkrb5 ${LIBSL} -lroken ${LIBVERS} \
	-lasn1 -lcrypto -ledit -lheimbase -lwind -lcom_err \
	-lcurses

.include <bsd.prog.mk>

ktutil-commands.h: ${KRB5DIR}/admin/ktutil-commands.in
	${SLC} ${.ALLSRC:M*.in}

.for ext in c o
ktutil-commands.${ext}: ktutil-commands.h
.endfor

.PATH: ${KRB5DIR}/admin
