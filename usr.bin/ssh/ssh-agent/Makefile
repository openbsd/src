.PATH:		${.CURDIR}/..

PROG=	ssh-agent
BINOWN=	root
BINMODE=555
BINDIR=	/usr/bin
LDADD=	-lcrypto -lutil -lz
DPADD=	${LIBCRYPTO} ${LIBDES} ${LIBUTIL} ${LIBZ}
MAN=	ssh-agent.1

SRCS=	ssh-agent.c log-client.c rsa.c buffer.c \
	xmalloc.c bufaux.c authfd.c authfile.c \
	mpaux.c crc32.c match.c minfd.c cipher.c compress.c

gen_minfd: gen_minfd.c
minfd.o: minfd.h
minfd.h: gen_minfd
	./gen_minfd $(USER_SHELLS) > minfd.h

.include <bsd.prog.mk>
