#	$NetBSD: Makefile.inc,v 1.3 1995/08/14 04:09:23 cgd Exp $

SRCS+=	bcmp.c bcopy.S bzero.S ffs.c index.c memchr.c memcmp.c memset.c \
	rindex.c strcat.c strcmp.c strcpy.c strcspn.c strlen.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
SRCS+=	memcpy.S memmove.S
LSRCS+=	bcopy.c bzero.c
