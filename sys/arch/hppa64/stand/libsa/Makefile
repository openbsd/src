#	$OpenBSD: Makefile,v 1.1 2005/04/01 10:40:48 mickey Exp $

.include "${.CURDIR}/../Makefile.inc"

LIB=	sa

S=${.CURDIR}/../../../..
SADIR=${.CURDIR}/..
DIR_SA=	$S/lib/libsa

NOPROFILE=noprofile
NOPIC=nopic

#AS=cat ;
#AS+=	-R
#AS+=	-a

# hppa64 stuff
SRCS=	machdep.c pdc.c itecons.c dev_hppa64.c time.c \
	ct.c dk.c lf.c lif.c cmd_hppa64.c

# stand routines
SRCS+=	alloc.c exit.c getfile.c gets.c globals.c \
	printf.c strerror.c strtol.c strchr.c ctime.c loadfile.c snprintf.c

# io routines
SRCS+=	close.c closeall.c dev.c disklabel.c dkcksum.c fstat.c ioctl.c lseek.c \
	open.c read.c stat.c write.c cread.c readdir.c cons.c

# boot filesystems
SRCS+=	ufs.c cd9660.c

.PATH:  ${DIR_SA}

all: ${SALIB}

install:

.include <bsd.lib.mk>

CPPFLAGS+=-DHEAP_LIMIT=${HEAP_LIMIT} ${DEBUGFLAGS} -DNO_NET
CPPFLAGS+=-I${S}/stand/boot
CFLAGS+=${SACFLAGS} -D__INTERNAL_LIBSA_CREAD -DCOMPAT_UFS
