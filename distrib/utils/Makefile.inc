# $NetBSD: Makefile.inc,v 1.3 1995/11/21 21:17:19 gwr Exp $
# utils one might want in a crunched binary

BSDSRCDIR?=	/usr/src
LDSTATIC=	-static

install:	.NOTMAIN
	@echo "install not expected"

