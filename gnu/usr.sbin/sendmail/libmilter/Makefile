#	$OpenBSD: Makefile,v 1.2 2000/04/02 19:48:31 millert Exp $

LIB=	libmilter
SRCS=	main.c engine.c listener.c handler.c comm.c smfi.c signal.c sm_gethost.c

# This is not a library that gets installed so only build the .a version
# In the future we may wish to install it to ease the use of external filters.
NOPROFILE=1
NOPIC=1

.include <bsd.lib.mk>
