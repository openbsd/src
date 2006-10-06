#	$OpenBSD: Makefile,v 1.1 2006/10/06 21:48:50 mickey Exp $

PROG=	mbr
SRCS=	mbr.S
MAN=	mbr.8
AFLAGS+=-nostdinc -I${.OBJDIR} -I${.CURDIR}/.. -I${.CURDIR}/../../.. #-Wa,a
LDFLAGS+=-N -e start -Ttext 0x8c200200
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

.include <bsd.prog.mk>
