#	$OpenBSD: Makefile.inc,v 1.1.1.1 1998/06/23 18:56:53 mickey Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	random.c ashrdi3.c

# `source' files built from m4 source
SRCS+=	divU.S divI.S
SRCS+=	remU.S remI.S
SRCS+=	mulU.S mulI.S

