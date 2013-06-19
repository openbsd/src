# $OpenBSD: Makefile,v 1.2 2013/06/19 19:36:53 robert Exp $

PROG=	hxtool
NOMAN=
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl -I.
DPADD=	${LIBHX509} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBSL} ${LIBVERS} ${LIBEDIT}
LDADD=	-lroken -lasn1 -lcrypto ${LIBSL} ${LIBVERS} -ledit -lcurses -lcom_err -lwind
SRCS=	hxtool.c hxtool-commands.c hxtool-commands.h

hxtool-commands.h: hxtool-commands.in
	${SLC} ${.ALLSRC:M*.in}

.for ext in c o
hxtool-commands.${ext}: hxtool-commands.h
.endfor

.include <bsd.prog.mk>
