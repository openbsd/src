# $OpenBSD: Makefile,v 1.3 1996/10/04 14:58:34 niklas Exp $
# $NetBSD: Makefile,v 1.2 1996/05/20 13:31:40 is Exp $
#
AS		= as -m68060
LD		= ld

.SUFFIXES:	.o .S .sa .defs .h

.sa.S:
	sh ${.CURDIR}/asm2gas ${.IMPSRC} >${.TARGET}

060sp.o: netbsd.S inetbsd.S fnetbsd.S isp.S fpsp.S copyright.S
	$(AS) -I${.CURDIR} -o ${.TARGET} ${.CURDIR}/netbsd.S

clean cleandir:
	rm -f 060sp.o isp.S fpsp.S

_SUBDIRUSE:

all:

depend:

install:

includes:

.include <bsd.obj.mk>
.include <bsd.own.mk>
