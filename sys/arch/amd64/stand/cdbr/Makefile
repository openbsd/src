#	$OpenBSD: Makefile,v 1.4 2012/08/21 14:46:18 pascal Exp $
#

PROG=	cdbr
SRCS=	cdbr.S
AFLAGS+=-m32 -I${.CURDIR} -I${.CURDIR}/../../.. #-Wa,-a
AFLAGS+=-DOSREV=\"${OSREV}\"
AFLAGS+=-DMACH=\"${MACHINE}\" -DMACH_U=\"${MACHINE:U}\"
AFLAGS+=${NOPIE_FLAGS}
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
