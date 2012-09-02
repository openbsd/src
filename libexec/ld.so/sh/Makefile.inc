#	$OpenBSD: Makefile.inc,v 1.5 2012/09/02 16:32:52 jsg Exp $

CFLAGS += -fpic
CFLAGS+=-m4-nofpu
AFLAGS += -D_STANDALONE
CPPFLAGS += -I${.CURDIR}/../../lib/libc/arch/sh
ELF_LDFLAGS+=-z nocombreloc
LIBCSRCDIR=${.CURDIR}/../../lib/libc
LDADD=	`$(CC) -print-libgcc-file-name`
