#	$OpenBSD: Makefile.inc,v 1.2 2019/04/29 01:48:40 deraadt Exp $

LDSTATIC=-static

# Hack needed to find kernel images...
BSDSRCDIR?=/usr/src
SRCSYSDIR?=${BSDSRCDIR}/sys
