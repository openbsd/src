#	$OpenBSD: Makefile,v 1.25 2013/12/28 02:51:06 deraadt Exp $

COPTS?=
MAN?=	boot.8
MLINKS?=boot.8 boot.conf.5

.if !empty(COPTS:M-DFDBOOT)
SOFTRAID?=no
.else
SOFTRAID?=yes
.endif

.if ${MACHINE} == "amd64"
S=	${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

PROG?=	boot
SRCS=	srt0.S conf.c
LD?=	ld
SIZE?=	size
LDFLAGS+=-nostdlib -Bstatic -Ttext $(LINKADDR) -N -x -nopie
LDFLAGS+=-melf_i386 -L/usr/libdata 

.PATH:	${SADIR}/libsa
SRCS+=	alloca.S gidt.S random_i386.S
SRCS+=	cmd_i386.c dev_i386.c exec_i386.c gateA20.c machdep.c
SRCS+=	bioscons.c biosdev.c diskprobe.c memprobe.c time.c
.if ${SOFTRAID:L} == "yes"
SRCS+=	softraid.c
.endif

.PATH:	${S}/stand/boot
SRCS+=	boot.c bootarg.c cmd.c vars.c

.PATH:	${S}/lib/libsa
SRCS+=	alloc.c ctime.c exit.c getfile.c gets.c globals.c memcmp.c memcpy.c \
	memset.c printf.c snprintf.c strcmp.c strerror.c strlcat.c strlcpy.c \
	strlen.c strncmp.c strncpy.c strtol.c strtoll.c
SRCS+=	close.c closeall.c cons.c cread.c dev.c disklabel.c dkcksum.c fstat.c \
	ioctl.c lseek.c open.c read.c readdir.c stat.c write.c
SRCS+=	elf32.c elf64.c loadfile.c
SRCS+=	ufs.c
.if ${SOFTRAID:L} == "yes"
SRCS+=	aes_xts.c explicit_bzero.c hmac_sha1.c pbkdf2.c rijndael.c sha1.c
.endif

.PATH:	${S}/lib/libkern/arch/amd64 ${S}/lib/libkern
SRCS+=	divdi3.c moddi3.c qdivrem.c

.PATH:	${S}/lib/libz
SRCS+=	adler32.c crc32.c inflate.c inftrees.c

boot.bin: boot
	objcopy -v -O binary ${PROG} boot.bin

CLEANFILES+= ${PROG}.new

.depend srt0.o gidt.o: assym.h

${PROG}: $(OBJS)
	$(LD) $(LDFLAGS) -o ${PROG}.new $(OBJS)
	@perl ${SADIR}/boot/check-boot.pl ${PROG}.new
	@mv ${PROG}.new ${PROG}
	@$(SIZE) ${PROG}

.else
NOPROG=
.endif

.include <bsd.prog.mk>

.if ${SOFTRAID:L} == "yes"
CPPFLAGS+=-DSOFTRAID
.endif
CPPFLAGS+=-DBOOTMAGIC=$(BOOTMAGIC) ${DEBUGFLAGS} -DLINKADDR=${LINKADDR}
CPPFLAGS+=-DSLOW -DSMALL -DNOBYFOUR -DNO_GZIP -DDYNAMIC_CRC_TABLE
CPPFLAGS+=-DHEAP_LIMIT=${HEAP_LIMIT} -I${S}/stand/boot #-DCOMPAT_UFS
CFLAGS+=-m32 $(SACFLAGS) -D__INTERNAL_LIBSA_CREAD -fno-pie
AFLAGS+=-m32 # -Wa,-R
# AFLAGS+=-Wa,-a
AFLAGS+=-fno-pie
