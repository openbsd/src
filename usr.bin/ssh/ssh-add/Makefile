.PATH:		${.CURDIR}/..

PROG=	ssh-add
BINOWN=	root
BINMODE=555
BINDIR=	/usr/bin
LDADD=	-lcrypto -ldes -lutil -lz
DPADD=	${LIBCRYPTO} ${LIBDES} ${LIBUTIL} ${LIBZ}
MAN=	ssh-add.1

SRCS=	ssh-add.c log-client.c readpass.c rsa.c buffer.c \
	xmalloc.c bufaux.c authfd.c authfile.c crc32.c match.c \
	mpaux.c minfd.c cipher.c compress.c

gen_minfd: gen_minfd.c
minfd.o: minfd.h
minfd.h: gen_minfd
	./gen_minfd $(USER_SHELLS) > minfd.h

.include <bsd.prog.mk>
