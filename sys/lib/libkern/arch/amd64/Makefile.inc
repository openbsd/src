#	$NetBSD: Makefile.inc,v 1.2 2002/11/25 00:55:22 fvdl Exp $

SRCS+=	imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	byte_swap_2.S byte_swap_4.S \
	bcmp.S bcopy.S bzero.S ffs.S \
	memchr.S memcmp.S memcpy.S memmove.S memset.S \
	ovbcopy.S \
	strchr.S strcmp.S \
	strlcpy.c strlcat.c strlen.S \
	strncasecmp.c strncmp.c strncpy.c strrchr.S \
	scanc.S skpc.S random.c
#	 bswap64.c strcasecmp.c strncasecmp.c \ strtoul.c \

CFLAGS+=-mcmodel=kernel
