#	$OpenBSD: Makefile.inc,v 1.2 2003/08/07 20:57:03 miod Exp $

REV=${OSrev}

LDSTATIC=-static

# Hack needed to find kernel images...
BSDSRCDIR?=/usr/src
SRCSYSDIR?=${BSDSRCDIR}/sys
