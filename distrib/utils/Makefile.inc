# $NetBSD: Makefile.inc,v 1.2.4.1 1995/11/21 21:26:41 gwr Exp $
# utils one might want in a crunched binary

BSDSRCDIR?=	/usr/src
LDSTATIC=	-static

install:	.NOTMAIN
	@echo "install not expected"

