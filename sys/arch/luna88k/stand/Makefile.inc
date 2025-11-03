#	$OpenBSD: Makefile.inc,v 1.5 2025/11/03 19:15:16 miod Exp $
#	$NetBSD: Makefile.inc,v 1.1 2013/01/06 07:04:49 tsutsui Exp $

NOPIE=
BINDIR=		/usr/mdec
CFLAGS+=	-Wall -Werror -fno-stack-protector
CFLAGS+=	-mmemcpy
CFLAGS+=	-fno-pie
CFLAGS+=	-fno-builtin-printf -fno-builtin-putchar -fno-builtin-vprintf
CFLAGS+=	-fno-builtin-exit
