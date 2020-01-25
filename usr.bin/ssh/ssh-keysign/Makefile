#	$OpenBSD: Makefile,v 1.20 2020/01/25 23:02:14 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keysign.c readconf.c compat.c
SRCS+=	cleanup.c fatal.c
SRCS+=	uidswap.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_PKT} \
	${SRCS_UTL} ${SRCS_SK_CLIENT}
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
