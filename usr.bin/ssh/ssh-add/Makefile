.PATH:		${.CURDIR}/..

PROG=	ssh-add
BINOWN=	root
BINGRP=	root
BINMODE=555
BINDIR=	/usr/bin
LDADD=	-lkrb -lcrypto -ldes -lgmp -lutil -lz
MAN=	ssh-add.1

SRCS=	ssh-add.o log-client.o readpass.o rsa.o randoms.o ssh_md5.o buffer.o \
	xmalloc.o bufaux.o authfd.o authfile.o crc32.c rsaglue.c match.c \
	mpaux.c minfd.c cipher.c compress.c

gen_minfd: gen_minfd.c

minfd.o: minfd.h
minfd.h: gen_minfd
	./gen_minfd $(USER_SHELLS) > minfd.h

.include<bsd.prog.mk>
