#	$OpenBSD: Makefile.inc,v 1.3 1997/10/01 22:12:53 deraadt Exp $

REV=	22

LDSTATIC=	-static

# Hack needed to find kernel images...
BSDSRCDIR?=	/usr/src
SRCSYSDIR?=	${BSDSRCDIR}/sys
