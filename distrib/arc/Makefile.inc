#	$OpenBSD: Makefile.inc,v 1.5 1998/06/27 20:52:02 todd Exp $

REV=	${OSrev}

LDSTATIC=	-static

# Hack needed to find kernel images...
BSDSRCDIR?=	/usr/src
SRCSYSDIR?=	${BSDSRCDIR}/sys
