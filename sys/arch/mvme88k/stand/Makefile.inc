#	$OpenBSD: Makefile.inc,v 1.8 2004/07/13 21:03:38 marc Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=	-fno-stack-protector -mmemcpy -Derrno=errno

# Load addresses for first and second stage bootstraps
STAGE1_RELOC=0x009F0000
STAGE2_RELOC=0x00AF0000


