#	$OpenBSD: Makefile.inc,v 1.3 1998/06/24 14:42:40 mickey Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	random.c ashrdi3.c divdi3.c strncpy.c

SRCS+=	milli.S bzero.S bcopy.S ffs.S
