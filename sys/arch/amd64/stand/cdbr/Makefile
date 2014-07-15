#	$OpenBSD: Makefile,v 1.9 2014/07/15 22:14:37 deraadt Exp $
#

S=	${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

PROG=	cdbr
SRCS=	cdbr.S
AFLAGS+=-m32
AFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../.. #-Wa,-a
AFLAGS+=-DOSREV=\"${OSREV}\"
AFLAGS+=-DMACH=\"${MACHINE}\" -DMACH_U=\"${MACHINE:U}\"
AFLAGS+=-fno-pie
LD=ld
ORG=	0x0000
LDFLAGS=-nostdlib -Ttext ${ORG} -x -N -s -Bstatic -e start -nopie
LDFLAGS+=-melf_i386 -L/usr/libdata
INSTALL_STRIP=
BINMODE=644

NOMAN=
#MAN+=	cdbr.8

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@size $(PROG)
	@if [ -x ${.OBJDIR}/${PROG} ]; then \
		cp ${PROG} ${PROG}.orig; \
		objcopy -O binary ${PROG} ${.OBJDIR}/.tmp;\
		mv -f ${.OBJDIR}/.tmp ${.OBJDIR}/${PROG}; \
		ls -l ${.OBJDIR}/${PROG}; \
	fi

.include <bsd.prog.mk>
