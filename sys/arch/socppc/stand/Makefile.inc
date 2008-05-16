#	$OpenBSD: Makefile.inc,v 1.2 2008/05/16 18:57:01 kettenis Exp $

.if exists(${STANDIR}/mkboot/mkboot)
MKBOOT=${STANDIR}/mkboot/mkboot
.else
MKBOOT=${STANDIR}/mkboot/${__objdir}/mkboot
.endif

BINDIR=/usr/mdec
