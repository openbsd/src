#	$OpenBSD: Makefile.inc,v 1.2 1997/09/21 11:45:57 deraadt Exp $

# Revision is 1.2
REV=	12

LDSTATIC=	-static

# Hack needed to find kernel images...
BSDSRCDIR?=	/usr/src
SRCSYSDIR?=	${BSDSRCDIR}/sys
