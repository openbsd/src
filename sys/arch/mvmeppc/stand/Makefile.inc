#	$OpenBSD: Makefile.inc,v 1.5 2005/12/30 02:03:49 millert Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=	-fno-stack-protector
CFLAGS+=	-fno-builtin-vprintf -fno-builtin-printf -fno-builtin-putchar

# Load addresses for first and second stage bootstraps 
STAGE1_RELOC=4000
STAGE2_RELOC=6000
STAGE3_RELOC=6000


