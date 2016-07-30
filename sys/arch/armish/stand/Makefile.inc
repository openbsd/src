#	$OpenBSD: Makefile.inc,v 1.4 2016/07/30 03:25:48 guenther Exp $

BINDIR=/usr/mdec
MANSUBDIR=armish

CFLAGS+=	-fno-pie
LDFLAGS+=	-nopie -znorelro
