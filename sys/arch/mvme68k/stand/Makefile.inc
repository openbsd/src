#	$OpenBSD: Makefile.inc,v 1.9 2012/12/31 21:35:32 miod Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=-Wall -Werror -fno-stack-protector
#CFLAGS+=-fno-builtin-printf -fno-builtin-putchar -fno-builtin-vprintf
CFLAGS+=-DHEAP_START=${HEAP_START}

# Load addresses for first and second stage bootstraps
STAGE1_RELOC=	0x00100000 
STAGE2_RELOC=	0x006f0000 

# Base of the heap (ethernet buffers will be allocated below it)
HEAP_START=    0x00800000
