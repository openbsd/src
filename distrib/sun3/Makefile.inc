#	$NetBSD: Makefile.inc,v 1.2 1995/10/13 16:01:02 gwr Exp $

# Revision is 1.1
REV=	11

LDSTATIC=	-static

# Hack needed to find kernel images...
BSDSRCDIR?=	/usr/src
SRCSYSDIR?=	${BSDSRCDIR}/sys
