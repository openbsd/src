#	$OpenBSD: Makefile.inc,v 1.2 2006/05/20 22:38:32 miod Exp $

MDEC_DIR?=	/usr/mdec

CFLAGS+=	-Wall -Werror -fno-stack-protector -mmemcpy

# Load addresses for bootstrap code
# This should work on 8MB machines, if each bootstrap code fits in 60KB
# (not 64!)
STAGE1_RELOC=0x7c0000
STAGE2_RELOC=0x7d0000
