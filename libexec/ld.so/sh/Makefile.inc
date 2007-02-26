#	$OpenBSD: Makefile.inc,v 1.2 2007/02/26 19:46:27 miod Exp $

CFLAGS += -fpic
AFLAGS += -D_STANDALONE
AFLAGS += -I${.CURDIR}/../../lib/libc/arch/sh
SRCS+= udivsi3.S sdivsi3.S
ELF_LDFLAGS+=-z nocombreloc
LIBCSRCDIR=${.CURDIR}/../../lib/libc
LDADD=	`$(CC) -print-libgcc-file-name`
