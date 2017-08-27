#	$OpenBSD: Makefile.inc,v 1.7 2017/08/27 21:59:52 deraadt Exp $

CFLAGS += -fpic
CFLAGS+=-m4-nofpu
AFLAGS += -D_STANDALONE
LDADD=	`$(CC) -print-libgcc-file-name`
