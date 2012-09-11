#	$OpenBSD: Makefile,v 1.3 2012/09/11 15:59:01 kettenis Exp $

.include <bsd.own.mk>

NOMAN=
#MAN=	mbr.8

.if ${MACHINE} == "socppc"
PROG=	mbr
SRCS=	mbr.S
AFLAGS+=-I${.OBJDIR} -I${.CURDIR}/../../.. #-Wa,a
LDFLAGS+=-N -e start -Ttext 0 -nopie
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
