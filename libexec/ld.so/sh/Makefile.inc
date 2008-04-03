#	$OpenBSD: Makefile.inc,v 1.4 2008/04/03 00:13:33 drahn Exp $

CFLAGS += -fpic
CFLAGS+=-m4-nofpu
AFLAGS += -D_STANDALONE
AFLAGS += -I${.CURDIR}/../../lib/libc/arch/sh
ELF_LDFLAGS+=-z nocombreloc
LIBCSRCDIR=${.CURDIR}/../../lib/libc
LDADD=	`$(CC) -print-libgcc-file-name`
