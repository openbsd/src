# $OpenBSD: Makefile,v 1.7 2013/02/02 13:32:05 miod Exp $
# $NetBSD: Makefile,v 1.3 1996/12/31 22:44:13 veego Exp $
#
.if ${MACHINE_ARCH} == "m68k"

AS?=	as
AS+=	-m68060
LD=	ld

.SUFFIXES:	.o .S .sa .defs .h

.sa.S:
	sh ${.CURDIR}/asm2gas ${.IMPSRC} >${.TARGET}

060sp.o: netbsd.S inetbsd.S fnetbsd.S isp.S fpsp.S copyright.S
	$(CC) ${AFLAGS} -m68060 ${CPPFLAGS} -I${.CURDIR} -I. \
	    -c -o ${.TARGET} ${.CURDIR}/netbsd.S

clean cleandir:
	rm -f 060sp.o isp.S fpsp.S

_SUBDIRUSE:

.else
clean cleandir:

.endif

all:

depend:

install:

includes:

.include <bsd.obj.mk>
.include <bsd.own.mk>
