#	$OpenBSD: Makefile.inc,v 1.3 2008/06/09 15:01:56 deraadt Exp $

.if exists(${STANDIR}/mkboot/mkboot)
MKBOOT=${STANDIR}/mkboot/mkboot
.else
MKBOOT=${STANDIR}/mkboot/${__objdir}/mkboot
.endif

BINDIR=/usr/mdec
MANSUBDIR=socppc
