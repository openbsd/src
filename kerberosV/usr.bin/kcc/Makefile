# $OpenBSD: Makefile,v 1.2 2013/06/19 19:36:54 robert Exp $

PROG=	kcc
MAN=	klist.1 kswitch.1
LINKS=	${BINDIR}/kcc ${BINDIR}/klist \
	${BINDIR}/kcc ${BINDIR}/kswitch
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl -I.
DPADD=	${LIBKRB5} ${LIBHEIMNTLM} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCRYPT} ${LIBKAFS5} ${LIBSL} ${LIBVERS} ${LIBEDIT}
LDADD=	-lkrb5 -lroken -lasn1 -lcrypto -lkafs5 ${LIBSL} \
	${LIBVERS} -ledit -lcurses -lwind -lcom_err -lheimbase
SRCS=	kcc.c \
	kcc-commands.c \
	kcc-commands.h \
	klist.c \
	kswitch.c \
	copy_cred_cache.c

kcc-commands.h: kcc-commands.in
	${SLC} ${.ALLSRC:M*.in}

.for ext in c o
kcc-commands.${ext}: kcc-commands.h
.endfor

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
