#	$OpenBSD: Makefile,v 1.17 2013/01/18 03:45:23 jsing Exp $
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

${PROG}: $(OBJS)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS)
	@size $(PROG)
	@if [ -x ${.OBJDIR}/${PROG} ]; then \
		objcopy -O binary ${PROG} ${.OBJDIR}/.tmp;\
		mv -f ${.OBJDIR}/.tmp ${.OBJDIR}/${PROG}; \
		ls -l ${.OBJDIR}/${PROG}; \
	fi

.include <bsd.prog.mk>
