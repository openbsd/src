#	$OpenBSD: Makefile.inc,v 1.3 2004/07/13 21:03:38 marc Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=	-fno-stack-protector -Derrno=errno

# Load addresses for first and second stage bootstraps 
STAGE1_RELOC=4000
STAGE2_RELOC=6000
STAGE3_RELOC=6000


