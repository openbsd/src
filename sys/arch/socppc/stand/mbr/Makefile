#	$OpenBSD: Makefile,v 1.4 2016/07/30 03:25:49 guenther Exp $

.include <bsd.own.mk>

NOMAN=
#MAN=	mbr.8

.if ${MACHINE} == "socppc"
PROG=	mbr
SRCS=	mbr.S
AFLAGS+=-I${.OBJDIR} -I${.CURDIR}/../../.. #-Wa,a
LDFLAGS+=-N -e start -Ttext 0 -nopie -znorelro
OBJCOPY?=objcopy

INSTALL_STRIP=
S=	${.CURDIR}/../../../..

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@size $(PROG)
	@if [ -x ${.OBJDIR}/${PROG} ]; then \
		${OBJCOPY} -O binary ${PROG} ${.OBJDIR}/.tmp;\
		mv -f ${.OBJDIR}/.tmp ${.OBJDIR}/${PROG}; \
		ls -l ${.OBJDIR}/${PROG}; \
	fi

.else
NOPROG=
.endif

.include <bsd.prog.mk>
