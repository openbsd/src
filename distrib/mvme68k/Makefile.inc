#	$OpenBSD: Makefile.inc,v 1.3 2000/01/24 04:50:25 smurph Exp $

REV=${OSrev}

LDSTATIC=	-static

# Hack needed to find kernel images...
BSDSRCDIR?=	/usr/src
SRCSYSDIR?=	${BSDSRCDIR}/sys
