#	$OpenBSD: Makefile,v 1.2 1997/04/07 09:00:10 deraadt Exp $
#

PROG=	mbr
SRCS=	mbr.S
AFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../.. #-Wa,-a
LDFLAGS=-s -Wl,-T0,-N,-x -nostdlib
MAN+=	mbr.8
STRIP=

# Uncomment this to make mbr talk to a serial port.
#CPPFLAGS+=-DSERIAL=0

all:
	@if [ -x ${.OBJDIR}/${PROG} ]; then \
		dd if=${.OBJDIR}/${PROG} of=${.OBJDIR}/.tmp ibs=32 skip=1; \
		mv -f ${.OBJDIR}/.tmp ${.OBJDIR}/${PROG}; \
		ls -l ${.OBJDIR}/${PROG}; \
	fi

.include <bsd.prog.mk>
