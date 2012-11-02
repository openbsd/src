#	$OpenBSD: Makefile,v 1.7 2012/11/02 21:54:37 miod Exp $

LIB=	smutil
SRCS=	debug.c err.c lockfile.c safefile.c snprintf.c cf.c
ENVDEF=	-DNOT_SENDMAIL

# This is not a library that gets installed so only build the .a version
NOPROFILE=1
NOPIC=1

.include <bsd.lib.mk>
