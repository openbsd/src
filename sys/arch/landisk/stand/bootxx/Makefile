#	$OpenBSD: Makefile,v 1.3 2006/10/11 17:19:06 drahn Exp $

MAN=	bootxx.8

.if ${MACHINE} == "landisk"
PROG=	bootxx
SRCS=	pbr.S bootxx.S boot1.c
LDFLAGS=-nostdlib -Ttext 0x8c201000 -N -x -Bstatic -e start

INSTALL_STRIP=

BOOTXX_SECTORS?= 15
BOOTXX_MAXSIZE?= (( ${BOOTXX_SECTORS} * 512 ))

CPPFLAGS+=-D_STANDALONE
CPPFLAGS+=-nostdinc -I${.OBJDIR} -I${.CURDIR}/.. -I${.CURDIR}/../../../..
CPPFLAGS+=-DLOADADDRESS=0x8ff00000 -DBOOTXX_SECTORS=${BOOTXX_SECTORS}
CFLAGS+=-m4-nofpu

S=	${.CURDIR}/../../../..
SAREL=
.include "${S}/lib/libsa/Makefile.inc"
DPADD+= $(SALIB)
LDADD+= $(SALIB)

KERN_AS?=	library
.include "${S}/lib/libkern/Makefile.inc"
DPADD+= $(KERNLIB)
LDADD+= $(KERNLIB)

LIBGCC!=${CC} -print-libgcc-file-name
LDADD+= $(LIBGCC)

${PROG}: ${OBJS} ${DPADD}
	${LD} -o ${PROG}.sym ${LDFLAGS} \
		-Map ${PROG}.map -cref ${OBJS} ${LDADD}
	${OBJCOPY} -O binary ${PROG}.sym ${PROG}
	@ sz=$$(ls -ln ${PROG} | tr -s ' ' | cut -d' ' -f5); \
	if [ "$$sz" -gt "$$${BOOTXX_MAXSIZE}" ]; then \
		echo "### ${PROG} size $$sz is larger than ${BOOTXX_MAXSIZE}" >&2 \
		rm -f ${PROG}; \
		! :; \
	else \
		: pad to sector boundary; \
		pad=$$(( 512 - ( $$sz & 511 ) )); \
		[ $$pad = 512 ] || \
		    dd if=/dev/zero bs=1 count=$$pad >>${PROG} 2>/dev/null; \
		echo "${PROG} size $$sz, $$((${BOOTXX_MAXSIZE} - $$sz)) free"; \
	fi
.else
NOPROG=
.endif

.include <bsd.prog.mk>
