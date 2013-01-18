#	$OpenBSD: Makefile,v 1.5 2013/01/18 03:45:23 jsing Exp $
#

S=	${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

PROG=	cdbr
SRCS=	cdbr.S
AFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../.. #-Wa,-a
AFLAGS+=-DOSREV=\"${OSREV}\"
AFLAGS+=-DMACH=\"${MACHINE}\" -DMACH_U=\"${MACHINE:U}\"
AFLAGS+=-fno-pie
LD=ld
ORG=	0x0000
LDFLAGS=-nostdlib -Ttext ${ORG} -x -N -s -Bstatic -e start -nopie

NOMAN=
#MAN+=	cdbr.8

INSTALL_STRIP=

${PROG}: $(OBJS)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS)
	@size $(PROG)
	@if [ -x ${.OBJDIR}/${PROG} ]; then \
		objcopy -O binary ${PROG} ${.OBJDIR}/.tmp;\
		mv -f ${.OBJDIR}/.tmp ${.OBJDIR}/${PROG}; \
		ls -l ${.OBJDIR}/${PROG}; \
	fi

.include <bsd.prog.mk>
