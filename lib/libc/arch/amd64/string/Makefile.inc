#	$OpenBSD: Makefile.inc,v 1.3 2012/01/17 02:34:18 guenther Exp $

SRCS+=  bcmp.c ffs.S index.c memchr.c memcmp.c bcopy.c bzero.c \
        rindex.c strcat.c strcmp.c strcpy.c strcspn.c strlen.c \
        strncat.c strncmp.c strncpy.c strpbrk.c strsep.c \
        strspn.c strstr.c swab.c memset.c strlcpy.c strlcat.c

LSRCS+=	ffs.c
