#	$OpenBSD: Makefile,v 1.20 2013/12/28 02:51:07 deraadt Exp $

MAN=	cdboot.8

.if ${MACHINE} == "i386"
S=	${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

PROG=	cdboot
SRCS=	srt0.S boot.c conf.c
LD?=	ld
SIZE?=	size
LDFLAGS+=-nostdlib -Bstatic -Ttext $(LINKADDR) -N -x -nopie
INSTALL_STRIP=

CLEANFILES+=	crt0.o ${PROG}.whole

.PATH:	${SADIR}/libsa
SRCS+=	alloca.S debug_i386.S gidt.S random_i386.S
SRCS+=	cmd_i386.c dev_i386.c exec_i386.c gateA20.c machdep.c
SRCS+=	apmprobe.c bioscons.c biosdev.c debug.c diskprobe.c memprobe.c \
	pciprobe.c ps2probe.c time.c
SRCS+=	softraid.c

.PATH:	${S}/stand/boot
SRCS+=	bootarg.c cmd.c vars.c

.PATH:	${S}/lib/libsa
SRCS+=	alloc.c ctime.c exit.c memcmp.c memcpy.c memset.c printf.c snprintf.c \
	strcmp.c strerror.c strlen.c strncmp.c strncpy.c strtol.c strtoll.c
SRCS+=	close.c closeall.c cons.c cread.c dev.c disklabel.c dkcksum.c fstat.c \
	lseek.c open.c read.c readdir.c stat.c
SRCS+=	elf32.c elf64.c loadfile.c
SRCS+=	cd9660.c ufs.c
SRCS+=	aes_xts.c explicit_bzero.c hmac_sha1.c pbkdf2.c rijndael.c sha1.c

.PATH:	${S}/lib/libkern/arch/i386
SRCS+=	moddi3.c divdi3.c qdivrem.c

.PATH:	${S}/lib/libkern
SRCS+=	strlcpy.c

.PATH:	${S}/lib/libz
SRCS+=	adler32.c crc32.c inflate.c inftrees.c

.depend srt0.o gidt.o: assym.h

${PROG}: $(OBJS)
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS)
	@$(SIZE) ${PROG}
	cp ${PROG} ${PROG}.whole
	@if [ -x ${.OBJDIR}/${PROG} ]; then \
		objcopy -O binary ${PROG} ${.OBJDIR}/.tmp;\
		mv -f ${.OBJDIR}/.tmp ${.OBJDIR}/${PROG}; \
		ls -l ${.OBJDIR}/${PROG}; \
	fi

.else
NOPROG=
.endif

.include <bsd.prog.mk>

CPPFLAGS+=-DSOFTRAID
CPPFLAGS+=-DBOOTMAGIC=$(BOOTMAGIC) ${DEBUGFLAGS}
CPPFLAGS+=-DLINKADDR=${LINKADDR}
CPPFLAGS+=-DSLOW -DSMALL -DNOBYFOUR -DNO_GZIP -DDYNAMIC_CRC_TABLE
CPPFLAGS+=-I${S}/stand/boot
CFLAGS+=${SACFLAGS} -D__INTERNAL_LIBSA_CREAD
CFLAGS+=-DOSREV=\"${OSREV}\" -DMACHINE=\"${MACHINE}\"
CFLAGS+=-DKERNEL=\"/${OSREV}/${MACHINE}/bsd.rd\"
CFLAGS+=-fno-pie
#AFLAGS+=-Wa,-R
# AFLAGS+=-Wa,-a
AFLAGS+=-fno-pie
