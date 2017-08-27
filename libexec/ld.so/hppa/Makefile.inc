#	$OpenBSD: Makefile.inc,v 1.2 2017/08/27 21:59:52 deraadt Exp $

CFLAGS += -fpic
AFLAGS += -fpic
LDADD=	`$(CC) -print-libgcc-file-name`
