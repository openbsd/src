#       $OpenBSD: Makefile.inc,v 1.6 2007/05/15 18:42:31 otto Exp $

SRCS+=  bcmp.S bcopy.S bzero.S ffs.S index.S memchr.c memcmp.S memset.S \
	rindex.c strcat.c strcmp.c strcpy.c strcspn.c strlcpy.c strlen.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
SRCS+=	memcpy.S memmove.S

LSRCS+=	bcmp.s bcopy.c bzero.c ffs.c index.c memcmp.c memset.c memcpy.c \
	memmove.c
