.PATH:		${.CURDIR}/..

PROG=	ssh-keygen
BINOWN=	root
BINGRP=	root
BINMODE=555
BINDIR=	/usr/bin
LDADD=	-lkrb -lcrypto -ldes -lgmp -lutil -lz
MAN=	ssh-keygen.1

SRCS=	ssh-agent.c log-client.c rsa.c randoms.c ssh_md5.c buffer.c \
	xmalloc.c bufaux.c authfd.c authfile.c rsaglue.c \
	mpaux.c crc32.c match.c minfd.c cipher.c compress.c

gen_minfd: gen_minfd.c
minfd.o: minfd.h
minfd.h: gen_minfd
	./gen_minfd $(USER_SHELLS) > minfd.h

.include<bsd.prog.mk>
