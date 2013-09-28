#	$OpenBSD: Makefile.inc,v 1.3 2013/09/28 21:59:38 miod Exp $

MDEC_DIR?=	/usr/mdec

CFLAGS+=	-Wall -Werror -fno-stack-protector -mmemcpy
CFLAGS+=	-fno-builtin-printf -fno-builtin-putchar -fno-builtin-vprintf


# Load addresses for bootstrap code
# This should work on 8MB machines, if each bootstrap code fits in 60KB
# (not 64!)
STAGE1_RELOC=0x7c0000
STAGE2_RELOC=0x7d0000
