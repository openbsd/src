#	$OpenBSD: Makefile,v 1.11 2018/06/23 22:13:50 kettenis Exp $

NOMAN=		#

.if ${MACHINE} == "armv7"

PROG=		BOOTARM.EFI
OBJFMT=		binary
INSTALL_STRIP=
BINDIR=		/usr/mdec
SRCS=		start.S self_reloc.c efiboot.c conf.c exec.c efidev.c efipxe.c
SRCS+=		fdt.c

S=		${.CURDIR}/../../../..
EFIDIR=		${S}/stand/efi

OBJCOPY?=	objcopy
OBJDUMP?=	objdump

LDFLAGS+=	-nostdlib -T ${.CURDIR}/ldscript.arm -Bsymbolic -shared
LDFLAGS+=	-z notext -z norelro

.PATH:	${S}/stand/boot
SRCS+=	boot.c cmd.c vars.c

.PATH:	${S}/lib/libsa
SRCS+=	alloc.c ctime.c exit.c getchar.c memcmp.c memcpy.c memmove.c memset.c \
	printf.c putchar.c snprintf.c strchr.c strcmp.c strerror.c strncmp.c \
	strncpy.c strtol.c
SRCS+=	close.c closeall.c cons.c cread.c dev.c disklabel.c dkcksum.c fstat.c \
	lseek.c open.c read.c readdir.c stat.c
SRCS+=	loadfile.c
SRCS+=	ufs.c
SRCS+=	arp.c ether.c globals.c in_cksum.c net.c netif.c netudp.c tftp.c

.PATH:	${S}/lib/libkern/arch/arm ${S}/lib/libkern
SRCS+=	divsi3.S divdi3.c moddi3.c qdivrem.c strlcpy.c strlen.c
SRCS+=	__aeabi_ldivmod.S

.PATH:	${S}/lib/libz
SRCS+=	adler32.c crc32.c inflate.c inftrees.c

CPPFLAGS+=	-nostdinc
CPPFLAGS+=	-I${S} -I. -I${.CURDIR}
CPPFLAGS+=	-I${EFIDIR}/include -I${EFIDIR}/include/arm
CPPFLAGS+=	-D_STANDALONE
CPPFLAGS+=	-DSMALL -DSLOW -DNOBYFOUR -D__INTERNAL_LIBSA_CREAD
CPPFLAGS+=	-DNEEDS_HEAP_H
COPTS+=		-ffreestanding -fno-stack-protector
COPTS+=		-fshort-wchar -fPIC -fno-builtin
COPTS+=		-Wall -Werror
COPTS+=		-mfloat-abi=soft

PROG.elf=	${PROG:S/.EFI/.elf/}
CLEANFILES+=	${PROG.elf} ${PROG.elf}.tmp

${PROG}: ${PROG.elf}
	${OBJCOPY} -j .peheader -j .text -j .sdata -j .data \
		-j .dynamic -j .dynsym -j .dynstr -j .rel -j .rel.dyn \
		-j .rela -j .rela.dyn -j .reloc \
		--output-target=${OBJFMT} ${PROG.elf} ${.TARGET}

.include <bsd.prog.mk>

${PROG.elf}: ${OBJS}
	${LD} ${LDFLAGS} -o ${.TARGET}.tmp ${OBJS} ${LDADD}
	@if ${OBJDUMP} -t ${.TARGET}.tmp | grep 'UND'; then	\
		(echo Undefined symbols; false);		\
	fi
	mv ${.TARGET}.tmp ${.TARGET}

.if !make(clean) && !make(cleandir) && !make(includes) && !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../../${MACHINE}/include machine)
	@([ -h arm ] || ln -s ${.CURDIR}/../../../arm/include arm)
.NOPATH: machine arm
CLEANFILES+=	machine arm
.endif

.else
NOPROG=yes
.include <bsd.prog.mk>
.endif
