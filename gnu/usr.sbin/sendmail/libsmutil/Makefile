#	$OpenBSD: Makefile,v 1.3 2000/05/15 03:37:32 millert Exp $

LIB=	smutil
SRCS=	debug.c errstring.c lockfile.c safefile.c snprintf.c
ENVDEF=	-DNOT_SENDMAIL

# This is not a library that gets installed so only build the .a version
NOPROFILE=1
NOPIC=1

.include <bsd.lib.mk>
