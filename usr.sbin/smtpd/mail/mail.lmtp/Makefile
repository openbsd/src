.PATH:		${.CURDIR}/../..

PROG=	mail.lmtp
BINOWN=	root
BINGRP=	wheel

BINMODE?=0555

BINDIR=	/usr/libexec
MAN=	mail.lmtp.8

CFLAGS+=	-fstack-protector-all
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-Werror-implicit-function-declaration

SRCS=	mail.lmtp.c

.include <bsd.prog.mk>
