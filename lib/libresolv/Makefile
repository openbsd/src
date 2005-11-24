#	$OpenBSD: Makefile,v 1.5 2005/11/24 20:49:20 deraadt Exp $

# libresolv no longer needs a special version of the resolver routines.
# The resolver is always in libc.
# However, an empty library must exist in case something tries to use it.

NOPIC=
LIB=resolv
WANTLINT=

SRCS=nothing.c

.include <bsd.lib.mk>
