#	$OpenBSD: Makefile.inc,v 1.1 1998/12/17 02:16:29 smurph Exp $

# Revision is 1.2
REV=12

LDSTATIC=-static

# Hack needed to find kernel images...
BSDSRCDIR?=/usr/src
SRCSYSDIR?=${BSDSRCDIR}/sys
