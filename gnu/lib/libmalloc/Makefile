#	$Id: Makefile,v 1.2 1996/02/02 07:10:59 etheisen Exp $

CFLAGS+= -I${.CURDIR} -I/usr/include

LIB=	gnumalloc
SRCS+=	combined.c
#SRCS+=	malloc.c free.c realloc.c
SRCS+=	cfree.c calloc.c morecore.c
SRCS+=	memalign.c valloc.c mcheck.c mtrace.c mstats.c vm-limit.c
SRCS+=	ralloc.c
NOMAN=	noman

.include <bsd.lib.mk>
