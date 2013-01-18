#	$OpenBSD: Makefile,v 1.18 2013/01/18 04:10:16 jsing Exp $
#

S=	${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

PROG=	mbr
SRCS=	mbr.S
AFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../.. #-Wa,-a
AFLAGS+=-fno-pie
LD=ld
LDFLAGS=-nostdlib -Ttext 0 -x -N -s -Bstatic -e start -nopie

NOMAN=
#MAN+=	mbr.8

INSTALL_STRIP=

# Uncomment this to make mbr talk to a serial port.
#CPPFLAGS+=-DSERIAL=0

${.CURDIR}/mbr.S: assym.h

${PROG}: $(OBJS)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS)
	@size $(PROG)
	@if [ -x ${.OBJDIR}/${PROG} ]; then \
		objcopy -O binary ${PROG} ${.OBJDIR}/.tmp;\
		mv -f ${.OBJDIR}/.tmp ${.OBJDIR}/${PROG}; \
		ls -l ${.OBJDIR}/${PROG}; \
	fi

.include <bsd.prog.mk>
