#	from: @(#)Makefile	5.1 (Berkeley) 6/5/90
#	$Id: Makefile,v 1.3 2000/01/01 10:03:01 matthieu Exp $

# libresolv no longer needs a special version of the resolver routines.
# The resolver is always in libc.
# However, an empty library must exist in case something tries to use it.

NOPIC=
LIB=resolv

SRCS=nothing.c

.include <bsd.lib.mk>
