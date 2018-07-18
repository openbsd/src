#	$OpenBSD: Makefile,v 1.14 2018/07/18 11:34:05 dtucker Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keysign.c readconf.c
SRCS+=	atomicio.c cleanup.c fatal.c msg.c
SRCS+=	uidswap.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_PKT} \
	${SRCS_UTL}
PROG=	ssh-keysign
BINOWN=	root

BINMODE?=4555

BINDIR=	/usr/libexec
MAN=	ssh-keysign.8

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}
