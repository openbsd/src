.PATH:		${.CURDIR}/..

PROG=	ssh-add
BINOWN=	root
BINMODE=555
BINDIR=	/usr/bin
LDADD=	-lcrypto -lutil -lz
DPADD=	${LIBCRYPTO} ${LIBDES} ${LIBUTIL} ${LIBZ}
MAN=	ssh-add.1

SRCS=	ssh-add.c log-client.c readpass.c rsa.c buffer.c \
	xmalloc.c bufaux.c authfd.c authfile.c crc32.c match.c \
	mpaux.c cipher.c compress.c

.include <bsd.prog.mk>
