#	$OpenBSD: Makefile,v 1.2 2003/07/18 21:18:51 david Exp $

PROG=	tokeninit
SRCS=	tokeninit.c init.c token.c tokendb.c
MAN=	tokeninit.8
LDADD+=	-ldes
DPADD=  ${LIBDES}

CFLAGS+=-I${.CURDIR}/../../libexec/login_token
.PATH:	${.CURDIR}/../../libexec/login_token

TOKENS=	activ crypto snk

MLINKS=	tokeninit.8 activinit.8
MLINKS+=tokeninit.8 cryptoinit.8
MLINKS+=tokeninit.8 snkinit.8

BINMODE=500

afterinstall:
	for i in ${TOKENS} ; do \
	    cd ${DESTDIR}${BINDIR} && \
	    rm -f $${i}init && \
	    ln ${PROG} $${i}init ; \
	done

.include <bsd.prog.mk>
