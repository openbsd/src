#	$NetBSD: Makefile.inc,v 1.2.6.1 1996/08/29 03:10:06 gwr Exp $

# Revision is 1.2
REV=	12

LDSTATIC=	-static

# Hack needed to find kernel images...
BSDSRCDIR?=	/usr/src
SRCSYSDIR?=	${BSDSRCDIR}/sys
