#	$OpenBSD: Makefile,v 1.1 2006/10/06 21:48:50 mickey Exp $

PROG=	bootxx
SRCS=	pbr.S bootxx.S boot1.c
MAN=	bootxx.8
LDFLAGS=-nostdlib -Ttext 0x8c201000 -N -x -Bstatic -e start

INSTALL_STRIP=

BOOTXX_SECTORS?= 15
BOOTXX_MAXSIZE?= (( ${BOOTXX_SECTORS} * 512 ))

CPPFLAGS+=-D_STANDALONE -DNO_READDIR
CPPFLAGS+=-nostdinc -I${.OBJDIR} -I${.CURDIR}/.. -I${.CURDIR}/../../../..
CPPFLAGS+=-DLOADADDRESS=0x8ff00000 -DBOOTXX_SECTORS=${BOOTXX_SECTORS}

S=	${.CURDIR}/../../../..
SAREL=
.include "${S}/lib/libsa/Makefile.inc"
DPADD+= $(SALIB)
LDADD+= $(SALIB)

KERN_AS?=	library
.include "${S}/lib/libkern/Makefile.inc"
DPADD+= $(KERNLIB)
LDADD+= $(KERNLIB)

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

.include <bsd.prog.mk>
