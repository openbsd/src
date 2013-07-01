# $OpenBSD: Makefile,v 1.4 2013/07/01 05:15:24 ajacoutot Exp $

PROG=	iprop-log
MPAGES=	iprop-log.8
MSRC=	${KRB5DIR}/lib/kadm5

SRCS=	iprop-log.c \
	kadm5_err.h \
	iprop-commands.c

CFLAGS+=	-I${KRB5DIR}/lib/kadm5 \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl \
		-I.
DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCRYPT} ${LIBSL} ${LIBROKEN} ${LIBVERS} ${LIBEDIT}
LDADD=	-lkadm5srv -lhdb -lkrb5 -lasn1 -lcrypto ${LIBSL} -lroken \
	${LIBVERS} -ledit -lwind -lheimbase \
	-lcurses -lcom_err

CLEANFILES+=	iprop-commands.[ch]*

iprop-commands.h iprop-commands.c: iprop-commands.in
	${SLC} ${.ALLSRC:M*.in}

iprop-commands.o: iprop-commands.h kadm5_err.h

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/kadm5
