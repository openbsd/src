#	$OpenBSD: Makefile.inc,v 1.2 1997/09/21 11:45:53 deraadt Exp $

# Revision is 2.0
REV=	20

LDSTATIC=	-static

# Hack needed to find kernel images...
BSDSRCDIR?=	/usr/src
SRCSYSDIR?=	${BSDSRCDIR}/sys
