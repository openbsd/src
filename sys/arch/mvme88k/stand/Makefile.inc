#	$OpenBSD: Makefile.inc,v 1.5 2002/12/02 09:00:25 miod Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=	-fno-stack-protector

# Load addresses for first and second stage bootstraps 
STAGE1_RELOC=0x009F0000
STAGE2_RELOC=0x00AF0000


