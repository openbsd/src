# $OpenBSD: Makefile,v 1.3 2013/06/21 17:02:20 robert Exp $

PROG=	iprop-log
MAN=	iprop-log.8

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
