.PATH:		${.CURDIR}/..

PROG=	ssh-add
BINOWN=	root
BINMODE=555
BINDIR=	/usr/bin
LDADD=	-lkrb -lcrypto -ldes -lgmp -lutil -lz
MAN=	ssh-add.1

SRCS=	ssh-add.c log-client.c readpass.c rsa.c randoms.c ssh_md5.c buffer.c \
	xmalloc.c bufaux.c authfd.c authfile.c crc32.c rsaglue.c match.c \
	mpaux.c minfd.c cipher.c compress.c

gen_minfd: gen_minfd.c
minfd.o: minfd.h
minfd.h: gen_minfd
	./gen_minfd $(USER_SHELLS) > minfd.h

.include <bsd.prog.mk>
