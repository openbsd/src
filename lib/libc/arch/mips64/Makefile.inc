#	$OpenBSD: Makefile.inc,v 1.4 2023/01/11 17:00:20 deraadt Exp $

LDADD+= -Wl,--no-execute-only

CERROR=	cerror.S
