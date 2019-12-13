#	$OpenBSD: Makefile,v 1.15 2019/12/13 19:09:10 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keyscan.c
SRCS+=	atomicio.c cleanup.c compat.c hostfile.c ssh_api.c dns.c
SRCS+=	${SRCS_BASE} ${SRCS_KEX} ${SRCS_KEXC} ${SRCS_KEXS} ${SRCS_KEY} \
	${SRCS_PKT} ${SRCS_UTL} ${SRCS_SK_CLIENT}
PROG=	ssh-keyscan

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+= -lcrypto -lz -lutil
DPADD+= ${LIBCRYPTO} ${LIBZ} ${LIBUTIL}

