#	$OpenBSD: Makefile.inc,v 1.3 1997/09/21 11:46:05 deraadt Exp $

# Revision is 1.2
REV=	12

LDSTATIC=	-static

# Hack needed to find kernel images...
BSDSRCDIR?=	/usr/src
SRCSYSDIR?=	${BSDSRCDIR}/sys
