#	$OpenBSD: Makefile,v 1.2 1997/05/11 16:17:55 pefo Exp $

LIB=	sa

S=${.CURDIR}/../../../..
DIR_SA= $S/lib/libsa
DIR_KERN=$S/lib/libkern

CFLAGS+=$(SACFLAGS) -D__INTERNAL_LIBSA_CREAD
CFLAGS+=-DSTART=$(START) -DHEAP_LIMIT=$(HEAP_LIMIT)
CFLAGS+=${DEBUGFLAGS} -I${.CURDIR} -I${.CURDIR}/..
CFLAGS+=-I$(S)/lib/libsa -I$(S)

SRCS=	alloc.c close.c dev.c disklabel.c dkcksum.c getfile.c ioctl.c \
	lseek.c open.c printf.c read.c ufs.c write.c devopen.c getenv.c   \
	gets.c memcmp.c memcpy.c memset.c strcat.c strcmp.c strcpy.c strlen.c \
	strncpy.c

.PATH:	${DIR_SA} ${DIR_KERN}

all: ${SALIB}

NOPROFILE=noprofile
NOPIC=nopic

.include <bsd.lib.mk>
