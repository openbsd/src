.PATH:		${.CURDIR}/../..

PROG=	mail.maildir
BINOWN=	root
BINGRP=	wheel

BINMODE?=0555

BINDIR=	/usr/libexec
MAN=	mail.maildir.8

CFLAGS+=	-fstack-protector-all
CFLAGS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
CFLAGS+=	-Werror-implicit-function-declaration

SRCS=	mail.maildir.c

.include <bsd.prog.mk>
