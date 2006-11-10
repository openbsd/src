#	$OpenBSD: Makefile.inc,v 1.1 2006/11/10 07:44:58 drahn Exp $

CFLAGS += -fpic
AFLAGS += -D_STANDALONE
AFLAGS += -I${.CURDIR}/../../lib/libc/arch/sh
SRCS+= udivsi3.S sdivsi3.S
ELF_LDFLAGS+=-z nocombreloc
LIBCSRCDIR=${.CURDIR}/../../lib/libc
