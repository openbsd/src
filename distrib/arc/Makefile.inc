#	$OpenBSD: Makefile.inc,v 1.4 1998/03/08 00:26:13 deraadt Exp $

REV=	23

LDSTATIC=	-static

# Hack needed to find kernel images...
BSDSRCDIR?=	/usr/src
SRCSYSDIR?=	${BSDSRCDIR}/sys
