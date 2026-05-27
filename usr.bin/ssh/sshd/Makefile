#	$OpenBSD: Makefile,v 1.114 2026/05/27 13:57:26 deraadt Exp $

.PATH:		${.CURDIR}/..

SRCS=	sshd.c servconf.c sshpty.c srclimit.c groupaccess.c auth2-methods.c
SRCS+=	dns.c fatal.c compat.c utf8.c authfd.c canohost.c kex-names.c
SRCS+=	${SRCS_BASE} ${SRCS_KEY} ${SRCS_KEYP} ${SRCS_KRL} \
	${SRCS_MAC} ${SRCS_UTL} ${SRCS_SK_CLIENT} ${SRCS_PKCS11_CLIENT}

PROG=	sshd
RELINK=./${PROG} -V # test it works
BINMODE=511
BINDIR=	/usr/sbin
MAN=	sshd.8 sshd_config.5

.include <bsd.prog.mk>

.if (${OPENSSL:L} == "yes")
LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
.endif

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.if (${ZLIB:L} == "yes")
LDADD+=	-lz
DPADD+=	${LIBZ}
.endif
