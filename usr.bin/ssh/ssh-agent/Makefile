#	$OpenBSD: Makefile,v 1.18 2001/06/26 05:33:35 markus Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh-agent
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	ssh-agent.1

SRCS=	ssh-agent.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}

.if (${SMARTCARD:L} == "yes")
CFLAGS+= -DSMARTCARD
LDADD+=	-lsectok
.endif # SMARTCARD
