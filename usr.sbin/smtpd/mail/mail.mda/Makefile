.PATH:		${.CURDIR}/../..

PROG=	mail.mda
BINOWN=	root
BINGRP=	wheel

BINMODE?=0555

BINDIR=	/usr/libexec
MAN=	mail.mda.8

CFLAGS+=	-fstack-protector-all
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-Werror-implicit-function-declaration

SRCS=	mail.mda.c

.include <bsd.prog.mk>
