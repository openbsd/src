#	$OpenBSD: Makefile,v 1.1 2010/02/14 22:39:33 miod Exp $

.include "${.CURDIR}/../Makefile.inc"

LIB=	sa

.PATH:	${.CURDIR}/../../../../lib/libsa

CLEANFILES += machine mips64

CFLAGS+= ${CEXTRAFLAGS} ${SAABI} -nostdinc -mno-abicalls -D_NO_ABICALLS \
	-I${.CURDIR} -I${.CURDIR}/../include -I${.CURDIR}/../.. \
	-I${.CURDIR}/../../.. -I${.CURDIR}/../../../.. \
	-I${.CURDIR}/../../../../lib/libsa \
	-I${.OBJDIR}

# stand routines
SRCS=	alloc.c cons.c ctime.c exit.c exec.c getfile.c gets.c globals.c \
	memcmp.c memcpy.c memset.c printf.c \
	snprintf.c strchr.c strcmp.c strerror.c strncmp.c strncpy.c strtol.c

# io routines
SRCS+=	close.c closeall.c dev.c disklabel.c dkcksum.c fstat.c ioctl.c \
	lseek.c open.c read.c readdir.c stat.c write.c
#SRCS+=	nullfs.c

#SRCS+=	cread.c
#CPPFLAGS+= -D__INTERNAL_LIBSA_CREAD

# boot filesystems
SRCS+=	ufs.c cd9660.c

CFLAGS+=-DNO_NET

SRCS+=	loadfile.c

${OBJS}:	${.CURDIR}/../Makefile.inc

NOPROFILE=
NOPIC=
WANTLINT=	no

.if !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../include machine)
	@([ -h mips64 ] || ln -s ${.CURDIR}/../../../mips64/include mips64)
CLEANFILES+=	machine mips64
.endif

install:

.include <bsd.lib.mk>
