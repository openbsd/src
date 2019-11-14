#	$OpenBSD: Makefile,v 1.14 2019/11/14 21:27:30 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keyscan.c
SRCS+=	atomicio.c cleanup.c compat.c hostfile.c ssh_api.c dns.c
SRCS+=	${SRCS_BASE} ${SRCS_KEX} ${SRCS_KEXC} ${SRCS_KEXS} ${SRCS_KEY} \
	${SRCS_PKT} ${SRCS_UTL}
PROG=	ssh-keyscan

BINDIR=	/usr/bin

.include <bsd.prog.mk>

LDADD+= -lfido2 -lcbor -lusbhid
DPADD+=	${LIBFIDO2} ${LIBCBOR} ${LIBUSBHID}

LDADD+= -lcrypto -lz -lutil
DPADD+= ${LIBCRYPTO} ${LIBZ} ${LIBUTIL}

