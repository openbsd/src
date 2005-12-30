#	$OpenBSD: Makefile.inc,v 1.9 2005/12/30 02:03:49 millert Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=	-fno-stack-protector -mmemcpy

# Load addresses for first and second stage bootstraps
STAGE1_RELOC=0x009F0000
STAGE2_RELOC=0x00AF0000


