#	$OpenBSD: Makefile.inc,v 1.1 2006/05/16 22:48:16 miod Exp $

MDEC_DIR?=	/usr/mdec

CFLAGS+=	-Wall -Werror -fno-stack-protector -mmemcpy

# Load addresses for bootstrap code
# This will work on 8MB machines, if each bootstrap code fits in 60KB
# (not 64!)
STAGE1_RELOC=0x7e0000
STAGE2_RELOC=0x7f0000
