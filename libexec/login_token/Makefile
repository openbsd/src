#	$OpenBSD: Makefile,v 1.4 2002/11/21 22:14:51 millert Exp $

PROG=	login_token
SRCS=	login_token.c init.c token.c tokendb.c
MAN=	login_token.8
DPADD=	${LIBDES}
LDADD=	-ldes

TOKENS=	activ crypto snk

MLINKS=	login_token.8 login_activ.8
MLINKS+=login_token.8 login_crypto.8
MLINKS+=login_token.8 login_snk.8

afterinstall:
	for i in ${TOKENS} ; do \
	    cd ${DESTDIR}${BINDIR} && \
	    rm -f login_$$i && \
	    ln ${PROG} login_$$i ; \
	done

BINOWN=	root
BINGRP=	_token
BINMODE=2555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
