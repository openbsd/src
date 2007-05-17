#       $OpenBSD: Makefile.inc,v 1.7 2007/05/17 04:32:00 drahn Exp $

SRCS+=  bcmp.S bcopy.S bzero.S ffs.S index.S memchr.c memcmp.S memset.S \
	rindex.c strcat.c strcmp.c strcpy.c strcspn.c strlcpy.c strlen.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
SRCS+=	memcpy.S memmove.S

LSRCS+=	bcmp.c bcopy.c bzero.c ffs.c index.c memcmp.c memset.c memcpy.c \
	memmove.c
