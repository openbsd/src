# $OpenBSD: Makefile,v 1.5 2013/07/10 10:43:32 ajacoutot Exp $

PROG=	kdigest
MPAGES=	kdigest.8
MSRC=	${KRB5DIR}/kuser

CPPFLAGS=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl -I.
DPADD=	${LIBKRB5} ${LIBHEIMNTLM} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBSL} ${LIBVERS} ${LIBEDIT}
LDADD=	-lkrb5 -lheimntlm -lroken -lasn1 -lcrypto ${LIBSL} \
	${LIBVERS} -ledit -lwind \
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
