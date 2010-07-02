#	$OpenBSD: Makefile.inc,v 1.1 2010/07/02 23:09:17 jsing Exp $

CFLAGS += -fpic
AFLAGS += -fpic
#ELF_LDFLAGS=-t
LDADD=	`$(CC) -print-libgcc-file-name`
