#	$OpenBSD: Makefile.inc,v 1.1 1998/08/28 20:59:41 mickey Exp $

SRCS+=	memchr.c memcmp.c memset.c index.c rindex.c \
	strcat.c strcmp.c strcpy.c strcspn.c strlen.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
SRCS+=	bcmp.S bcopy.S bzero.S ffs.S memmove.S
