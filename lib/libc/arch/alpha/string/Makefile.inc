#	$OpenBSD: Makefile.inc,v 1.4 1996/11/13 22:05:06 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.4 1996/07/15 03:13:18 cgd Exp $

SRCS+=	bcmp.c bcopy.S bzero.S ffs.S index.c memchr.c memcmp.c memset.c \
	rindex.c strcat.c strcmp.c strcpy.c strcspn.c strlen.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c
SRCS+=	memcpy.S memmove.S
LSRCS+=	bcopy.c bzero.c
