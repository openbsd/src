#	$OpenBSD: Makefile.inc,v 1.1 2004/05/25 21:48:00 mickey Exp $

CFLAGS += -fpic
AFLAGS += -fpic
#ELF_LDFLAGS=-t
LDADD=	`$(CC) -print-libgcc-file-name`
