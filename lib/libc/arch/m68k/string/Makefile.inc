#	$OpenBSD: Makefile.inc,v 1.5 1999/04/25 00:10:06 millert Exp $

SRCS+=	bcmp.S bcopy.S bzero.S ffs.S index.S memchr.c memcmp.S memset.S \
	rindex.S strcat.S strcmp.S strcpy.S strcspn.c strlen.S strlcpy.S \
	strncat.c strncmp.S strncpy.S strpbrk.c strsep.c \
	strspn.c strstr.c swab.S
SRCS+=	memcpy.S memmove.S strchr.S strrchr.S
LSRCS+=	bcmp.c bcopy.c bzero.c ffs.c index.c memcmp.c memset.c rindex.c \
	strcmp.c strcpy.c strlen.c strlcpy.c strncmp.c strncpy.c swab.c
