.PATH:		${.CURDIR}/..

PROG=	ssh-keyscan
BINOWN=	root

BINMODE?=555

BINDIR=	/usr/bin
MAN=	ssh-keyscan.1

SRCS=	ssh-keyscan.c log-client.c

.include <bsd.prog.mk>

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
