#	$OpenBSD: Makefile.inc,v 1.5 2004/07/13 21:03:38 marc Exp $

MDEC_DIR?=/usr/mdec
CFLAGS+=	-fno-stack-protector -Derrno=errno
