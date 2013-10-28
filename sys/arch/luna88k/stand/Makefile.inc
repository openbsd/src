#	$OpenBSD: Makefile.inc,v 1.1 2013/10/28 22:13:11 miod Exp $
#	$NetBSD: Makefile.inc,v 1.1 2013/01/06 07:04:49 tsutsui Exp $

NOPIE=
CFLAGS+=	-Wall -Werror -fno-stack-protector
CFLAGS+=	-mmemcpy
CFLAGS+=	-fno-builtin-printf -fno-builtin-putchar -fno-builtin-vprintf
