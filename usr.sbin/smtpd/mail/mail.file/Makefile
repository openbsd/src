.PATH:		${.CURDIR}/../..

PROG=	mail.file
BINOWN=	root
BINGRP=	wheel

BINMODE?=0555

BINDIR=	/usr/libexec
MAN=	mail.file.8

CFLAGS+=	-fstack-protector-all
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-Werror-implicit-function-declaration

SRCS=	mail.file.c

.include <bsd.prog.mk>
