#	$OpenBSD: Makefile,v 1.3 2000/05/15 03:37:31 millert Exp $

LIB=	smdb
SRCS=	smdb.c smdb1.c smdb2.c smndbm.c
ENVDEF=	-DNOT_SENDMAIL

# This is not a library that gets installed so only build the .a version
NOPROFILE=1
NOPIC=1

.include <bsd.lib.mk>
