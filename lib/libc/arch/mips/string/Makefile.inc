#	$OpenBSD: Makefile.inc,v 1.4 1999/04/25 00:23:58 millert Exp $

SRCS+=	bcmp.S bcopy.S bzero.S  ffs.S index.S memchr.c memcmp.c memset.c \
	rindex.S strcat.c strcmp.S strcpy.c strcspn.c strlcpy.c strlen.S \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
LSRCS+=	bcmp.c bcopy.c bzero.c ffs.c index.c rindex.c strcmp.c strlen.c
