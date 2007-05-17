# $OpenBSD: Makefile.inc,v 1.5 2007/05/17 04:32:00 drahn Exp $
# $NetBSD: Makefile.inc,v 1.5 2002/11/23 14:26:04 chris Exp $

SRCS+=  memcpy.S _memcpy.S bcopy.S memmove.S memset.S bzero.S ffs.S strcmp.S
SRCS+=	strncmp.S memcmp.S
SRCS+=	bcmp.c index.c memchr.c \
	rindex.c strcat.c strcpy.c strcspn.c strlen.c \
	strncat.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c strlcpy.c strlcat.c

LSRCS+=	memcpy.c bcopy.c memmove.c memset.c bzero.c ffs.c strcmp.c strncmp.c \
	memcmp.c
