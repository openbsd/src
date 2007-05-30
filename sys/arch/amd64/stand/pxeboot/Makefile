#	$OpenBSD: Makefile,v 1.7 2007/05/30 01:25:43 tom Exp $

.include "${.CURDIR}/../Makefile.inc"

MAN=	pxeboot.8

.if ${MACHINE} == "amd64"
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

PROG=	pxeboot
SRCS=	srt0.S conf.c devopen.c net.c open.c
LD?=	ld
SIZE?=	size
LDFLAGS+=-melf_i386 -nostdlib -Bstatic -Ttext $(LINKADDR) -N -x -noinhibit-exec
LDFLAGS+=-L/usr/libdata 
INSTALL_STRIP=

.PATH:	${SADIR}/libsa
# i386 stuff (so, it will possibly load in the same 64k)
SRCS+=	machdep.c exec_i386.c cmd_i386.c
SRCS+=	gidt.S alloca.S biosdev.c bioscons.c gateA20.c \
	memprobe.c diskprobe.c time.c ## biosprobe.c
SRCS+=	pxe.c pxe_call.S pxe_net.c

.PATH:	${S}/stand/boot
SRCS+=	boot.c cmd.c vars.c bootarg.c

.PATH:	${S}/lib/libsa
.PATH:	${S}/lib/libkern	# for strl* and gcc support
# stand routines
SRCS+=	alloc.c exit.c getfile.c gets.c globals.c strcmp.c strlen.c \
	strncmp.c memcmp.c memcpy.c memset.c printf.c snprintf.c \
	strerror.c strncpy.c strtol.c strtoll.c ctime.c strlcpy.c strlcat.c
# io routines
SRCS+=	close.c closeall.c dev.c disklabel.c dkcksum.c fstat.c ioctl.c lseek.c \
	read.c stat.c write.c cread.c readdir.c cons.c loadfile.c \
	elf32.c elf64.c
# network routines
SRCS+=	ether.c netif.c rpc.c
# network info services
SRCS+=	bootp.c bootparam.c
# boot filesystems
SRCS+=	ufs.c nfs.c tftp.c
# gcc support
SRCS+=	divdi3.c moddi3.c qdivrem.c

.PATH:	${S}/lib/libz
SRCS+=	adler32.c crc32.c inflate.c inftrees.c

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

CPPFLAGS+=-DBOOTMAGIC=$(BOOTMAGIC) ${DEBUGFLAGS} -DLINKADDR=${LINKADDR}
CPPFLAGS+=-DSLOW -DSMALL -DNOBYFOUR -DNO_GZIP -DDYNAMIC_CRC_TABLE
CPPFLAGS+=-DHEAP_LIMIT=${HEAP_LIMIT} -I${S}/stand/boot #-DCOMPAT_UFS
CFLAGS+=-m32 $(SACFLAGS) -D__INTERNAL_LIBSA_CREAD
AFLAGS+=-m32 # -Wa,-R
# AFLAGS+=-Wa,-a
