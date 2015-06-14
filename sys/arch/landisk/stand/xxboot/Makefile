#	$OpenBSD: Makefile,v 1.8 2015/06/14 19:08:59 miod Exp $

MAN=	xxboot.8

.if ${MACHINE} == "landisk"
PROG=	xxboot
LINKS=	${BINDIR}/xxboot ${BINDIR}/sdboot
LINKS+=	${BINDIR}/xxboot ${BINDIR}/wdboot
SRCS=	pbr.S xxboot.S boot1.c
LDFLAGS=-nostdlib -Ttext 0x8c201000 -N -x -Bstatic -e start -nopie

INSTALL_STRIP=

XXBOOT_SECTORS?= 15
XXBOOT_MAXSIZE?= (( ${XXBOOT_SECTORS} * 512 ))

CPPFLAGS+=-D_STANDALONE
CPPFLAGS+=-nostdinc -I${.OBJDIR} -I${.CURDIR}/.. -I${.CURDIR}/../../../..
CPPFLAGS+=-DLOADADDRESS=0x8ff00000 -DXXBOOT_SECTORS=${XXBOOT_SECTORS}
CFLAGS+=-m4-nofpu -fno-pie
AFLAGS+= -fno-pie

OBJCOPY?=objcopy

S=	${.CURDIR}/../../../..
SAREL=
.include "${S}/lib/libsa/Makefile.inc"
DPADD+= $(SALIB)
LDADD+= $(SALIB)

.PATH:	${S}/lib/libkern/arch/sh ${S}/lib/libkern
SRCS+=	strlen.c
SRCS+=	ashrdi3.c sdivsi3.S udivsi3.S

${PROG}: ${OBJS} ${DPADD}
	${LD} -o ${PROG}.sym ${LDFLAGS} \
		-Map ${PROG}.map -cref ${OBJS} ${LDADD}
	${OBJCOPY} -O binary ${PROG}.sym ${PROG}
	@ sz=$$(ls -ln ${PROG} | tr -s ' ' | cut -d' ' -f5); \
	if [ "$$sz" -gt "$$${XXBOOT_MAXSIZE}" ]; then \
		echo "### ${PROG} size $$sz is larger than ${XXBOOT_MAXSIZE}" >&2 \
		rm -f ${PROG}; \
		! :; \
	else \
		: pad to sector boundary; \
		pad=$$(( 512 - ( $$sz & 511 ) )); \
		[ $$pad = 512 ] || \
		    dd if=/dev/zero bs=1 count=$$pad >>${PROG} 2>/dev/null; \
		echo "${PROG} size $$sz, $$((${XXBOOT_MAXSIZE} - $$sz)) free"; \
	fi
.else
NOPROG=
.endif


.include <bsd.prog.mk>
