#	$OpenBSD: Makefile.inc,v 1.7 2003/10/11 22:08:57 miod Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=	-fno-stack-protector -mmemcpy

# Load addresses for first and second stage bootstraps
STAGE1_RELOC=0x009F0000
STAGE2_RELOC=0x00AF0000


