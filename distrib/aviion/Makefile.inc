#	$OpenBSD: Makefile.inc,v 1.1 2013/10/15 13:28:04 miod Exp $

REV=${OSrev}

LDSTATIC=-static

# Hack needed to find kernel images...
BSDSRCDIR?=/usr/src
SRCSYSDIR?=${BSDSRCDIR}/sys
