# $OpenBSD: Makefile,v 1.23 2013/07/10 10:43:32 ajacoutot Exp $

PROG=	ktutil
MPAGES=	ktutil.8
MSRC=	${KRB5DIR}/admin

SRCS=	add.c \
	change.c \
	copy.c \
	destroy.c \
	get.c \
	ktutil.c \
	ktutil-commands.c \
	list.c \
	purge.c \
	remove.c \
	rename.c

CPPFLAGS=	-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/sl -I.
DPADD=	${LIBKADM5CLNT} ${LIBKRB5} ${LIBSL} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBEDIT}
LDADD=	-lkadm5clnt -lkrb5 ${LIBSL} -lroken ${LIBVERS} \
	-lasn1 -lcrypto -ledit -lheimbase -lwind -lcom_err \
	-lcurses

CLEANFILES+=	ktutil-commands.[ch]*

.include <bsd.prog.mk>

ktutil-commands.h ktutil-commands.c: ${KRB5DIR}/admin/ktutil-commands.in
	${SLC} ${.ALLSRC:M*.in}

ktutil-commands.o: ktutil-commands.h

.PATH: ${KRB5DIR}/admin
