#	$OpenBSD: Makefile.inc,v 1.1 1997/05/18 13:40:44 pefo Exp $

# Revision is 2.1
REV=	21

LDSTATIC=	-static

# Hack needed to find kernel images...
BSDSRCDIR?=	/usr/src
SRCSYSDIR?=	${BSDSRCDIR}/sys
