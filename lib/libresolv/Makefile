#	$OpenBSD: Makefile,v 1.4 2000/03/02 00:29:51 todd Exp $

# libresolv no longer needs a special version of the resolver routines.
# The resolver is always in libc.
# However, an empty library must exist in case something tries to use it.

NOPIC=
LIB=resolv

SRCS=nothing.c

.include <bsd.lib.mk>
