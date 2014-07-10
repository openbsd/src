#	$OpenBSD: Makefile.inc,v 1.5 2014/07/10 13:31:57 florian Exp $

BINDIR?=	/sbin
LDSTATIC=	${STATIC}
CFLAGS+=	-Werror-implicit-function-declaration
