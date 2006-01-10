#	$OpenBSD: Makefile.inc,v 1.7 2006/01/10 07:36:29 miod Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=	-fno-stack-protector

# Load addresses for first and second stage bootstraps
STAGE1_RELOC=0x10000 
STAGE2_RELOC=0x6F0000 
