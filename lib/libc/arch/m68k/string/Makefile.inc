#	$OpenBSD: Makefile.inc,v 1.7 2012/09/04 03:10:42 okan Exp $

SRCS+=	bcmp.S bcopy.S bzero.S ffs.S index.S memchr.c memcmp.S memset.S \
	rindex.S strcat.S strcmp.S strcpy.S strcspn.c strlcpy.S strlen.S \
	strncat.c strncmp.S strncpy.S strpbrk.c strsep.c \
	strspn.c strstr.c swab.S
SRCS+=	memcpy.S memmove.S strchr.S strrchr.S
