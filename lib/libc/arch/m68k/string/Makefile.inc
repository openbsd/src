#	$OpenBSD: Makefile.inc,v 1.6 1999/04/25 00:23:58 millert Exp $

SRCS+=	bcmp.S bcopy.S bzero.S ffs.S index.S memchr.c memcmp.S memset.S \
	rindex.S strcat.S strcmp.S strcpy.S strcspn.c strlcpy.S strlen.S \
	strncat.c strncmp.S strncpy.S strpbrk.c strsep.c \
	strspn.c strstr.c swab.S
SRCS+=	memcpy.S memmove.S strchr.S strrchr.S
LSRCS+=	bcmp.c bcopy.c bzero.c ffs.c index.c memcmp.c memset.c rindex.c \
	strcmp.c strcpy.c strlcpy.c strlen.c strncmp.c strncpy.c swab.c
