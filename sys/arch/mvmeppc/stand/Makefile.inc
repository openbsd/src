#	$OpenBSD: Makefile.inc,v 1.4 2004/11/15 14:03:19 miod Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=	-fno-stack-protector -Derrno=errno
CFLAGS+=	-fno-builtin-vprintf -fno-builtin-printf -fno-builtin-putchar

# Load addresses for first and second stage bootstraps 
STAGE1_RELOC=4000
STAGE2_RELOC=6000
STAGE3_RELOC=6000


