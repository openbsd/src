#	$OpenBSD: Makefile,v 1.13 2000/06/08 00:56:08 mickey Exp $
#

PROG=	mbr
SRCS=	mbr.S
AFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../.. #-Wa,-a
LD=ld
LDFLAGS=-nostdlib -Ttext 0 -x -N -s -Bstatic

NOMAN=
#MAN+=	mbr.8

INSTALL_STRIP=
SADIR=${.CURDIR}/..
S=	${.CURDIR}/../../../..

# Uncomment this to make mbr talk to a serial port.
#CPPFLAGS+=-DSERIAL=0

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@size $(PROG)
	@if [ -x ${.OBJDIR}/${PROG} ]; then \
		dd if=${.OBJDIR}/${PROG} of=${.OBJDIR}/.tmp ibs=32 skip=1 2>/dev/null; \
		mv -f ${.OBJDIR}/.tmp ${.OBJDIR}/${PROG}; \
		ls -l ${.OBJDIR}/${PROG}; \
	fi

.include <bsd.prog.mk>
