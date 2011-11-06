#	$OpenBSD: Makefile,v 1.36 2011/11/06 13:39:43 jsing Exp $

.include "${.CURDIR}/../Makefile.inc"

MAN=	boot.8
MLINKS=	boot.8 boot.conf.5

.if ${MACHINE} == "i386"
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

PROG=	boot
LD?=	ld
SIZE?=	size
LDFLAGS+=-nostdlib -Bstatic

.if defined(DEBUGFLAGS) && !empty(DEBUGFLAGS:M-D_TEST)
CLEANFILES+=	srt0.o
SRCS=	crt0.c
.else
LDFLAGS+=-Ttext $(LINKADDR) -Z -x -noinhibit-exec
CLEANFILES+=	crt0.o
SRCS=	srt0.S
.endif

.PATH:	${S}/stand/boot
SRCS+=	boot.c cmd.c vars.c bootarg.c conf.c

.PATH:	${SADIR}/libsa
SRCS+=	alloca.S debug_i386.S gidt.S
SRCS+=	apmprobe.c bioscons.c biosdev.c cmd_i386.c debug.c dev_i386.c \
	diskprobe.c exec_i386.c gateA20.c machdep.c memprobe.c pciprobe.c \
	ps2probe.c time.c

.PATH:	${S}/lib/libsa
SRCS+=	alloc.c cread.c ctime.c exit.c memcmp.c memcpy.c memset.c printf.c \
	snprintf.c strcmp.c strerror.c strlen.c strncmp.c strncpy.c strtol.c \
	strtoll.c
SRCS+=	close.c closeall.c cons.c dev.c disklabel.c dkcksum.c fstat.c lseek.c \
	open.c read.c readdir.c stat.c
SRCS+=	elf32.c elf64.c loadfile.c
SRCS+=	ufs.c

.PATH:	${S}/lib/libkern
SRCS+=	strlcpy.c

.PATH:	${S}/lib/libkern/arch/i386
SRCS+=	moddi3.c divdi3.c qdivrem.c

.PATH:	${S}/lib/libz
SRCS+=	adler32.c crc32.c inflate.c inftrees.c

boot.bin: boot
	objcopy -v -O binary ${PROG} boot.bin

${PROG}: $(OBJS)
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS)
	@$(SIZE) ${PROG}

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
