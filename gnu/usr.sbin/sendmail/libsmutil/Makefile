#	$OpenBSD: Makefile,v 1.4 2001/09/11 19:02:49 millert Exp $

LIB=	smutil
SRCS=	debug.c err.c lockfile.c safefile.c snprintf.c cf.c
ENVDEF=	-DNOT_SENDMAIL

# This is not a library that gets installed so only build the .a version
NOPROFILE=1
NOPIC=1

.include <bsd.lib.mk>
