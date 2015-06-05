#	$OpenBSD: Makefile.inc,v 1.3 2015/06/05 18:14:26 miod Exp $

BINDIR=/usr/mdec
MANSUBDIR=armish

CFLAGS+=	-fno-pie
LDFLAGS+=	-nopie
