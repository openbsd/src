# $OpenBSD: Makefile,v 1.1 2013/06/17 19:11:35 robert Exp $

PROG=	kdigest
MAN=	kdigest.8
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl -I.
DPADD=	${LIBKRB5} ${LIBHEIMNTLM} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCRYPT} ${LIBSL} ${LIBVERS} ${LIBEDIT}
LDADD=	-lkrb5 -lheimntlm -lroken -lasn1 -lcrypto ${LIBSL} \
	${LIBVERS} -ledit -lhx509 -lwind -lpthread \
	-lcom_err -lheimbase -lcurses
SRCS=	kdigest.c \
	kdigest-commands.c \
	kdigest-commands.h

kdigest-commands.h: kdigest-commands.in
	${SLC} ${.ALLSRC:M*.in}

.for ext in c o
kdigest-commands.${ext}: kdigest-commands.h
.endfor

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
