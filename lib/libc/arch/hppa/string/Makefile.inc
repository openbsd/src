#	$OpenBSD: Makefile.inc,v 1.2 1999/04/25 00:23:57 millert Exp $

SRCS+=	memchr.c memcmp.c memset.c index.c rindex.c \
	strcat.c strcmp.c strcpy.c strcspn.c strlcpy.c strlen.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
SRCS+=	bcmp.S bcopy.S bzero.S ffs.S memmove.S
