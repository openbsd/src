.PATH:		${.CURDIR}/..

PROG=	ssh-keygen
BINOWN=	root
BINGRP=	root
BINMODE=555
BINDIR=	/usr/bin
LDADD=	-lkrb -lcrypto -ldes -lgmp -lutil -lz
MAN=	ssh-keygen.1

SRCS=	ssh-keygen.c log-client.c readpass.c rsa.c randoms.c ssh_md5.c \
	buffer.c xmalloc.c authfile.c mpaux.c bufaux.c \
	crc32.c rsaglue.c match.c minfd.c cipher.c compress.c

gen_minfd: gen_minfd.c
minfd.o: minfd.h
minfd.h: gen_minfd
	./gen_minfd $(USER_SHELLS) > minfd.h

.include<bsd.prog.mk>
