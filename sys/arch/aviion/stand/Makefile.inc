#	$OpenBSD: Makefile.inc,v 1.4 2013/10/08 21:55:20 miod Exp $

MDEC_DIR?=	/usr/mdec

CWARNFLAGS=	-Wall -Werror -fno-stack-protector

HOSTCFLAGS+=	${CWARNFLAGS}
CFLAGS+=	${CWARNFLAGS} -mmemcpy
CFLAGS+=	-fno-builtin-printf -fno-builtin-putchar -fno-builtin-vprintf

# Load addresses for bootstrap code
# This should work on 8MB machines, if each bootstrap code fits in 60KB
# (not 64!)
STAGE1_RELOC=0x7c0000
STAGE2_RELOC=0x7d0000
