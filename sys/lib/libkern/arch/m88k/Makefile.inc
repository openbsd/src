#	$OpenBSD: Makefile.inc,v 1.9 2006/11/17 22:32:38 miod Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	memchr.c memcmp.c memset.c bcmp.c random.c \
	strcmp.c strlcat.c strlcpy.c strlen.c strncmp.c \
	strncpy.c scanc.c skpc.c strncasecmp.c ffs.c

SRCS+=	bcopy.S bzero.S memcpy.S memmove.S ovbcopy.S
