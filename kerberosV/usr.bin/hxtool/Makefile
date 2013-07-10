# $OpenBSD: Makefile,v 1.4 2013/07/10 10:43:32 ajacoutot Exp $

PROG=	hxtool
NOMAN=
CPPFLAGS=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl -I.
DPADD=	${LIBHX509} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} ${LIBSL} ${LIBVERS} ${LIBEDIT}
LDADD=	-lroken -lasn1 -lcrypto ${LIBSL} ${LIBVERS} -ledit -lcurses -lcom_err -lwind
SRCS=	hxtool.c hxtool-commands.c hxtool-commands.h

hxtool-commands.h: hxtool-commands.in
	${SLC} ${.ALLSRC:M*.in}

.for ext in c o
hxtool-commands.${ext}: hxtool-commands.h
.endfor

.include <bsd.prog.mk>
