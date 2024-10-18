#	$OpenBSD: Makefile,v 1.17 2024/10/18 05:32:51 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keyscan.c
SRCS+=	atomicio.c cleanup.c compat.c hostfile.c ssh_api.c dns.c fatal.c
SRCS+=	${SRCS_BASE} ${SRCS_KEX} ${SRCS_KEXC} ${SRCS_KEXS} ${SRCS_KEY} \
	${SRCS_PKT} ${SRCS_UTL} ${SRCS_SK_CLIENT}
PROG=	ssh-keyscan

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+= -lcrypto -lutil
DPADD+= ${LIBCRYPTO} ${LIBUTIL}

.if (${ZLIB:L} == "yes")
LDADD+=	-lz
DPADD+=	${LIBZ}
.endif

