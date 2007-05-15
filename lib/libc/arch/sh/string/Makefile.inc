#	$OpenBSD: Makefile.inc,v 1.2 2007/05/15 18:42:31 otto Exp $
#	$NetBSD: Makefile.inc,v 1.3 2002/11/20 14:23:54 itohy Exp $

SRCS+=	bcmp.c bcopy.S bzero.S ffs.S index.c memchr.c memcmp.c memset.S \
	rindex.c strcat.c strcmp.c strcpy.c strcspn.c strlen.c \
	strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
	strspn.c strstr.c swab.c strlcpy.c strlcat.c
SRCS+=	memcpy.S memmove.S

LSRCS+=	bcopy.c bzero.c ffs.c memset.c memcpy.c memmove.c
