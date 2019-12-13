#	$OpenBSD: Makefile,v 1.18 2019/12/13 19:09:10 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keysign.c readconf.c compat.c
SRCS+=	atomicio.c cleanup.c fatal.c
SRCS+=	uidswap.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_PKT} \
	${SRCS_UTL} ${SRCS_SK_CLIENT}
PROG=	ssh-keysign
BINOWN=	root
BINMODE=4555

BINDIR=	/usr/libexec
MAN=	ssh-keysign.8

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}
