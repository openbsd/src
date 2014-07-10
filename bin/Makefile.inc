#	$OpenBSD: Makefile.inc,v 1.5 2014/07/10 12:55:14 tedu Exp $

BINDIR?=	/bin
LDSTATIC=	${STATIC}
CFLAGS+=-Werror-implicit-function-declaration
