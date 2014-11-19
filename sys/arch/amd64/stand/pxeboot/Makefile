#	$OpenBSD: Makefile,v 1.23 2014/11/19 20:09:58 miod Exp $

MAN=	pxeboot.8

.if ${MACHINE} == "amd64"
S=	${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

PROG=	pxeboot
SRCS=	srt0.S conf.c devopen.c open.c
LD?=	ld
SIZE?=	size
LDFLAGS+=-nostdlib -Bstatic -Ttext $(LINKADDR) -N -x -nopie
LDFLAGS+=-melf_i386 -L/usr/libdata
INSTALL_STRIP=
BINMODE=644

.PATH:	${SADIR}/libsa
# i386 stuff (so, it will possibly load in the same 64k)
SRCS+=	machdep.c exec_i386.c cmd_i386.c
SRCS+=	gidt.S random_i386.S alloca.S biosdev.c bioscons.c gateA20.c \
	memprobe.c diskprobe.c time.c ## biosprobe.c
SRCS+=	pxe.c pxe_call.S pxe_net.c pxe_udp.c
SRCS+=	softraid.c

.PATH:	${S}/stand/boot
SRCS+=	boot.c cmd.c vars.c bootarg.c

.PATH:	${S}/lib/libsa
SRCS+=	alloc.c exit.c getchar.c getfile.c gets.c globals.c putchar.c strcmp.c \
	strlen.c strncmp.c memcmp.c memcpy.c memset.c printf.c snprintf.c \
	strerror.c strncpy.c strtol.c strtoll.c ctime.c strlcpy.c strlcat.c
SRCS+=	aes_xts.c explicit_bzero.c hmac_sha1.c pbkdf2.c rijndael.c sha1.c

SRCS+=	close.c closeall.c dev.c disklabel.c dkcksum.c fstat.c ioctl.c lseek.c \
	read.c stat.c write.c cread.c readdir.c cons.c loadfile.c \
	elf32.c elf64.c
SRCS+=	ether.c net.c netif.c rpc.c
SRCS+=	bootp.c bootparam.c
SRCS+=	ufs.c nfs.c tftp.c

.PATH:	${S}/lib/libkern/arch/amd64 ${S}/lib/libkern
SRCS+=	divdi3.c moddi3.c qdivrem.c udivdi3.c umoddi3.c

.PATH:	${S}/lib/libz
SRCS+=	adler32.c crc32.c inflate.c inftrees.c

.depend srt0.o gidt.o pxe_call.o: assym.h

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
CPPFLAGS+=-DBOOTMAGIC=$(BOOTMAGIC) ${DEBUGFLAGS} -DLINKADDR=${LINKADDR}
CPPFLAGS+=-DSLOW -DSMALL -DNOBYFOUR -DNO_GZIP -DDYNAMIC_CRC_TABLE
CPPFLAGS+=-DHEAP_LIMIT=${HEAP_LIMIT} -I${S}/stand/boot #-DCOMPAT_UFS
CFLAGS+=-m32
CFLAGS+=$(SACFLAGS) -D__INTERNAL_LIBSA_CREAD -fno-pie
# AFLAGS+=-Wa,-a
AFLAGS+=-m32 # -Wa,-R
AFLAGS+=-fno-pie
