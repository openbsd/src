#	$OpenBSD: Makefile.inc,v 1.5 1998/07/20 18:17:15 mickey Exp $

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	random.c ashrdi3.c divdi3.c strncpy.c strncmp.c strcmp.c memchr.c \
	strlen.c strcpy.c strcat.c skpc.c strncasecmp.c scanc.c

SRCS+=	milli.S bzero.S bcopy.S bcmp.S ffs.S
