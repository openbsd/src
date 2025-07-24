#	$OpenBSD: Makefile,v 1.22 2025/07/24 05:44:55 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keysign.c readconf.c compat.c
SRCS+=	cleanup.c fatal.c
SRCS+=	uidswap.c utf8.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_PKT} \
	${SRCS_UTL} ${SRCS_SK_CLIENT} ${SRCS_PKCS11_CLIENT}
PROG=	ssh-keysign
BINOWN=	root
BINMODE=4555

BINDIR=	/usr/libexec
MAN=	ssh-keysign.8

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}

.if (${ZLIB:L} == "yes")
LDADD+=	-lz
DPADD+=	${LIBZ}
.endif
