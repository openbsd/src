#	$OpenBSD: Makefile,v 1.4 2012/08/21 14:46:18 pascal Exp $
#

PROG=	mbr
SRCS=	mbr.S
AFLAGS+=-m32 -I${.CURDIR} -I${.CURDIR}/../../.. #-Wa,-a
AFLAGS+=${NOPIE_FLAGS}
LD=ld
LDFLAGS=-melf_i386 -nostdlib -Ttext 0 -x -N -s -Bstatic -e start
LDFLAGS+=-L/usr/libdata 

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
		objcopy -O binary ${PROG} ${.OBJDIR}/.tmp;\
		mv -f ${.OBJDIR}/.tmp ${.OBJDIR}/${PROG}; \
		ls -l ${.OBJDIR}/${PROG}; \
	fi

.include <bsd.prog.mk>
