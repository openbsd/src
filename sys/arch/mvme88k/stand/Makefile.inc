#	$OpenBSD: Makefile.inc,v 1.6 2003/10/01 20:39:40 miod Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=	-fno-stack-protector -mmemcpy

# Load addresses for first and second stage bootstraps 
STAGE1_RELOC=0x009F0000
STAGE2_RELOC=0x00AF0000


