#	$OpenBSD: Makefile,v 1.3 2004/10/09 21:15:21 mickey Exp $
#

PROG=	cdbr
SRCS=	cdbr.S
AFLAGS+=-m32 -I${.CURDIR} -I${.CURDIR}/../../.. #-Wa,-a
AFLAGS+=-DOSREV=\"${OSREV}\"
AFLAGS+=-DMACH=\"${MACHINE}\" -DMACH_U=\"${MACHINE:U}\"
LD=ld
ORG=	0x0000
LDFLAGS=-melf_i386 -nostdlib -Ttext ${ORG} -x -N -s -Bstatic -e start
LDFLAGS+=-L/usr/libdata 

NOMAN=
#MAN+=	cdbr.8

INSTALL_STRIP=
SADIR=${.CURDIR}/..
S=	${.CURDIR}/../../../..

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@size $(PROG)
	@if [ -x ${.OBJDIR}/${PROG} ]; then \
		objcopy -O binary ${PROG} ${.OBJDIR}/.tmp;\
		mv -f ${.OBJDIR}/.tmp ${.OBJDIR}/${PROG}; \
		ls -l ${.OBJDIR}/${PROG}; \
	fi

.include <bsd.prog.mk>
