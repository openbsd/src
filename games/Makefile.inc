#	$OpenBSD: Makefile.inc,v 1.4 1997/09/21 11:35:10 deraadt Exp $

BINGRP?=	games
.if defined(HIDEGAME)
BINDIR?=	/usr/games/hide
BINMODE?=	550
.else
BINDIR?=	/usr/games
.endif
