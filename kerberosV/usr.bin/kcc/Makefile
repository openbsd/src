# $OpenBSD: Makefile,v 1.4 2013/06/21 17:02:20 robert Exp $

PROG=	kcc
MAN=	klist.1
LINKS=	${BINDIR}/kcc ${BINDIR}/klist
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

CLEANFILES+=	kcc-commands.[ch]*

kcc-commands.h kcc-commands.c: kcc-commands.in
	${SLC} ${.ALLSRC:M*.in}

kcc-commands.o: kcc-commands.h

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
