#	$OpenBSD: Makefile,v 1.7 1998/05/25 19:06:41 mickey Exp $
#

PROG=	mbr
SRCS=	mbr.S
AFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../.. #-Wa,-a
LD=ld
LDFLAGS=-nostdlib -Ttext 0 -x -N -s -Bstatic
MAN+=	mbr.8
INSTALL_STRIP=
SADIR=${.CURDIR}/..
CLEANFILES=	i386 machine

# Uncomment this to make mbr talk to a serial port.
#CPPFLAGS+=-DSERIAL=0

all:	machine-links

machine-links:
	@rm -f machine i386
	@ln -fs ${.CURDIR}/../.. i386
	@ln -fs ${.CURDIR}/../../include machine

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@size $(PROG)
	@if [ -x ${.OBJDIR}/${PROG} ]; then \
		dd if=${.OBJDIR}/${PROG} of=${.OBJDIR}/.tmp ibs=32 skip=1 2>/dev/null; \
		mv -f ${.OBJDIR}/.tmp ${.OBJDIR}/${PROG}; \
		ls -l ${.OBJDIR}/${PROG}; \
	fi

.include <bsd.prog.mk>
