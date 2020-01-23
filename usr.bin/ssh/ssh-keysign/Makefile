#	$OpenBSD: Makefile,v 1.19 2020/01/23 10:24:30 dtucker Exp $

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

LDADD+=	-lcrypto -lutil
DPADD+=	${LIBCRYPTO} ${LIBUTIL}

.if (${ZLIB:L} == "yes")
LDADD+=	-lz
DPADD+=	${LIBZ}
.endif
