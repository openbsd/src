#	$OpenBSD: Makefile.inc,v 1.13 2013/04/18 20:43:16 miod Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=-Wall -Werror -fno-stack-protector -mmemcpy
CFLAGS+=-fno-builtin-printf -fno-builtin-putchar -fno-builtin-vprintf
CFLAGS+=-DHEAP_START=${HEAP_START}

# Load addresses for first and second stage bootstraps
STAGE1_RELOC=	0x00680000
STAGE2_RELOC=	0x00780000

# Base of the heap (ethernet buffers will be allocated below it)
HEAP_START=	0x00700000
