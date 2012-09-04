#       $OpenBSD: Makefile.inc,v 1.8 2012/09/04 03:10:42 okan Exp $

SRCS+=  bcmp.S bcopy.S bzero.S ffs.S index.S memchr.c memcmp.S memset.S \
	rindex.c strcat.c strcmp.c strcpy.c strcspn.c strlcpy.c strlen.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
SRCS+=	memcpy.S memmove.S
