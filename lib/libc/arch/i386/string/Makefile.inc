#	$OpenBSD: Makefile.inc,v 1.4 1999/04/25 00:23:58 millert Exp $

SRCS+=	bcmp.S bcopy.S bzero.S ffs.S index.S memchr.S memcmp.S memset.S \
	rindex.S strcat.S strcmp.S strcpy.S strcspn.c strlcpy.c strlen.S \
	strncat.c strncmp.S strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.S
SRCS+=	memcpy.S memmove.S strchr.S strrchr.S
LSRCS+=	bcmp.c bcopy.c bzero.c ffs.c index.c memchr.c memcmp.c memset.c \
	rindex.c strcat.c strcmp.c strcpy.c strlen.c strncmp.c swab.c
