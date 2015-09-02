#	$OpenBSD: Makefile,v 1.27 2015/09/02 01:52:25 yasuoka Exp $

MAN=	cdboot.8

.if ${MACHINE} == "amd64"
S=	${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

PROG=	cdboot
SRCS=	srt0.S boot.c conf.c
LD?=	ld
SIZE?=	size
LDFLAGS+=-nostdlib -Bstatic -Ttext $(LINKADDR) -N -x -nopie
LDFLAGS+=-melf_i386 -L/usr/libdata
INSTALL_STRIP=
BINMODE=644

.PATH:	${SADIR}/libsa
SRCS+=	machdep.c dev_i386.c exec_i386.c cmd_i386.c
SRCS+=	gidt.S random_i386.S biosdev.c bioscons.c gateA20.c \
	memprobe.c diskprobe.c time.c
SRCS+=	softraid.c

.PATH:	${S}/stand/boot
SRCS+=	cmd.c vars.c bootarg.c

.PATH:	${S}/lib/libsa
SRCS+=	alloc.c exit.c getchar.c putchar.c strcmp.c strlen.c \
	strncmp.c memcmp.c memcpy.c memset.c printf.c snprintf.c \
	strerror.c strncpy.c strtol.c strtoll.c ctime.c strlcpy.c
SRCS+=	close.c closeall.c dev.c disklabel.c dkcksum.c fstat.c lseek.c \
	open.c read.c stat.c cread.c readdir.c cons.c loadfile.c \
	elf32.c elf64.c
SRCS+=	ufs.c cd9660.c
SRCS+=	aes_xts.c explicit_bzero.c hmac_sha1.c pbkdf2.c rijndael.c sha1.c

.PATH:	${S}/lib/libkern/arch/i386 ${S}/lib/libkern
SRCS+=	divdi3.c moddi3.c qdivrem.c

.PATH:	${S}/lib/libz
SRCS+=	adler32.c crc32.c inflate.c inftrees.c

.depend srt0.o gidt.o: assym.h

${PROG}: $(OBJS)
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS)
	@$(SIZE) $(PROG)
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
CFLAGS+=-m32
CFLAGS+=${SACFLAGS} -D__INTERNAL_LIBSA_CREAD
CFLAGS+=-DOSREV=\"${OSREV}\" -DMACHINE=\"${MACHINE}\"
CFLAGS+=-DKERNEL=\"/${OSREV}/${MACHINE}/bsd.rd\"
CFLAGS+=-fno-pie
AFLAGS+=-m32
AFLAGS+=-fno-pie
