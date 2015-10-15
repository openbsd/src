#	$OpenBSD: Makefile,v 1.15 2015/10/15 19:29:32 miod Exp $
#	$NetBSD: Makefile,v 1.12 2002/02/24 01:04:25 matt Exp $

S=	${.CURDIR}/../../../../

PROG=	xxboot
WARNS?=	1

SRCS=	start.S bootxx.c romread.S str.S urem.s udiv.s

INSTALL_STRIP=
CPPFLAGS+=-D_STANDALONE -DLIBSA_NO_FD_CHECKING -DLIBSA_NO_RAW_ACCESS \
	-DLIBSA_NO_TWIDDLE -DLIBSA_SINGLE_DEVICE=rom \
	-DLIBSA_NO_COMPAT_UFS \
	-DLIBSA_NO_FS_SYMLINK -DLIBSA_NO_FS_CLOSE \
	-DLIBSA_NO_FS_WRITE -DLIBSA_NO_FS_SEEK \
	-DNEED_UFS
AFLAGS+=-D_LOCORE
BINDIR=	/usr/mdec
NOMAN=	# defined
NOPIE=

CFLAGS=	-Os

SAREL=
SA_AS=	library
.include "${S}/lib/libsa/Makefile.inc"
LIBSA=	${SALIB}

STRIP?=strip
OBJCOPY?=objcopy

.if ${MACHINE} == "vax"
.PHONY: machine-links
beforedepend: machine-links
machine-links:
	@[ -h machine ] || ln -s ${S}/arch/${MACHINE}/include machine
	@[ -h ${MACHINE_ARCH} ] || ln -s ${S}/arch/${MACHINE_ARCH}/include ${MACHINE_ARCH}
.NOPATH: machine ${MACHINE_ARCH}
CLEANFILES+= machine ${MACHINE_ARCH} ${PROG}.out assym.h
.endif

start.o: assym.h

assym.h: $S/kern/genassym.sh Makefile ${.CURDIR}/genassym.cf
	sh $S/kern/genassym.sh ${CC} ${CFLAGS} ${CPPFLAGS} \
	    < ${.CURDIR}/genassym.cf | sort -u > assym.h

${PROG}: ${OBJS} ${LIBSA}
	${LD} -N -Ttext 100000 -o ${PROG}.out ${OBJS} ${LIBSA}
	${STRIP} ${PROG}.out
	/usr/bin/size ${PROG}.out
	${OBJCOPY} -O binary ${PROG}.out ${PROG}

clean::
	rm -f a.out [Ee]rrs mklog *.core ${PROG} ${OBJS} ${LOBJS} \
	    ${CLEANFILES} 

.include <bsd.prog.mk>
