#	$OpenBSD: Makefile.inc,v 1.4 1999/04/25 00:23:59 millert Exp $

SRCS+=	bcmp.c bcopy.c bzero.S ffs.S index.c memchr.c memcmp.c memset.c \
	rindex.c strcat.c strcmp.c strcpy.c strcspn.c strlcpy.c strlen.S \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
LSRCS+=	bzero.c ffs.c strlen.c
