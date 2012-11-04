#	$OpenBSD: Makefile.inc,v 1.8 2012/11/04 13:36:47 miod Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=	-fno-stack-protector

# Load addresses for first and second stage bootstraps
STAGE1_RELOC=0x100000 
STAGE2_RELOC=0x6F0000 
