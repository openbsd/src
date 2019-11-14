#	$OpenBSD: Makefile,v 1.17 2019/11/14 21:27:31 djm Exp $

.PATH:		${.CURDIR}/..

SRCS=	ssh-keysign.c readconf.c compat.c
SRCS+=	atomicio.c cleanup.c fatal.c msg.c
SRCS+=	uidswap.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} ${SRCS_PKT} \
	${SRCS_UTL}
PROG=	ssh-keysign
BINOWN=	root
BINMODE=4555

BINDIR=	/usr/libexec
MAN=	ssh-keysign.8

.include <bsd.prog.mk>

LDADD+= -lfido2 -lcbor -lusbhid
DPADD+=	${LIBFIDO2} ${LIBCBOR} ${LIBUSBHID}

LDADD+=	-lcrypto -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}
