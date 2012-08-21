#	$OpenBSD: Makefile,v 1.3 2012/08/21 14:46:19 pascal Exp $

.include <bsd.own.mk>

MAN=	mbr.8

.if ${MACHINE} == "landisk"
PROG=	mbr
SRCS=	mbr.S
AFLAGS+=-nostdinc -I${.OBJDIR} -I${.CURDIR}/.. -I${.CURDIR}/../../.. #-Wa,a
LDFLAGS+=-N -e start -Ttext 0x8c200200
NOPIE=
OBJCOPY?=objcopy

INSTALL_STRIP=
S=	${.CURDIR}/../../../..

# Uncomment this to make mbr talk to a serial port.
#CPPFLAGS+=-DSERIAL=0

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
