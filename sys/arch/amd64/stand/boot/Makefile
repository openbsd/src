#	$OpenBSD: Makefile,v 1.2 2004/05/17 22:49:46 drahn Exp $

.include "${.CURDIR}/../Makefile.inc"

MAN=	boot.8
MLINKS=	boot.8 boot.conf.8

.if ${MACHINE} == "amd64"
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

PROG=	boot
SRCS=	srt0.S conf.c
LD?=	ld
SIZE?=	size
LDFLAGS+=-melf_i386 -nostdlib -Bstatic -Ttext $(LINKADDR) -N -x -noinhibit-exec
LDFLAGS+=-L/usr/libdata 

.PATH:	${SADIR}/libsa
# i386 stuff (so, it will possibly load in the same 64k)
SRCS+=	machdep.c dev_i386.c exec_i386.c cmd_i386.c
SRCS+=	gidt.S alloca.S biosdev.c bioscons.c gateA20.c \
	memprobe.c diskprobe.c time.c biosprobe.c

.PATH:	${S}/stand/boot
SRCS+=	boot.c cmd.c vars.c bootarg.c

.PATH:	${S}/lib/libsa
.PATH:	${S}/lib/libkern	# for strl*
# stand routines
SRCS+=	alloc.c exit.c getfile.c gets.c globals.c strcmp.c strlen.c \
	strncmp.c memcmp.c memcpy.c memset.c printf.c snprintf.c \
	strerror.c strncpy.c strtol.c ctime.c strlcpy.c strlcat.c
# io routines
SRCS+=	close.c closeall.c dev.c disklabel.c dkcksum.c fstat.c ioctl.c lseek.c \
	open.c read.c stat.c write.c cread.c readdir.c cons.c loadfile.c
# boot filesystems
SRCS+=	ufs.c

.PATH:	${S}/lib/libz
SRCS+=	adler32.c crc32.c inflate.c inftrees.c

boot.bin: boot
	objcopy -v -O binary ${PROG} boot.bin

${PROG}: $(OBJS)
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS)
	#@$(SIZE) ${PROG}

.else
NOPROG=
.endif

.include <bsd.prog.mk>

CPPFLAGS+=-DBOOTMAGIC=$(BOOTMAGIC) ${DEBUGFLAGS} -DLINKADDR=${LINKADDR}
CPPFLAGS+=-DSLOW -DSMALL -DNOBYFOUR -DNO_GZIP -DDYNAMIC_CRC_TABLE
CPPFLAGS+=-DHEAP_LIMIT=${HEAP_LIMIT} -I${S}/stand/boot #-DCOMPAT_UFS
CFLAGS+=-m32 $(SACFLAGS) -D__INTERNAL_LIBSA_CREAD
AFLAGS+=-m32 # -Wa,-R
# AFLAGS+=-Wa,-a
