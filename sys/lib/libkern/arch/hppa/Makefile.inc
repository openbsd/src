#	$OpenBSD: Makefile.inc,v 1.6 1999/04/20 21:08:51 mickey Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	random.c ashrdi3.c divdi3.c strncpy.c strncmp.c strcmp.c memchr.c \
	memset.c strlen.c strcpy.c strcat.c skpc.c strncasecmp.c scanc.c

SRCS+=	milli.S bzero.S bcopy.S bcmp.S ffs.S
