#	$OpenBSD: Makefile,v 1.1 2004/08/21 18:48:37 tom Exp $
#

PROG=	cdbr
SRCS=	cdbr.S
AFLAGS+=-m32 -I${.CURDIR} -I${.CURDIR}/../../.. #-Wa,-a
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
	#@size $(PROG)
	@if [ -x ${.OBJDIR}/${PROG} ]; then \
		objcopy -O binary ${PROG} ${.OBJDIR}/.tmp;\
		mv -f ${.OBJDIR}/.tmp ${.OBJDIR}/${PROG}; \
		ls -l ${.OBJDIR}/${PROG}; \
	fi

.include <bsd.prog.mk>
