#	$OpenBSD: Makefile,v 1.2 2000/04/02 19:48:31 millert Exp $

LIB=	smdb
SRCS=	smdb.c smdb1.c smdb2.c smndbm.c
ENVDEF+=-DNOT_SENDMAIL

# This is not a library that gets installed so only build the .a version
NOPROFILE=1
NOPIC=1

.include <bsd.lib.mk>
