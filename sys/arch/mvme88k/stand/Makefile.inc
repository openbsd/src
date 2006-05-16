#	$OpenBSD: Makefile.inc,v 1.10 2006/05/16 22:52:07 miod Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=-Wall -Werror -fno-stack-protector -mmemcpy

# Load addresses for first and second stage bootstraps
STAGE1_RELOC=0x009F0000
STAGE2_RELOC=0x00AF0000


