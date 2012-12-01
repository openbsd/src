#	$OpenBSD: Makefile.inc,v 1.12 2012/12/01 21:08:18 miod Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=-Wall -Werror -fno-stack-protector -mmemcpy
CFLAGS+=-fno-builtin-printf -fno-builtin-putchar -fno-builtin-vprintf
CFLAGS+=-DHEAP_START=${HEAP_START}

# Load addresses for first and second stage bootstraps
STAGE1_RELOC=	0x009f0000
STAGE2_RELOC=	0x00af0000

# Base of the heap (ethernet buffers will be allocated below it)
HEAP_START=	0x00c00000
